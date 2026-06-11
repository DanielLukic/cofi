#include "files/files_search.h"

#include <gio/gio.h>
#include <string.h>

#include "config/config.h"
#include "core/app/app_data.h"
#include "core/log/log.h"
#include "core/selection/selection.h"
#include "matching/fzf_algo.h"
#include "ui/display.h"

#define FILES_TIMEOUT_MS 5000
#define FILES_READ_CHUNK 8192

typedef struct {
    int index;
    score_t score;
    int path_length;
} FilesHit;

typedef struct FilesSearchContext {
    int refcount;
    AppData *app;
    guint generation;
    GCancellable *cancel;
    GSubprocess *process;
    GInputStream *stdout_pipe;
    GByteArray *buffer;
    GPtrArray *paths;
    gchar **exclude_patterns;
    guint timeout_source_id;
    gboolean read_done;
    gboolean wait_done;
    gboolean abandoned;
    gboolean cap_reached;
} FilesSearchContext;

static FilesSearchContext *s_pending_search = NULL;
static gboolean s_missing_tool_logged = FALSE;

#ifdef COFI_TESTING
static FilesSearchSpawnImpl s_spawn_impl = NULL;
static gchar *s_tool_path_override = NULL;
#endif

static FilesMode *files_mode(AppData *app) {
    return app ? &app->files_mode : NULL;
}

static FilesSearchContext *files_search_ref(FilesSearchContext *ctx) {
    if (ctx) ctx->refcount++;
    return ctx;
}

static void files_search_unref(FilesSearchContext *ctx) {
    if (!ctx) return;
    ctx->refcount--;
    if (ctx->refcount > 0) return;
    if (ctx->timeout_source_id != 0) {
        g_source_remove(ctx->timeout_source_id);
    }
    g_clear_object(&ctx->stdout_pipe);
    g_clear_object(&ctx->process);
    g_clear_object(&ctx->cancel);
    g_clear_pointer(&ctx->buffer, g_byte_array_unref);
    g_clear_pointer(&ctx->paths, g_ptr_array_unref);
    g_strfreev(ctx->exclude_patterns);
    g_free(ctx);
}

static void clear_pending_if_current(FilesSearchContext *ctx) {
    if (s_pending_search == ctx) {
        s_pending_search = NULL;
        files_search_unref(ctx);
    }
}

static gboolean files_tab_active(AppData *app) {
    FilesMode *mode = files_mode(app);
    return app && mode && mode->tab_mode >= 0 && (int)app->current_tab == mode->tab_mode;
}

static void files_set_status(FilesMode *mode, const char *message) {
    if (!mode) return;
    g_strlcpy(mode->status_message, message ? message : "", sizeof(mode->status_message));
}

static const char *builtin_excludes[] = {
    ".cache",
    ".local",
    ".config",
    "node_modules",
    ".git",
    ".npm",
    ".nvm",
    ".gradle",
    "snap",
    ".var",
    ".mozilla",
    ".ssh",
    ".gnupg",
    ".aws",
    ".docker",
    ".password-store",
    ".claude*",
    ".codex",
    NULL,
};

static gchar **build_excludes(const CofiConfig *config) {
    GPtrArray *patterns = g_ptr_array_new_with_free_func(g_free);
    for (int i = 0; builtin_excludes[i]; i++) {
        g_ptr_array_add(patterns, g_strdup(builtin_excludes[i]));
    }

    if (config && config->files_excludes[0] != '\0') {
        gchar **parts = g_strsplit(config->files_excludes, ",", -1);
        for (int i = 0; parts && parts[i]; i++) {
            char *trimmed = g_strstrip(parts[i]);
            if (trimmed && trimmed[0] != '\0') {
                g_ptr_array_add(patterns, g_strdup(trimmed));
            }
        }
        g_strfreev(parts);
    }

    g_ptr_array_add(patterns, NULL);
    return (gchar **)g_ptr_array_free(patterns, FALSE);
}

static gboolean path_excluded(gchar **patterns, const char *path) {
    if (!patterns || !path) return FALSE;
    gchar **parts = g_strsplit(path, "/", -1);
    for (int i = 0; patterns[i]; i++) {
        for (int j = 0; parts && parts[j]; j++) {
            if (parts[j][0] == '\0') continue;
            if (g_pattern_match_simple(patterns[i], parts[j])) {
                g_strfreev(parts);
                return TRUE;
            }
        }
    }
    g_strfreev(parts);
    return FALSE;
}

