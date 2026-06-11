#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "projects/projects.h"
#include "projects/projects_parse.h"
#include "projects/locate/projects_locate.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s (line %d)\n", name, __LINE__); fail++; } \
} while (0)

#define ASSERT_EQ_INT(name, expected, actual) \
    ASSERT_TRUE(name, (expected) == (actual))

static gboolean g_remote_scope_active;
static gboolean g_remote_scope_loading;
static int g_locate_search_calls;
static int g_locate_cancel_calls;
static guint g_last_locate_generation;
static char g_last_locate_query[256];

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

gboolean detach_launch_in_terminal_cmd(const char *command) {
    (void)command;
    return TRUE;
}

gboolean detach_launch_argv_array(const char *const *argv) {
    (void)argv;
    return TRUE;
}

void get_window_list(AppData *app) {
    (void)app;
}

void activate_window(Display *display, Window window_id) {
    (void)display;
    (void)window_id;
}

void preserve_selection(AppData *app) {
    (void)app;
}

void restore_selection(AppData *app) {
    (void)app;
}

void update_scroll_position(AppData *app) {
    (void)app;
}

void update_display(AppData *app) {
    (void)app;
}

void reset_selection(AppData *app) {
    (void)app;
}

gboolean projects_activate_remote_attach_window(AppData *app,
                                                const char *host,
                                                const char *remote_tool,
                                                const char *session_name) {
    (void)app;
    (void)host;
    (void)remote_tool;
    (void)session_name;
    return FALSE;
}

gboolean projects_activate_tmux_window(AppData *app,
                                       const char *tmux_path,
                                       const char *session_name) {
    (void)app;
    (void)tmux_path;
    (void)session_name;
    return FALSE;
}

gboolean projects_activate_zellij_window(AppData *app,
                                         const char *zellij_path,
                                         const char *session_name) {
    (void)app;
    (void)zellij_path;
    (void)session_name;
    return FALSE;
}

WindowInfo *projects_find_caja_folder_window(AppData *app,
                                             const char *path,
                                             Window *stacking,
                                             unsigned long stack_count) {
    (void)app;
    (void)path;
    (void)stacking;
    (void)stack_count;
    return NULL;
}

void projects_remote_store_save_intent(const char *host,
                                       ProjectBackend backend,
                                       const char *name,
                                       const char *cwd) {
    (void)host;
    (void)backend;
    (void)name;
    (void)cwd;
}

gboolean projects_remote_store_forget(const char *host,
                                      ProjectBackend backend,
                                      const char *name,
                                      const char *cwd) {
    (void)host;
    (void)backend;
    (void)name;
    (void)cwd;
    return TRUE;
}

gboolean projects_remote_scope_is_active(void) {
    return g_remote_scope_active;
}

gboolean projects_remote_scope_is_loading(void) {
    return g_remote_scope_loading;
}

void projects_remote_scope_clear_status_message(void) {}

void projects_remote_scope_clear(void) {}

const char *projects_remote_scope_current_host(void) {
    return NULL;
}

void projects_refresh(AppData *app) {
    (void)app;
}

void projects_locate_search_async(AppData *app, const char *query, guint generation) {
    (void)app;
    g_locate_search_calls++;
    g_last_locate_generation = generation;
    g_strlcpy(g_last_locate_query, query ? query : "", sizeof(g_last_locate_query));
}

void projects_locate_cancel_pending(AppData *app) {
    (void)app;
    g_locate_cancel_calls++;
}

void projects_locate_set_results_callback(ProjectsLocateResultsCallback callback) {
    (void)callback;
}

static void reset_capture(void) {
    g_remote_scope_active = FALSE;
    g_remote_scope_loading = FALSE;
    g_locate_search_calls = 0;
    g_locate_cancel_calls = 0;
    g_last_locate_generation = 0;
    g_last_locate_query[0] = '\0';
}

static void seed_folder(ProjectFolder *folder, const char *path) {
    folder->path = g_strdup(path);
    folder->label = g_path_get_basename(path);
    folder->source = FOLDER_SOURCE_ZOXIDE;
    folder->is_remote = FALSE;
    folder->remote_host[0] = '\0';
}

