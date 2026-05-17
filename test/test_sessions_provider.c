#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>

#include "../src/app_data.h"
#include "../src/cofi_tab_provider.h"
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
static char g_last_attach_name[MAX_SESSION_NAME_LEN];
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

void sessions_refresh(AppData *app) {
    (void)app;
    g_refresh_calls++;
}

gboolean sessions_has_named(AppData *app, const char *name) {
    (void)app;
    return g_has_named_result && name && strcmp(name, "work") == 0;
}

CofiActionStatus sessions_attach_named(AppData *app, const char *name) {
    (void)app;
    g_attach_named_calls++;
    g_strlcpy(g_last_attach_name, name ? name : "", sizeof(g_last_attach_name));
    return g_attach_named_result;
}

CofiActionStatus sessions_slot_recall(AppData *app, const char *payload) {
    (void)app;
    g_slot_recall_calls++;
    g_strlcpy(g_last_slot_payload, payload ? payload : "", sizeof(g_last_slot_payload));
    return g_slot_recall_result;
}

SessionEntry *sessions_selected_session(AppData *app) {
    (void)app;
    return NULL;
}

SessionFolder *sessions_selected_folder(AppData *app) {
    (void)app;
    return NULL;
}

gchar *sessions_build_folder_session_name(const char *path) {
    (void)path;
    return g_strdup("folder");
}

void show_session_new_overlay(AppData *app,
                              SessionBackend backend,
                              const char *start_dir,
                              const char *initial_name) {
    (void)app;
    (void)backend;
    (void)start_dir;
    (void)initial_name;
}

void show_session_kill_overlay(AppData *app, const char *session_name, SessionBackend backend) {
    (void)app;
    (void)session_name;
    (void)backend;
}

void show_session_rename_overlay(AppData *app, const char *session_name) {
    (void)app;
    (void)session_name;
}

SessionFolder *sessions_folder_at_visible(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return NULL;
}

CofiActionStatus sessions_open_folder(AppData *app, const char *path) {
    (void)app;
    (void)path;
    return COFI_HANDLED_HIDE;
}

CofiActionStatus sessions_attach_visible(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return COFI_HANDLED_HIDE;
}

int sessions_row_count(AppData *app) {
    (void)app;
    return 0;
}

void sessions_format_row(AppData *app, int visible_idx, CofiRowCells *out) {
    (void)app;
    (void)visible_idx;
    if (out) out->cell_count = 0;
}

const char *sessions_match_string(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return "";
}

const char *sessions_row_identity(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return "";
}

void sessions_on_enter(AppData *app) {
    (void)app;
}

void sessions_on_query_changed(AppData *app, const char *query) {
    (void)app;
    (void)query;
}

void sessions_on_tick(AppData *app, int generation) {
    (void)app;
    (void)generation;
}

const char *sessions_get_shortcut_hint(AppData *app) {
    (void)app;
    return "";
}

const char *sessions_slot_payload_for(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return NULL;
}

#include "../src/cofi_tab_provider.c"
#include "../src/slot_store.c"
#include "../src/sessions_provider.c"

static const CofiTabProvider *registered_sessions_provider(void) {
    cofi_registry_reset();
    sessions_provider_register();
    return cofi_get_provider_for_tab(TAB_SESSIONS);
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
    const CofiTabProvider *p = registered_sessions_provider();

    ASSERT_TRUE("sessions command registered", p != NULL);
    ASSERT_TRUE("sessions command primary", p && strcmp(p->primary_cmd, "sessions") == 0);
    ASSERT_TRUE("sessions command alias tmux",
                p && p->aliases && strcmp(p->aliases[0], "tmux") == 0);
    ASSERT_TRUE("sessions command alias zellij",
                p && p->aliases && strcmp(p->aliases[3], "zellij") == 0);
    ASSERT_TRUE("sessions command help",
                p && strcmp(p->command_help_format,
                            "sessions, tmux, tx, zj, zellij [@SLOT|SESSION]") == 0);
    ASSERT_TRUE("sessions command description",
                p && strcmp(p->command_description, "Switch to sessions tab") == 0);
    ASSERT_TRUE("sessions command handler registered", p && p->command_handler != NULL);
    ASSERT_TRUE("sessions command keeps open", p && p->command_keeps_open_on_hotkey_auto == 1);
}

