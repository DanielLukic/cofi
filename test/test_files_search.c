#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config/config.h"
#include "core/app/app_data.h"
#include "files/files_search.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(name, cond) \
    do { \
        tests_run++; \
        if (cond) { \
            tests_passed++; \
            printf("PASS: %s\n", name); \
        } else { \
            printf("FAIL: %s (line %d)\n", name, __LINE__); \
        } \
    } while (0)

static char g_script_path[512];
static int g_update_display_calls;
static int g_warn_count;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)file;
    (void)line;
    (void)fmt;
    if (level >= 3) {
        g_warn_count++;
    }
}

void update_display(AppData *app) {
    (void)app;
    g_update_display_calls++;
}

void preserve_selection(AppData *app) {
    if (!app) return;
    const char *id = files_row_identity(app, app->selection.provider_index);
    if (!id) id = "";
    g_strlcpy(app->selection.selected_provider_id,
              id,
              sizeof(app->selection.selected_provider_id));
}

void restore_selection(AppData *app) {
    if (!app) return;
    if (app->selection.selected_provider_id[0] == '\0') {
        app->selection.provider_index = 0;
        return;
    }
    int count = files_row_count(app);
    for (int i = 0; i < count; i++) {
        const char *id = files_row_identity(app, i);
        if (id && strcmp(id, app->selection.selected_provider_id) == 0) {
            app->selection.provider_index = i;
            return;
        }
    }
    app->selection.provider_index = 0;
}

void update_scroll_position(AppData *app) {
    (void)app;
}

static GSubprocess *spawn_test_script(const char *tool_path,
                                      const char *home_path,
                                      gchar **argv,
                                      GError **error) {
    (void)tool_path;
    (void)home_path;
    (void)argv;
    const gchar *script_argv[] = {g_script_path, NULL};
    return g_subprocess_newv(script_argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE, error);
}

static gboolean loop_until(gboolean (*predicate)(gpointer), gpointer data, guint timeout_ms) {
    GMainContext *ctx = g_main_context_default();
    gint64 deadline = g_get_monotonic_time() + (gint64)timeout_ms * 1000;
    while (!predicate(data) && g_get_monotonic_time() < deadline) {
        while (g_main_context_pending(ctx)) {
            g_main_context_iteration(ctx, FALSE);
        }
        g_usleep(1000);
    }
    return predicate(data);
}

static gboolean search_is_idle(gpointer data) {
    AppData *app = data;
    return app && !app->files_mode.loading && !files_search_has_pending_for_test();
}

static void write_script(const char *path, const char *body) {
    FILE *f = fopen(path, "w");
    ASSERT_TRUE("create test script", f != NULL);
    if (!f) return;
    fputs("#!/bin/sh\n", f);
    fputs(body, f);
    fclose(f);
    chmod(path, 0755);
}

static char *make_temp_dir(void) {
    char tmpl[] = "/tmp/cofi-files-search-XXXXXX";
    char *dir = g_strdup(tmpl);
    ASSERT_TRUE("mkdtemp dir", mkdtemp(dir) != NULL);
    return dir;
}

static void init_test_config(CofiConfig *config) {
    memset(config, 0, sizeof(*config));
    config->files_enabled = 1;
    config->files_fd_path[0] = '\0';
    config->files_excludes[0] = '\0';
}

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    init_test_config(&app->config);
    init_files_mode(&app->files_mode);
    app->files_mode.tab_mode = 99;
    app->current_tab = 99;
    app->window_visible = TRUE;
    g_update_display_calls = 0;
    g_warn_count = 0;
    files_search_reset_for_test();
    files_search_set_spawn_impl_for_test(spawn_test_script);
    files_search_set_tool_path_for_test("/usr/bin/fd");
}

static void kickoff_search(AppData *app, const char *query, guint generation) {
    app->files_mode.search_generation = generation - 1;
    files_on_query_changed(app, query);
    files_search_refresh(app);
}

static void test_excludes_compose_defaults_and_config(void) {
    CofiConfig config;
    init_test_config(&config);
    g_strlcpy(config.files_excludes, ".venv,dist", sizeof(config.files_excludes));

    ASSERT_TRUE("default excludes .ssh",
                files_path_excluded_for_test(&config, "/tmp/.ssh/id_rsa"));
    ASSERT_TRUE("default excludes node_modules",
                files_path_excluded_for_test(&config, "/tmp/x/node_modules/pkg/index.js"));
    ASSERT_TRUE("custom excludes .venv",
                files_path_excluded_for_test(&config, "/tmp/proj/.venv/bin/python"));
    ASSERT_TRUE("custom excludes dist",
                files_path_excluded_for_test(&config, "/tmp/proj/dist/app.js"));
    ASSERT_TRUE("normal project file survives excludes",
                !files_path_excluded_for_test(&config, "/tmp/proj/src/main.c"));
}