static void seed_folder_with_source(ProjectFolder *folder,
                                    const char *path,
                                    ProjectFolderSource source) {
    seed_folder(folder, path);
    folder->source = source;
}

static void reset_mode(ProjectsMode *mode) {
    projects_clear_folders(mode->folders, mode->folder_count);
    init_projects_mode(mode);
}

static void test_locate_fires_when_only_fuzzy_primary_matches_exist(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    reset_capture();

    seed_folder(&app.projects_mode.folders[0], "/tmp/proj/hairnet");
    seed_folder(&app.projects_mode.folders[1], "/tmp/proj/hairband");
    seed_folder(&app.projects_mode.folders[2], "/tmp/proj/hayrun");
    seed_folder(&app.projects_mode.folders[3], "/tmp/proj/hardnose");
    app.projects_mode.folder_count = 4;
    app.projects_mode.primary_folder_count = 4;

    projects_filter(&app, "harn");

    ASSERT_EQ_INT("harn still has fuzzy primary rows", 4, app.projects_mode.filtered_count);
    ASSERT_TRUE("harn fires locate despite fuzzy rows",
                g_locate_search_calls == 1 &&
                strcmp(g_last_locate_query, "harn") == 0 &&
                g_last_locate_generation == 1);

    reset_mode(&app.projects_mode);
}

static void test_locate_still_fires_when_primary_contains_literal_substring(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    reset_capture();

    seed_folder(&app.projects_mode.folders[0], "/tmp/proj/cofi");
    seed_folder(&app.projects_mode.folders[1], "/tmp/proj/codex");
    app.projects_mode.folder_count = 2;
    app.projects_mode.primary_folder_count = 2;

    projects_filter(&app, "cof");

    ASSERT_TRUE("query change cancels pending locate", g_locate_cancel_calls == 1);
    ASSERT_EQ_INT("cof keeps literal primary rows", 1, app.projects_mode.filtered_count);
    ASSERT_TRUE("cof still fires locate without substring gate",
                g_locate_search_calls == 1 &&
                strcmp(g_last_locate_query, "cof") == 0 &&
                g_last_locate_generation == 1);

    reset_mode(&app.projects_mode);
}

static void test_locate_fires_when_primary_corpus_is_empty(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    reset_capture();

    projects_filter(&app, "harn");

    ASSERT_EQ_INT("empty corpus has no primary rows", 0, app.projects_mode.filtered_count);
    ASSERT_TRUE("empty corpus fires locate",
                g_locate_search_calls == 1 &&
                strcmp(g_last_locate_query, "harn") == 0);
}

static void test_locate_does_not_fire_for_short_queries(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    reset_capture();

    projects_filter(&app, "ha");

    ASSERT_EQ_INT("short query does not fire locate", 0, g_locate_search_calls);
}

static void test_primary_rows_get_priority_bonus_over_locate_rows(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    reset_capture();

    seed_folder_with_source(&app.projects_mode.folders[0],
                            "/tmp/proj/sample-harness",
                            FOLDER_SOURCE_ZOXIDE);
    seed_folder_with_source(&app.projects_mode.folders[1],
                            "/tmp/locate/harness",
                            FOLDER_SOURCE_LOCATE);
    app.projects_mode.folder_count = 2;
    app.projects_mode.primary_folder_count = 1;

    projects_filter(&app, "harnes");

    ASSERT_EQ_INT("priority test has two filtered rows", 2, app.projects_mode.filtered_count);
    ASSERT_TRUE("primary zoxide row ranks ahead of locate row",
                app.projects_mode.filtered_rows[0].type == PROJECT_ROW_FOLDER &&
                app.projects_mode.filtered_rows[0].index == 0);

    reset_mode(&app.projects_mode);
}

int main(void) {
    printf("projects_filter behavioral tests\n");
    printf("================================\n\n");

    test_locate_fires_when_only_fuzzy_primary_matches_exist();
    test_locate_still_fires_when_primary_contains_literal_substring();
    test_locate_fires_when_primary_corpus_is_empty();
    test_locate_does_not_fire_for_short_queries();
    test_primary_rows_get_priority_bonus_over_locate_rows();

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail == 0 ? 0 : 1;
}