static gchar *resolve_fd_tool(const CofiConfig *config) {
#ifdef COFI_TESTING
    if (s_tool_path_override) {
        return g_strdup(s_tool_path_override);
    }
#endif
    if (config && config->files_fd_path[0] != '\0') {
        if (g_file_test(config->files_fd_path, G_FILE_TEST_IS_REGULAR) &&
            g_file_test(config->files_fd_path, G_FILE_TEST_IS_EXECUTABLE)) {
            return g_strdup(config->files_fd_path);
        }
        log_warn("files: configured fd path is not executable: %s", config->files_fd_path);
        return NULL;
    }

    gchar *resolved = g_find_program_in_path("fd");
    if (!resolved) {
        resolved = g_find_program_in_path("fdfind");
    }
    if (!resolved && !s_missing_tool_logged) {
        log_info("files: fd/fdfind not found on PATH; files tab disabled");
        s_missing_tool_logged = TRUE;
    }
    return resolved;
}

static GSubprocess *spawn_fd(const char *tool_path,
                             const char *home_path,
                             gchar **argv,
                             GError **error) {
#ifdef COFI_TESTING
    if (s_spawn_impl) {
        return s_spawn_impl(tool_path, home_path, argv, error);
    }
#endif
    (void)tool_path;
    (void)home_path;
    return g_subprocess_newv((const gchar *const *)argv,
                             G_SUBPROCESS_FLAGS_STDOUT_PIPE,
                             error);
}

static gchar **build_fd_argv(const CofiConfig *config, const char *tool_path, const char *home_path) {
    GPtrArray *argv = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(argv, g_strdup(tool_path));
    g_ptr_array_add(argv, g_strdup("--type=f"));
    g_ptr_array_add(argv, g_strdup("--hidden"));
    g_ptr_array_add(argv, g_strdup("--color=never"));
    g_ptr_array_add(argv, g_strdup("--print0"));
    for (int i = 0; builtin_excludes[i]; i++) {
        g_ptr_array_add(argv, g_strdup("--exclude"));
        g_ptr_array_add(argv, g_strdup(builtin_excludes[i]));
    }

    if (config && config->files_excludes[0] != '\0') {
        gchar **parts = g_strsplit(config->files_excludes, ",", -1);
        for (int i = 0; parts && parts[i]; i++) {
            char *trimmed = g_strstrip(parts[i]);
            if (!trimmed || trimmed[0] == '\0') continue;
            g_ptr_array_add(argv, g_strdup("--exclude"));
            g_ptr_array_add(argv, g_strdup(trimmed));
        }
        g_strfreev(parts);
    }

    g_ptr_array_add(argv, g_strdup("."));
    g_ptr_array_add(argv, g_strdup(home_path));
    g_ptr_array_add(argv, NULL);
    return (gchar **)g_ptr_array_free(argv, FALSE);
}

static void files_free_paths(GPtrArray **paths_ptr) {
    if (!paths_ptr || !*paths_ptr) return;
    g_ptr_array_unref(*paths_ptr);
    *paths_ptr = NULL;
}

static void files_filter(AppData *app) {
    FilesMode *mode = files_mode(app);
    if (!mode) return;

    mode->filtered_count = 0;
    if (!mode->cache_valid || !mode->paths || mode->query[0] == '\0') {
        return;
    }

    FilesHit hits[FILES_MAX_VISIBLE];
    int hit_count = 0;
    for (guint i = 0; i < mode->paths->len; i++) {
        const char *path = g_ptr_array_index(mode->paths, i);
        if (!path || !fzf_has_match(mode->query, path)) {
            continue;
        }
        FilesHit hit = {
            .index = (int)i,
            .score = fzf_fuzzy_match(mode->query, path),
            .path_length = (int)strlen(path),
        };

        int insert_at = hit_count;
        if (hit_count < FILES_MAX_VISIBLE) {
            hit_count++;
        } else {
            int worst = 0;
            for (int j = 1; j < FILES_MAX_VISIBLE; j++) {
                if (hits[j].score < hits[worst].score ||
                    (hits[j].score == hits[worst].score &&
                     hits[j].path_length > hits[worst].path_length)) {
                    worst = j;
                }
            }
            if (hit.score < hits[worst].score ||
                (hit.score == hits[worst].score &&
                 hit.path_length >= hits[worst].path_length)) {
                continue;
            }
            insert_at = worst;
        }
        hits[insert_at] = hit;
    }

    if (hit_count > 1) {
        for (int i = 0; i < hit_count - 1; i++) {
            for (int j = i + 1; j < hit_count; j++) {
                const char *a = g_ptr_array_index(mode->paths, hits[i].index);
                const char *b = g_ptr_array_index(mode->paths, hits[j].index);
                gboolean swap = hits[j].score > hits[i].score ||
                    (hits[j].score == hits[i].score &&
                     hits[j].path_length < hits[i].path_length) ||
                    (hits[j].score == hits[i].score &&
                     hits[j].path_length == hits[i].path_length &&
                     g_strcmp0(b, a) < 0);
                if (swap) {
                    FilesHit tmp = hits[i];
                    hits[i] = hits[j];
                    hits[j] = tmp;
                }
            }
        }
    }

    mode->filtered_count = hit_count;
    for (int i = 0; i < hit_count; i++) {
        mode->filtered_indices[i] = hits[i].index;
    }
}