static void test_command_handler_without_args_surfaces_tab(void) {
    AppData app;
    const CofiTabProvider *p = registered_sessions_provider();
    setup_app(&app);
    reset_capture();

    gboolean result = p->command_handler(&app, NULL, "");

    ASSERT_TRUE("sessions command without args returns false", result == FALSE);
    ASSERT_TRUE("sessions command without args exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("sessions command without args surfaces once", g_surface_tab_calls == 1);
    ASSERT_TRUE("sessions command without args surfaces sessions tab",
                g_last_surface_tab == TAB_SESSIONS);
    ASSERT_TRUE("sessions command without args keeps origin windows",
                app.prefix_origin_tab == TAB_WINDOWS);
    ASSERT_TRUE("sessions command without args does not hide", g_hide_window_calls == 0);
    teardown_app(&app);
}

static void test_command_handler_named_session_hides(void) {
    AppData app;
    const CofiTabProvider *p = registered_sessions_provider();
    setup_app(&app);
    reset_capture();
    g_has_named_result = TRUE;

    gboolean result = p->command_handler(&app, NULL, "work");

    ASSERT_TRUE("sessions named command returns false", result == FALSE);
    ASSERT_TRUE("sessions named command refreshes", g_refresh_calls == 1);
    ASSERT_TRUE("sessions named command attaches", g_attach_named_calls == 1);
    ASSERT_TRUE("sessions named command passes name", strcmp(g_last_attach_name, "work") == 0);
    ASSERT_TRUE("sessions named command hides", g_hide_window_calls == 1);
    teardown_app(&app);
}

static void test_command_handler_recalls_slot_and_hides(void) {
    AppData app;
    const CofiTabProvider *p = registered_sessions_provider();
    setup_app(&app);
    slot_assign(&app.harpoon.store, 'a', "sessions", "session:tmux:work");
    reset_capture();

    gboolean result = p->command_handler(&app, NULL, "@a");

    ASSERT_TRUE("sessions slot command returns false", result == FALSE);
    ASSERT_TRUE("sessions slot command recalls", g_slot_recall_calls == 1);
    ASSERT_TRUE("sessions slot command passes payload",
                strcmp(g_last_slot_payload, "session:tmux:work") == 0);
    ASSERT_TRUE("sessions slot command hides", g_hide_window_calls == 1);
    teardown_app(&app);
}

static void test_command_handler_invalid_arg_shows_error(void) {
    AppData app;
    const CofiTabProvider *p = registered_sessions_provider();
    setup_app(&app);
    reset_capture();

    gboolean result = p->command_handler(&app, NULL, "missing");

    char text[128];
    read_textbuffer(app.textbuffer, text, sizeof(text));
    ASSERT_TRUE("sessions invalid command returns false", result == FALSE);
    ASSERT_TRUE("sessions invalid command does not hide", g_hide_window_calls == 0);
    ASSERT_TRUE("sessions invalid command sets help state", app.command_mode.showing_help);
    ASSERT_TRUE("sessions invalid command shows error",
                strcmp(text, "No matching tmux/zellij session.") == 0);
    teardown_app(&app);
}

static void test_command_args_contract(void) {
    AppData app;
    const CofiTabProvider *p = registered_sessions_provider();
    setup_app(&app);
    reset_capture();

    ASSERT_TRUE("sessions empty command args no-op",
                p->on_command_args(&app, "") == COFI_NO_OP);

    g_has_named_result = TRUE;
    ASSERT_TRUE("sessions named command args hide",
                p->on_command_args(&app, "work") == COFI_HANDLED_HIDE);

    g_has_named_result = FALSE;
    slot_assign(&app.harpoon.store, 'b', "sessions", "session:zellij:work");
    ASSERT_TRUE("sessions slot command args hide",
                p->on_command_args(&app, "@b") == COFI_HANDLED_HIDE);

    ASSERT_TRUE("sessions missing command args error",
                p->on_command_args(&app, "missing") == COFI_ACTION_ERROR);
    teardown_app(&app);
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) {
        printf("SKIP: GTK unavailable\n");
        return 0;
    }

    printf("sessions_provider behavioral tests\n");
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
