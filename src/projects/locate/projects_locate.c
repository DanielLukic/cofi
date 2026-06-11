#include "projects/locate/projects_locate.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <string.h>

#include "config/config.h"
#include "core/app/app_data.h"
#include "core/log/log.h"
#include "matching/fzf_algo.h"

#define PROJECTS_LOCATE_MAX_RESULTS 50
#define PROJECTS_LOCATE_MAX_ACCEPTED (PROJECTS_LOCATE_MAX_RESULTS * 4)
#define PROJECTS_LOCATE_DEFAULT_TIMEOUT_MS 1500

typedef struct {
    char *path;
    score_t total_score;
    score_t fuzzy_score;
    int path_length;
    int order;
} LocateHit;

typedef struct ProjectsLocateOp {
    int refcount;
    AppData *app;
    guint generation;
    char *query;
    GCancellable *cancel;
    GSubprocess *process;
    GDataInputStream *stdout_stream;
    GPtrArray *accepted_paths;
    gchar **exclude_patterns;
    gchar **search_roots;
    guint timeout_source_id;
    gboolean cap_reached;
    gboolean read_done;
    gboolean wait_done;
    gboolean abandoned;
} ProjectsLocateOp;

static ProjectsLocateResultsCallback s_callback = NULL;
static ProjectsLocateOp *s_pending_op = NULL;
static gboolean s_locate_tool_checked = FALSE;
static gboolean s_locate_available = FALSE;
static gboolean s_locate_unavailable_logged = FALSE;
static gchar *s_locate_tool_path = NULL;

#ifdef COFI_TESTING
static ProjectsLocateSpawnImpl s_spawn_impl = NULL;
static gchar *s_tool_path_override = NULL;
#endif

static int current_locate_generation(const AppData *app) {
    return app ? (int)app->projects_mode.locate_generation : -1;
}

static void projects_locate_op_unref(ProjectsLocateOp *ctx);

static ProjectsLocateOp *projects_locate_op_ref(ProjectsLocateOp *ctx) {
    if (ctx) ctx->refcount++;
    return ctx;
}

static void free_results_array(ProjectsLocateResult *results, int count) {
    if (!results) return;
    for (int i = 0; i < count; i++) {
        g_free(results[i].path);
    }
    g_free(results);
}

static void free_hit(gpointer data) {
    LocateHit *hit = data;
    if (!hit) return;
    g_free(hit->path);
    g_free(hit);
}

static void projects_locate_op_unref(ProjectsLocateOp *ctx) {
    if (!ctx) return;
    ctx->refcount--;
    if (ctx->refcount > 0) return;

    if (ctx->timeout_source_id != 0) {
        g_source_remove(ctx->timeout_source_id);
    }
    g_clear_object(&ctx->stdout_stream);
    g_clear_object(&ctx->process);
    g_clear_object(&ctx->cancel);
    g_clear_pointer(&ctx->query, g_free);
    g_clear_pointer(&ctx->accepted_paths, g_ptr_array_unref);
    g_strfreev(ctx->exclude_patterns);
    g_strfreev(ctx->search_roots);
    g_free(ctx);
}

static void clear_pending_if_current(ProjectsLocateOp *ctx) {
    if (s_pending_op == ctx) {
        s_pending_op = NULL;
        projects_locate_op_unref(ctx);
    }
}

static void abandon_operation(ProjectsLocateOp *ctx, gboolean force_exit) {
    if (!ctx || ctx->abandoned) return;
    ctx->abandoned = TRUE;
    if (ctx->cancel) {
        g_cancellable_cancel(ctx->cancel);
    }
    if (force_exit && ctx->process) {
        g_subprocess_force_exit(ctx->process);
    }
    clear_pending_if_current(ctx);
}

static gchar *expand_home_pattern(const char *pattern) {
    if (!pattern) return g_strdup("");
    if (pattern[0] != '~') return g_strdup(pattern);
    const char *home = g_get_home_dir();
    if (!home || home[0] == '\0') return g_strdup(pattern);
    if (pattern[1] == '\0') return g_strdup(home);
    if (pattern[1] == '/') return g_strconcat(home, pattern + 1, NULL);
    return g_strdup(pattern);
}

static gchar *normalize_search_root(const char *root) {
    gchar *expanded = expand_home_pattern(root);
    if (!expanded) return g_strdup("");
    g_strstrip(expanded);
    size_t len = strlen(expanded);
    while (len > 1 && expanded[len - 1] == '/') {
        expanded[len - 1] = '\0';
        len--;
    }
    return expanded;
}

