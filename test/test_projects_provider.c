#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>

#include "../src/app_data.h"
#include "../src/cofi_tab_provider.h"
#include "../src/command_registry.h"
#include "../src/slot_store.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(msg, cond) \
    do { \
        tests_run++; \
        if (cond) { \
            tests_passed++; \
            printf("PASS: %s\n", msg); \
        } else { \
            printf("FAIL: %s\n", msg); \
        } \
    } while (0)

static int g_exit_command_mode_calls;
static int g_hide_window_calls;
static int g_surface_tab_calls;
static TabMode g_last_surface_tab;
static int g_refresh_calls;
static int g_attach_named_calls;
static char g_last_attach_name[MAX_PROJECT_SESSION_NAME_LEN];
static int g_slot_recall_calls;
static char g_last_slot_payload[SLOT_STORE_PAYLOAD_LEN];
static gboolean g_has_named_result;
static CofiActionStatus g_attach_named_result = COFI_HANDLED_HIDE;
static CofiActionStatus g_slot_recall_result = COFI_HANDLED_HIDE;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

void exit_command_mode(AppData *app) {
    (void)app;
    g_exit_command_mode_calls++;
}

void hide_window(AppData *app) {
    (void)app;
    g_hide_window_calls++;
}

void surface_tab(AppData *app, TabMode tab) {
    (void)app;
    g_surface_tab_calls++;
    g_last_surface_tab = tab;
}

int cofi_register_command(const CommandSpec *spec) {
    return spec ? 0 : -1;
}

void projects_refresh(AppData *app) {
    (void)app;
    g_refresh_calls++;
}

gboolean projects_has_named(AppData *app, const char *name) {
    (void)app;
    return g_has_named_result && name && strcmp(name, "work") == 0;
}

CofiActionStatus projects_attach_named(AppData *app, const char *name) {
    (void)app;
    g_attach_named_calls++;
    g_strlcpy(g_last_attach_name, name ? name : "", sizeof(g_last_attach_name));
    return g_attach_named_result;
}

CofiActionStatus projects_slot_recall(AppData *app, const char *payload) {
    (void)app;
    g_slot_recall_calls++;
    g_strlcpy(g_last_slot_payload, payload ? payload : "", sizeof(g_last_slot_payload));
    return g_slot_recall_result;
}

ProjectSessionEntry *projects_selected_session(AppData *app) {
    (void)app;
    return NULL;
}

ProjectFolder *projects_selected_folder(AppData *app) {
    (void)app;
    return NULL;
}

gchar *projects_build_folder_session_name(const char *path) {
    (void)path;
    return g_strdup("folder");
}

void show_project_new_overlay(AppData *app,
                              ProjectBackend backend,
                              const char *start_dir,
                              const char *initial_name) {
    (void)app;
    (void)backend;
    (void)start_dir;
    (void)initial_name;
}

void show_project_kill_overlay(AppData *app, const char *session_name, ProjectBackend backend) {
    (void)app;
    (void)session_name;
    (void)backend;
}

void show_project_rename_overlay(AppData *app, const char *session_name) {
    (void)app;
    (void)session_name;
}

ProjectFolder *projects_folder_at_visible(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return NULL;
}

CofiActionStatus projects_open_folder(AppData *app, const char *path) {
    (void)app;
    (void)path;
    return COFI_HANDLED_HIDE;
}

CofiActionStatus projects_attach_visible(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return COFI_HANDLED_HIDE;
}

int projects_row_count(AppData *app) {
    (void)app;
    return 0;
}

void projects_format_row(AppData *app, int visible_idx, CofiRowCells *out) {
    (void)app;
    (void)visible_idx;
    if (out) out->cell_count = 0;
}

const char *projects_match_string(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return "";
}

const char *projects_row_identity(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return "";
}

void projects_on_enter(AppData *app) {
    (void)app;
}

void projects_on_query_changed(AppData *app, const char *query) {
    (void)app;
    (void)query;
}

void projects_on_tick(AppData *app, int generation) {
    (void)app;
    (void)generation;
}

const char *projects_get_shortcut_hint(AppData *app) {
    (void)app;
    return "";
}

const char *projects_slot_payload_for(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return NULL;
}

#include "../src/cofi_tab_provider.c"
#include "../src/slot_store.c"
#include "../src/projects_provider.c"

static const CofiTabProvider *registered_projects_provider(void) {
    cofi_registry_reset();
    projects_provider_register();
    return cofi_get_provider(s_projects_provider_id);
}

static void reset_capture(void) {
    g_exit_command_mode_calls = 0;
    g_hide_window_calls = 0;
    g_surface_tab_calls = 0;
    g_last_surface_tab = TAB_WINDOWS;
    g_refresh_calls = 0;
    g_attach_named_calls = 0;
    g_last_attach_name[0] = '\0';
    g_slot_recall_calls = 0;
    g_last_slot_payload[0] = '\0';
    g_has_named_result = FALSE;
    g_attach_named_result = COFI_HANDLED_HIDE;
    g_slot_recall_result = COFI_HANDLED_HIDE;
}

static void setup_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    app->current_tab = TAB_WINDOWS;
    app->textbuffer = gtk_text_buffer_new(NULL);
    slot_store_init(&app->harpoon.store);
}

static void teardown_app(AppData *app) {
    if (app->textbuffer) {
        g_object_unref(app->textbuffer);
        app->textbuffer = NULL;
    }
    slot_store_free(&app->harpoon.store);
}

static void read_textbuffer(GtkTextBuffer *buffer, char *out, size_t out_size) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    g_strlcpy(out, text ? text : "", out_size);
    g_free(text);
}