static void apply_new_cache(AppData *app, FilesSearchContext *ctx) {
    FilesMode *mode = files_mode(app);
    if (!mode || !ctx) return;

    gboolean preserve = files_tab_active(app);
    if (preserve) {
        preserve_selection(app);
    }

    files_free_paths(&mode->paths);
    mode->paths = ctx->paths;
    ctx->paths = g_ptr_array_new_with_free_func(g_free);
    mode->path_count = mode->paths ? (int)mode->paths->len : 0;
    mode->cache_valid = TRUE;
    mode->loading = FALSE;
    mode->unavailable = FALSE;
    mode->dirty = TRUE;
    files_set_status(mode, "");
    files_filter(app);

    if (preserve) {
        restore_selection(app);
        update_scroll_position(app);
    }
    if (app->window_visible && files_tab_active(app)) {
        mode->dirty = FALSE;
        update_display(app);
    }
}

static void maybe_finish_search(FilesSearchContext *ctx) {
    FilesMode *mode = files_mode(ctx ? ctx->app : NULL);
    if (!ctx || !mode || !ctx->read_done || !ctx->wait_done) return;
    if (ctx->abandoned || ctx->generation != mode->search_generation) {
        return;
    }
    clear_pending_if_current(ctx);
    apply_new_cache(ctx->app, ctx);
}

static void stop_after_cap(FilesSearchContext *ctx) {
    if (!ctx || ctx->cap_reached) return;
    ctx->cap_reached = TRUE;
    log_warn("files: cache capped at %d entries; truncating fd results", FILES_MAX_CACHE);
    if (ctx->process) {
        g_subprocess_force_exit(ctx->process);
    }
}

static void accept_path(FilesSearchContext *ctx, const char *path, gchar **patterns) {
    if (!ctx || !path || path[0] == '\0' || !ctx->paths) return;
    if (path_excluded(patterns, path)) return;
    if ((int)ctx->paths->len >= FILES_MAX_CACHE) {
        stop_after_cap(ctx);
        return;
    }
    g_ptr_array_add(ctx->paths, g_strdup(path));
}

static void process_buffer(FilesSearchContext *ctx, gboolean eof) {
    if (!ctx || !ctx->buffer) return;
    gsize start = 0;
    for (gsize i = 0; i < ctx->buffer->len; i++) {
        if (ctx->buffer->data[i] != '\0') continue;
        gsize len = i - start;
        gchar *path = g_strndup((const gchar *)ctx->buffer->data + start, len);
        accept_path(ctx, path, ctx->exclude_patterns);
        g_free(path);
        start = i + 1;
        if (ctx->cap_reached) break;
    }

    if (start > 0) {
        g_byte_array_remove_range(ctx->buffer, 0, start);
    }
    if (eof && ctx->buffer->len > 0 && !ctx->cap_reached) {
        gchar *path = g_strndup((const gchar *)ctx->buffer->data, ctx->buffer->len);
        accept_path(ctx, path, ctx->exclude_patterns);
        g_free(path);
        g_byte_array_set_size(ctx->buffer, 0);
    }
}

static void read_next_chunk(FilesSearchContext *ctx);