static void test_nul_parsing_keeps_newlines_inside_path_chunks(void) {
    AppData app;
    reset_state(&app);

    char *dir = make_temp_dir();
    g_snprintf(g_script_path, sizeof(g_script_path), "%s/nul.sh", dir);
    write_script(g_script_path,
                 "printf '%s\\0' '/tmp/studio.txt'\n"
                 "printf '%s\\0' '/tmp/line\nbreak.txt'\n");

    kickoff_search(&app, "txt", 1);
    ASSERT_TRUE("nul parser search completes", loop_until(search_is_idle, &app, 3000));
    ASSERT_TRUE("two paths cached", app.files_mode.path_count == 2);
    ASSERT_TRUE("newline preserved inside path",
                strcmp(files_path_at_visible(&app, 1), "/tmp/line\nbreak.txt") == 0 ||
                strcmp(files_path_at_visible(&app, 0), "/tmp/line\nbreak.txt") == 0);
    g_free(dir);
}

static void test_generation_guard_discards_stale_completion(void) {
    AppData app;
    reset_state(&app);

    char *dir = make_temp_dir();
    g_snprintf(g_script_path, sizeof(g_script_path), "%s/slow.sh", dir);
    write_script(g_script_path,
                 "sleep 0.2\n"
                 "printf '%s\\0' '/tmp/old.txt'\n");
    kickoff_search(&app, "old", 1);

    g_snprintf(g_script_path, sizeof(g_script_path), "%s/fast.sh", dir);
    write_script(g_script_path, "printf '%s\\0' '/tmp/new.txt'\n");
    kickoff_search(&app, "new", 2);

    ASSERT_TRUE("fresh search completes", loop_until(search_is_idle, &app, 3000));
    ASSERT_TRUE("stale search does not stomp cache",
                app.files_mode.path_count == 1 &&
                strcmp(g_ptr_array_index(app.files_mode.paths, 0), "/tmp/new.txt") == 0);
    g_free(dir);
}

static void test_cap_enforcement_limits_cache_to_fifty_thousand(void) {
    AppData app;
    reset_state(&app);

    char *dir = make_temp_dir();
    g_snprintf(g_script_path, sizeof(g_script_path), "%s/cap.sh", dir);
    FILE *script = fopen(g_script_path, "w");
    ASSERT_TRUE("open cap script", script != NULL);
    if (script) {
        fputs("#!/bin/sh\n", script);
        for (int i = 0; i < 60000; i++) {
            fprintf(script, "printf '%%s\\0' '/tmp/f%05d'\n", i);
        }
        fclose(script);
        chmod(g_script_path, 0755);
    }

    kickoff_search(&app, "f", 1);
    ASSERT_TRUE("cap search completes", loop_until(search_is_idle, &app, 5000));
    ASSERT_TRUE("cache capped at fifty thousand", app.files_mode.path_count == 50000);
    ASSERT_TRUE("warn logged on cap hit", g_warn_count > 0);
    g_free(dir);
}

static void test_fzf_top_hits_rank_known_paths(void) {
    AppData app;
    reset_state(&app);

    char *dir = make_temp_dir();
    g_snprintf(g_script_path, sizeof(g_script_path), "%s/rank.sh", dir);
    write_script(g_script_path,
                 "printf '%s\\0' '/tmp/notes/studio.txt'\n"
                 "printf '%s\\0' '/tmp/src/still-undone.md'\n"
                 "printf '%s\\0' '/tmp/archive/student-list.csv'\n");

    kickoff_search(&app, "studio", 1);
    ASSERT_TRUE("rank search completes", loop_until(search_is_idle, &app, 3000));
    ASSERT_TRUE("best hit is studio.txt",
                files_path_at_visible(&app, 0) &&
                strcmp(files_path_at_visible(&app, 0), "/tmp/notes/studio.txt") == 0);
    g_free(dir);
}

int main(void) {
    printf("files_search tests\n");
    printf("==================\n\n");

    test_excludes_compose_defaults_and_config();
    test_nul_parsing_keeps_newlines_inside_path_chunks();
    test_generation_guard_discards_stale_completion();
    test_cap_enforcement_limits_cache_to_fifty_thousand();
    test_fzf_top_hits_rank_known_paths();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_run == tests_passed ? 0 : 1;
}