static gchar **build_exclude_patterns(const CofiConfig *config) {
    const char *raw = config && config->projects_locate_excludes[0]
        ? config->projects_locate_excludes
        : "";
    gchar **parts = g_strsplit(raw, ",", -1);
    if (!parts) return NULL;

    GPtrArray *patterns = g_ptr_array_new_with_free_func(g_free);
    for (int i = 0; parts[i]; i++) {
        char *trimmed = g_strstrip(parts[i]);
        if (!trimmed || trimmed[0] == '\0') continue;
        g_ptr_array_add(patterns, expand_home_pattern(trimmed));
    }
    g_strfreev(parts);
    g_ptr_array_add(patterns, NULL);
    return (gchar **)g_ptr_array_free(patterns, FALSE);
}

static gchar **build_search_roots(const CofiConfig *config) {
    const char *raw = config ? config->projects_locate_search_roots : "";
    gchar **parts = g_strsplit(raw ? raw : "", ",", -1);
    if (!parts) return NULL;

    GPtrArray *roots = g_ptr_array_new_with_free_func(g_free);
    for (int i = 0; parts[i]; i++) {
        char *trimmed = g_strstrip(parts[i]);
        if (!trimmed || trimmed[0] == '\0') continue;
        g_ptr_array_add(roots, normalize_search_root(trimmed));
    }
    g_strfreev(parts);
    g_ptr_array_add(roots, NULL);
    return (gchar **)g_ptr_array_free(roots, FALSE);
}