static void on_stdout_chunk(GObject *source, GAsyncResult *result, gpointer user_data) {
    FilesSearchContext *ctx = user_data;
    FilesMode *mode = files_mode(ctx ? ctx->app : NULL);
    if (!ctx || !mode || ctx->abandoned || ctx->generation != mode->search_generation) {
        files_search_unref(ctx);
        return;
    }

    GError *error = NULL;
    GBytes *bytes = g_input_stream_read_bytes_finish(G_INPUT_STREAM(source), result, &error);
    if (error) {
        g_clear_error(&error);
        ctx->read_done = TRUE;
        maybe_finish_search(ctx);
        files_search_unref(ctx);
        return;
    }

    gsize len = 0;
    const guint8 *data = g_bytes_get_data(bytes, &len);
    if (len > 0) {
        g_byte_array_append(ctx->buffer, data, len);
        process_buffer(ctx, FALSE);
    } else {
        process_buffer(ctx, TRUE);
        ctx->read_done = TRUE;
    }
    g_bytes_unref(bytes);

    if (!ctx->read_done && !ctx->cap_reached) {
        read_next_chunk(ctx);
    } else if (ctx->cap_reached) {
        ctx->read_done = TRUE;
        maybe_finish_search(ctx);
    } else {
        maybe_finish_search(ctx);
    }
    files_search_unref(ctx);
}

static void read_next_chunk(FilesSearchContext *ctx) {
    if (!ctx || !ctx->stdout_pipe || ctx->abandoned) return;
    g_input_stream_read_bytes_async(ctx->stdout_pipe,
                                    FILES_READ_CHUNK,
                                    G_PRIORITY_DEFAULT,
                                    ctx->cancel,
                                    on_stdout_chunk,
                                    files_search_ref(ctx));
}

static void on_process_waited(GObject *source, GAsyncResult *result, gpointer user_data) {
    FilesSearchContext *ctx = user_data;
    FilesMode *mode = files_mode(ctx ? ctx->app : NULL);
    if (!ctx || !mode || ctx->abandoned || ctx->generation != mode->search_generation) {
        files_search_unref(ctx);
        return;
    }

    GError *error = NULL;
    g_subprocess_wait_check_finish(G_SUBPROCESS(source), result, &error);
    if (error) {
        g_clear_error(&error);
    }
    ctx->wait_done = TRUE;
    maybe_finish_search(ctx);
    files_search_unref(ctx);
}