static void test_registered_command_metadata(void) {
    const CofiTabProvider *p = registered_projects_provider();

    ASSERT_TRUE("projects provider registered", p != NULL);
    ASSERT_TRUE("projects provider uses dynamic tab", p->tab_mode >= TAB_COUNT);
    ASSERT_TRUE("projects command primary", strcmp(s_projects_command.primary, "projects") == 0);
    ASSERT_TRUE("projects command alias project",
                strcmp(s_projects_command.aliases[0], "project") == 0);
    ASSERT_TRUE("projects command alias tmux",
                strcmp(s_projects_command.aliases[1], "tmux") == 0);
    ASSERT_TRUE("projects command alias zellij",
                strcmp(s_projects_command.aliases[4], "zellij") == 0);
    ASSERT_TRUE("projects command help",
                strcmp(s_projects_command.help_format,
                       "projects, project, tmux, tx, zj, zellij [@SLOT|SESSION]") == 0);
    ASSERT_TRUE("projects command description",
                strcmp(s_projects_command.description, "Switch to projects tab") == 0);
    ASSERT_TRUE("projects command handler registered", s_projects_command.handler != NULL);
    ASSERT_TRUE("projects command keeps open", s_projects_command.keeps_open_on_hotkey_auto == 1);
}

static void test_command_handler_without_args_surfaces_tab(void) {
    AppData app;
    const CofiTabProvider *p = registered_projects_provider();
    setup_app(&app);
    reset_capture();

    gboolean result = s_projects_command.handler(&app, NULL, "");

    ASSERT_TRUE("projects command without args returns false", result == FALSE);
    ASSERT_TRUE("projects command without args exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("projects command without args surfaces once", g_surface_tab_calls == 1);
    ASSERT_TRUE("projects command without args surfaces projects tab",
                p && g_last_surface_tab == (TabMode)p->tab_mode);
    ASSERT_TRUE("projects command without args keeps origin windows",
                app.prefix_origin_tab == TAB_WINDOWS);
    ASSERT_TRUE("projects command without args does not hide", g_hide_window_calls == 0);
    teardown_app(&app);
}

static void test_command_handler_named_session_hides(void) {
    AppData app;
    registered_projects_provider();
    setup_app(&app);
    reset_capture();
    g_has_named_result = TRUE;

    gboolean result = s_projects_command.handler(&app, NULL, "work");

    ASSERT_TRUE("projects named command returns false", result == FALSE);
    ASSERT_TRUE("projects named command refreshes", g_refresh_calls == 1);
    ASSERT_TRUE("projects named command attaches", g_attach_named_calls == 1);
    ASSERT_TRUE("projects named command passes name", strcmp(g_last_attach_name, "work") == 0);
    ASSERT_TRUE("projects named command hides", g_hide_window_calls == 1);
    teardown_app(&app);
}

static void test_command_handler_recalls_slot_and_hides(void) {
    AppData app;
    registered_projects_provider();
    setup_app(&app);
    slot_assign(&app.harpoon.store, 'a', "projects", "session:tmux:work");
    reset_capture();

    gboolean result = s_projects_command.handler(&app, NULL, "@a");

    ASSERT_TRUE("projects slot command returns false", result == FALSE);
    ASSERT_TRUE("projects slot command recalls", g_slot_recall_calls == 1);
    ASSERT_TRUE("projects slot command passes payload",
                strcmp(g_last_slot_payload, "session:tmux:work") == 0);
    ASSERT_TRUE("projects slot command hides", g_hide_window_calls == 1);
    teardown_app(&app);
}

static void test_command_handler_invalid_arg_shows_error(void) {
    AppData app;
    registered_projects_provider();
    setup_app(&app);
    reset_capture();

    gboolean result = s_projects_command.handler(&app, NULL, "missing");

    char text[128];
    read_textbuffer(app.textbuffer, text, sizeof(text));
    ASSERT_TRUE("projects invalid command returns false", result == FALSE);
    ASSERT_TRUE("projects invalid command does not hide", g_hide_window_calls == 0);
    ASSERT_TRUE("projects invalid command sets help state", app.command_mode.showing_help);
    ASSERT_TRUE("projects invalid command shows error",
                strcmp(text, "No matching tmux/zellij session.") == 0);
    teardown_app(&app);
}

static void test_command_args_contract(void) {
    AppData app;
    const CofiTabProvider *p = registered_projects_provider();
    setup_app(&app);
    reset_capture();

    ASSERT_TRUE("projects empty command args no-op",
                p->on_command_args(&app, "") == COFI_NO_OP);

    g_has_named_result = TRUE;
    ASSERT_TRUE("projects named command args hide",
                p->on_command_args(&app, "work") == COFI_HANDLED_HIDE);

    g_has_named_result = FALSE;
    slot_assign(&app.harpoon.store, 'b', "projects", "session:zellij:work");
    ASSERT_TRUE("projects slot command args hide",
                p->on_command_args(&app, "@b") == COFI_HANDLED_HIDE);

    ASSERT_TRUE("projects missing command args error",
                p->on_command_args(&app, "missing") == COFI_ACTION_ERROR);
    teardown_app(&app);
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) {
        printf("SKIP: GTK unavailable\n");
        return 0;
    }

    printf("projects_provider behavioral tests\n");
    printf("===================================\n\n");

    test_registered_command_metadata();
    test_command_handler_without_args_surfaces_tab();
    test_command_handler_named_session_hides();
    test_command_handler_recalls_slot_and_hides();
    test_command_handler_invalid_arg_shows_error();
    test_command_args_contract();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