static gboolean path_matches_search_roots(gchar **roots, const char *path) {
    if (!path || path[0] == '\0') return FALSE;
    if (!roots || !roots[0]) return TRUE;
    for (int i = 0; roots[i]; i++) {
        if (g_str_has_prefix(path, roots[i])) {
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean path_excluded_by_patterns(gchar **patterns, const char *path) {
    if (!patterns || !path || path[0] == '\0') return FALSE;
    for (int i = 0; patterns[i]; i++) {
        if (g_pattern_match_simple(patterns[i], path)) {
            return TRUE;
        }
    }
    return FALSE;
}

static void append_escaped_glob_char(GString *glob, char c) {
    if (c == '*' || c == '?' || c == '[' || c == ']' || c == '\\') {
        g_string_append_c(glob, '\\');
    }
    g_string_append_c(glob, c);
}

static gchar *build_glob_query(const char *query) {
    GString *glob = g_string_new("");
    for (const char *p = query ? query : ""; *p; p++) {
        append_escaped_glob_char(glob, *p);
        g_string_append_c(glob, '*');
    }
    return g_string_free(glob, FALSE);
}

static void resolve_locate_tool_once(void) {
#ifdef COFI_TESTING
    if (s_tool_path_override) {
        s_locate_tool_checked = TRUE;
        s_locate_available = TRUE;
        g_free(s_locate_tool_path);
        s_locate_tool_path = g_strdup(s_tool_path_override);
        return;
    }
#endif
    if (s_locate_tool_checked) return;
    s_locate_tool_checked = TRUE;

    s_locate_tool_path = g_find_program_in_path("plocate");
    if (!s_locate_tool_path) {
        s_locate_tool_path = g_find_program_in_path("locate");
    }
    s_locate_available = s_locate_tool_path != NULL;
    if (!s_locate_available && !s_locate_unavailable_logged) {
        log_info("projects locate fallback disabled: neither plocate nor locate found in PATH");
        s_locate_unavailable_logged = TRUE;
    }
}

static GSubprocess *spawn_locate_process(const char *tool_path,
                                         const char *glob_query,
                                         GError **error) {
#ifdef COFI_TESTING
    if (s_spawn_impl) {
        return s_spawn_impl(tool_path, glob_query, error);
    }
#endif
    return g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE,
                            error,
                            tool_path,
                            "-i",
                            "-b",
                            glob_query,
                            NULL);
}

static gboolean accepted_path_exists(ProjectsLocateOp *ctx, const char *path) {
    if (!ctx || !path) return FALSE;
    for (guint i = 0; i < ctx->accepted_paths->len; i++) {
        const char *existing = g_ptr_array_index(ctx->accepted_paths, i);
        if (existing && strcmp(existing, path) == 0) return TRUE;
    }
    return FALSE;
}

static gboolean accept_path(ProjectsLocateOp *ctx, const char *path) {
    if (!ctx || !path || path[0] == '\0') return FALSE;
    if (!path_matches_search_roots(ctx->search_roots, path)) return FALSE;
    if (path_excluded_by_patterns(ctx->exclude_patterns, path)) return FALSE;
    if (!g_file_test(path, G_FILE_TEST_IS_DIR)) return FALSE;
    if (accepted_path_exists(ctx, path)) return FALSE;

    g_ptr_array_add(ctx->accepted_paths, g_strdup(path));
    if (ctx->accepted_paths->len >= PROJECTS_LOCATE_MAX_ACCEPTED && ctx->process) {
        ctx->cap_reached = TRUE;
        g_subprocess_force_exit(ctx->process);
    }
    return TRUE;
}

static score_t length_bonus_for_path(const char *path) {
    int path_length = path ? (int)strlen(path) : 0;
    int bonus = (300 - path_length) / 10;
    return bonus > 0 ? bonus : 0;
}

static int compare_hits(const void *a, const void *b) {
    const LocateHit *ha = *(const LocateHit * const *)a;
    const LocateHit *hb = *(const LocateHit * const *)b;
    if (ha->total_score != hb->total_score) {
        return hb->total_score - ha->total_score;
    }
    if (ha->path_length != hb->path_length) {
        return ha->path_length - hb->path_length;
    }
    int path_cmp = strcmp(ha->path, hb->path);
    if (path_cmp != 0) return path_cmp;
    return ha->order - hb->order;
}

static void deliver_results(ProjectsLocateOp *ctx) {
    if (!ctx || !s_callback) return;

    GPtrArray *hits = g_ptr_array_new_with_free_func(free_hit);
    for (guint i = 0; i < ctx->accepted_paths->len; i++) {
        const char *path = g_ptr_array_index(ctx->accepted_paths, i);
        if (!path) continue;
        gchar *basename = g_path_get_basename(path);
        if (!basename || basename[0] == '\0') {
            g_free(basename);
            continue;
        }
        LocateHit *hit = g_new0(LocateHit, 1);
        hit->path = g_strdup(path);
        hit->fuzzy_score = fzf_fuzzy_match(ctx->query, basename);
        hit->total_score = hit->fuzzy_score + length_bonus_for_path(path);
        hit->path_length = (int)strlen(path);
        hit->order = (int)i;
        g_ptr_array_add(hits, hit);
        g_free(basename);
    }
    if (hits->len > 1) {
        qsort(hits->pdata, hits->len, sizeof(gpointer), compare_hits);
    }

    int count = (int)hits->len;
    if (count > PROJECTS_LOCATE_MAX_RESULTS) {
        count = PROJECTS_LOCATE_MAX_RESULTS;
    }
    ProjectsLocateResult *results = count > 0 ? g_new0(ProjectsLocateResult, count) : NULL;
    for (int i = 0; i < count; i++) {
        LocateHit *hit = g_ptr_array_index(hits, (guint)i);
        results[i].path = g_strdup(hit->path);
    }
    s_callback(ctx->app, ctx->query, ctx->generation, results, count);
    free_results_array(results, count);
    g_ptr_array_unref(hits);
}

static void maybe_finish_operation(ProjectsLocateOp *ctx) {
    if (!ctx || !ctx->read_done || !ctx->wait_done) return;
    if (ctx->abandoned || current_locate_generation(ctx->app) != (int)ctx->generation) {
        return;
    }

    int exit_status = -1;
    if (ctx->process && g_subprocess_get_if_exited(ctx->process)) {
        exit_status = g_subprocess_get_exit_status(ctx->process);
    }
    if (!ctx->cap_reached && exit_status > 1 && ctx->accepted_paths->len == 0) {
        log_info("projects locate command exited with status %d for query '%s'",
                 exit_status, ctx->query ? ctx->query : "");
    }
    clear_pending_if_current(ctx);
    deliver_results(ctx);
}

static void read_next_line(ProjectsLocateOp *ctx);

static void on_stdout_line(GObject *source, GAsyncResult *result, gpointer user_data) {
    ProjectsLocateOp *ctx = user_data;
    if (!ctx || ctx->abandoned || current_locate_generation(ctx->app) != (int)ctx->generation) {
        projects_locate_op_unref(ctx);
        return;
    }

    GError *error = NULL;
    gsize length = 0;
    char *line = g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(source),
                                                      result,
                                                      &length,
                                                      &error);
    if (error) {
        g_clear_error(&error);
        ctx->read_done = TRUE;
        maybe_finish_operation(ctx);
        projects_locate_op_unref(ctx);
        return;
    }
    if (!line) {
        ctx->read_done = TRUE;
        maybe_finish_operation(ctx);
        projects_locate_op_unref(ctx);
        return;
    }

    if (length > 0) {
        g_strstrip(line);
        accept_path(ctx, line);
    }
    g_free(line);

    if (!ctx->cap_reached) {
        read_next_line(ctx);
    } else {
        ctx->read_done = TRUE;
        maybe_finish_operation(ctx);
    }
    projects_locate_op_unref(ctx);
}

static void read_next_line(ProjectsLocateOp *ctx) {
    if (!ctx || !ctx->stdout_stream || ctx->abandoned) return;
    g_data_input_stream_read_line_async(ctx->stdout_stream,
                                        G_PRIORITY_DEFAULT,
                                        ctx->cancel,
                                        on_stdout_line,
                                        projects_locate_op_ref(ctx));
}

static void on_process_waited(GObject *source, GAsyncResult *result, gpointer user_data) {
    ProjectsLocateOp *ctx = user_data;
    if (!ctx || ctx->abandoned || current_locate_generation(ctx->app) != (int)ctx->generation) {
        projects_locate_op_unref(ctx);
        return;
    }

    GError *error = NULL;
    if (!g_subprocess_wait_finish(G_SUBPROCESS(source), result, &error)) {
        g_clear_error(&error);
    }
    ctx->wait_done = TRUE;
    maybe_finish_operation(ctx);
    projects_locate_op_unref(ctx);
}

static gboolean on_timeout(gpointer user_data) {
    ProjectsLocateOp *ctx = user_data;
    if (!ctx) return G_SOURCE_REMOVE;
    ctx->timeout_source_id = 0;
    abandon_operation(ctx, TRUE);
    projects_locate_op_unref(ctx);
    return G_SOURCE_REMOVE;
}

void projects_locate_set_results_callback(ProjectsLocateResultsCallback callback) {
    s_callback = callback;
}

void projects_locate_cancel_pending(AppData *app) {
    if (!s_pending_op || s_pending_op->app != app) return;
    abandon_operation(s_pending_op, TRUE);
}

void projects_locate_search_async(AppData *app, const char *query, guint generation) {
    if (!app || !query || query[0] == '\0' || !app->config.projects_locate_enabled) return;

    resolve_locate_tool_once();
    if (!s_locate_available || !s_locate_tool_path) return;

    if (s_pending_op) {
        projects_locate_cancel_pending(s_pending_op->app);
    }

    ProjectsLocateOp *ctx = g_new0(ProjectsLocateOp, 1);
    ctx->refcount = 1;
    ctx->app = app;
    ctx->generation = generation;
    ctx->query = g_strdup(query);
    ctx->cancel = g_cancellable_new();
    ctx->accepted_paths = g_ptr_array_new_with_free_func(g_free);
    ctx->search_roots = build_search_roots(&app->config);
    ctx->exclude_patterns = build_exclude_patterns(&app->config);

    gchar *glob_query = build_glob_query(query);
    GError *error = NULL;
    ctx->process = spawn_locate_process(s_locate_tool_path, glob_query, &error);
    g_free(glob_query);
    if (!ctx->process) {
        log_info("projects locate command failed to start: %s",
                 error ? error->message : "unknown error");
        g_clear_error(&error);
        projects_locate_op_unref(ctx);
        return;
    }

    GInputStream *stdout_pipe = g_subprocess_get_stdout_pipe(ctx->process);
    ctx->stdout_stream = g_data_input_stream_new(stdout_pipe);
    g_data_input_stream_set_newline_type(ctx->stdout_stream, G_DATA_STREAM_NEWLINE_TYPE_LF);

    s_pending_op = ctx;
    int timeout_ms = app->config.projects_locate_timeout_ms > 0
        ? app->config.projects_locate_timeout_ms
        : PROJECTS_LOCATE_DEFAULT_TIMEOUT_MS;
    ctx->timeout_source_id = g_timeout_add((guint)timeout_ms, on_timeout, projects_locate_op_ref(ctx));

    g_subprocess_wait_async(ctx->process,
                            ctx->cancel,
                            on_process_waited,
                            projects_locate_op_ref(ctx));
    read_next_line(ctx);
}

#ifdef COFI_TESTING
gchar *projects_locate_glob_for_test(const char *query) {
    return build_glob_query(query);
}

gboolean projects_locate_path_excluded_for_test(const CofiConfig *config, const char *path) {
    gchar **patterns = build_exclude_patterns(config);
    gboolean excluded = path_excluded_by_patterns(patterns, path);
    g_strfreev(patterns);
    return excluded;
}

void projects_locate_set_spawn_impl_for_test(ProjectsLocateSpawnImpl impl) {
    s_spawn_impl = impl;
}

void projects_locate_set_tool_path_for_test(const char *tool_path) {
    g_free(s_tool_path_override);
    s_tool_path_override = tool_path ? g_strdup(tool_path) : NULL;
    s_locate_tool_checked = FALSE;
    s_locate_available = FALSE;
    g_clear_pointer(&s_locate_tool_path, g_free);
}

gboolean projects_locate_has_pending_for_test(void) {
    return s_pending_op != NULL;
}

void projects_locate_reset_for_test(void) {
    if (s_pending_op) {
        abandon_operation(s_pending_op, TRUE);
    }
    s_callback = NULL;
    s_locate_tool_checked = FALSE;
    s_locate_available = FALSE;
    s_locate_unavailable_logged = FALSE;
    g_clear_pointer(&s_locate_tool_path, g_free);
    g_clear_pointer(&s_tool_path_override, g_free);
    s_spawn_impl = NULL;
}
#endif