static void abandon_search(FilesSearchContext *ctx, gboolean force_exit) {
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

static gboolean on_timeout(gpointer data) {
    FilesSearchContext *ctx = data;
    FilesMode *mode = files_mode(ctx ? ctx->app : NULL);
    if (!ctx || !mode || ctx->generation != mode->search_generation) {
        files_search_unref(ctx);
        return G_SOURCE_REMOVE;
    }
    log_warn("files: fd search timed out after %dms", FILES_TIMEOUT_MS);
    abandon_search(ctx, TRUE);
    mode->loading = FALSE;
    files_set_status(mode, "fd timeout");
    files_search_unref(ctx);
    return G_SOURCE_REMOVE;
}

void cleanup_files_mode(AppData *app) {
    FilesMode *mode = files_mode(app);
    if (!mode) return;
    if (s_pending_search && s_pending_search->app == app) {
        abandon_search(s_pending_search, TRUE);
    }
    files_free_paths(&mode->paths);
    mode->path_count = 0;
    mode->filtered_count = 0;
    mode->cache_valid = FALSE;
    mode->loading = FALSE;
}

void files_on_enter(AppData *app) {
    FilesMode *mode = files_mode(app);
    if (!mode) return;
    if (!mode->cache_valid && !mode->loading) {
        files_search_refresh(app);
    } else if (mode->dirty && app->window_visible) {
        mode->dirty = FALSE;
        update_display(app);
    }
}

void files_on_leave(AppData *app) {
    FilesMode *mode = files_mode(app);
    if (!mode || !mode->loading) return;
    if (s_pending_search && s_pending_search->app == app) {
        abandon_search(s_pending_search, TRUE);
        mode->loading = FALSE;
    }
}

void files_on_query_changed(AppData *app, const char *query) {
    FilesMode *mode = files_mode(app);
    if (!mode) return;
    g_strlcpy(mode->query, query ? query : "", sizeof(mode->query));
    files_filter(app);
}

void files_search_refresh(AppData *app) {
    FilesMode *mode = files_mode(app);
    if (!mode) return;

    mode->search_generation++;
    if (s_pending_search && s_pending_search->app == app) {
        abandon_search(s_pending_search, TRUE);
    }

    if (!app->config.files_enabled) {
        mode->loading = FALSE;
        files_set_status(mode, "Files tab disabled");
        return;
    }

    gchar *tool_path = resolve_fd_tool(&app->config);
    if (!tool_path) {
        mode->unavailable = TRUE;
        mode->loading = FALSE;
        files_set_status(mode, "fd unavailable");
        return;
    }

    const char *home = g_get_home_dir();
    gchar **argv = build_fd_argv(&app->config, tool_path, home ? home : ".");
    FilesSearchContext *ctx = g_new0(FilesSearchContext, 1);
    ctx->refcount = 1;
    ctx->app = app;
    ctx->generation = mode->search_generation;
    ctx->cancel = g_cancellable_new();
    ctx->buffer = g_byte_array_new();
    ctx->paths = g_ptr_array_new_with_free_func(g_free);
    ctx->exclude_patterns = build_excludes(&app->config);

    GError *error = NULL;
    ctx->process = spawn_fd(tool_path, home ? home : ".", argv, &error);
    g_strfreev(argv);
    g_free(tool_path);
    if (!ctx->process) {
        log_warn("files: failed to start fd search: %s", error ? error->message : "unknown error");
        g_clear_error(&error);
        files_search_unref(ctx);
        mode->loading = FALSE;
        files_set_status(mode, "fd launch failed");
        return;
    }

    ctx->stdout_pipe = g_object_ref(g_subprocess_get_stdout_pipe(ctx->process));
    s_pending_search = files_search_ref(ctx);
    ctx->timeout_source_id = g_timeout_add(FILES_TIMEOUT_MS, on_timeout, files_search_ref(ctx));
    mode->loading = TRUE;
    mode->unavailable = FALSE;
    files_set_status(mode, "loading...");
    g_subprocess_wait_check_async(ctx->process, ctx->cancel, on_process_waited, files_search_ref(ctx));
    read_next_chunk(ctx);
    files_search_unref(ctx);
}

int files_row_count(AppData *app) {
    FilesMode *mode = files_mode(app);
    if (!mode) return 0;
    return mode->filtered_count > 0 ? mode->filtered_count : 1;
}

const char *files_path_at_visible(AppData *app, int visible_idx) {
    FilesMode *mode = files_mode(app);
    if (!mode || !mode->paths || visible_idx < 0 || visible_idx >= mode->filtered_count) {
        return NULL;
    }
    int raw = mode->filtered_indices[visible_idx];
    if (raw < 0 || raw >= (int)mode->paths->len) return NULL;
    return g_ptr_array_index(mode->paths, (guint)raw);
}

const char *files_match_string(AppData *app, int visible_idx) {
    const char *path = files_path_at_visible(app, visible_idx);
    return path ? path : files_status_message(app);
}

const char *files_row_identity(AppData *app, int visible_idx) {
    const char *path = files_path_at_visible(app, visible_idx);
    return path ? path : "";
}

const char *files_status_message(AppData *app) {
    FilesMode *mode = files_mode(app);
    if (!mode) return "";
    if (mode->status_message[0] != '\0' &&
        (mode->loading || mode->unavailable || !mode->cache_valid)) {
        return mode->status_message;
    }
    if (mode->loading && !mode->cache_valid) return "loading...";
    if (mode->unavailable) return "fd unavailable";
    if (!app->config.files_enabled) return "Files tab disabled";
    if (!mode->cache_valid) return "loading...";
    if (mode->query[0] == '\0') return "Type to search files...";
    return "No files matched";
}

gboolean files_status_is_error(AppData *app) {
    FilesMode *mode = files_mode(app);
    return mode && mode->unavailable;
}

#ifdef COFI_TESTING
void files_search_set_spawn_impl_for_test(FilesSearchSpawnImpl impl) {
    s_spawn_impl = impl;
}

void files_search_set_tool_path_for_test(const char *path) {
    g_free(s_tool_path_override);
    s_tool_path_override = path ? g_strdup(path) : NULL;
}

gboolean files_search_has_pending_for_test(void) {
    return s_pending_search != NULL;
}

void files_search_reset_for_test(void) {
    if (s_pending_search) {
        abandon_search(s_pending_search, TRUE);
    }
    g_clear_pointer(&s_tool_path_override, g_free);
    s_spawn_impl = NULL;
    s_missing_tool_logged = FALSE;
}

gboolean files_path_excluded_for_test(const CofiConfig *config, const char *path) {
    gchar **patterns = build_excludes(config);
    gboolean excluded = path_excluded(patterns, path);
    g_strfreev(patterns);
    return excluded;
}
#endif
