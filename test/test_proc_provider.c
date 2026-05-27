#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>

#include "core/app/app_data.h"
#include "providers/cofi_tab_provider.h"
#include "commands/command_registry.h"

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
static int g_surface_tab_calls;
static TabMode g_last_surface_tab;
static int g_execute_action_calls;
static char g_last_entry_text[128];
static guint g_last_modifier_state;
static gboolean g_execute_action_result = TRUE;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

gboolean proc_execute_action_with_modifiers(AppData *app, const char *entry_text, guint state) {
    (void)app;
    g_execute_action_calls++;
    g_strlcpy(g_last_entry_text, entry_text ? entry_text : "", sizeof(g_last_entry_text));
    g_last_modifier_state = state;
    return g_execute_action_result;
}

int proc_row_count(AppData *app) {
    (void)app;
    return 0;
}

void proc_format_row(AppData *app, int visible_idx, CofiRowCells *out) {
    (void)app;
    (void)visible_idx;
    if (out) {
        out->cell_count = 1;
        out->cells[0].text = "";
    }
}

const char *proc_match_string(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return "";
}

const char *proc_row_identity(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return "";
}

void proc_on_enter(AppData *app) {
    (void)app;
}

void proc_on_leave(AppData *app) {
    (void)app;
}

void proc_on_query_changed(AppData *app, const char *query) {
    (void)app;
    (void)query;
}

void proc_on_tick(AppData *app, int generation) {
    (void)app;
    (void)generation;
}

void exit_command_mode(AppData *app) {
    (void)app;
    g_exit_command_mode_calls++;
}

void surface_tab(AppData *app, TabMode tab) {
    (void)app;
    g_surface_tab_calls++;
    g_last_surface_tab = tab;
}

int cofi_register_command(const CommandSpec *spec) {
    return spec ? 0 : -1;
}

#include "providers/cofi_tab_provider.c"
#include "proc/proc_provider.c"

static const CofiTabProvider *registered_proc_provider(void) {
    cofi_registry_reset();
    proc_provider_register();
    return cofi_get_provider(s_proc_provider_id);
}

static void reset_capture(void) {
    g_exit_command_mode_calls = 0;
    g_surface_tab_calls = 0;
    g_last_surface_tab = TAB_WINDOWS;
    g_execute_action_calls = 0;
    g_last_entry_text[0] = '\0';
    g_last_modifier_state = 0;
    g_execute_action_result = TRUE;
}

static void setup_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    app->current_tab = TAB_WINDOWS;
    app->textbuffer = gtk_text_buffer_new(NULL);
}

static void teardown_app(AppData *app) {
    if (app->textbuffer) {
        g_object_unref(app->textbuffer);
        app->textbuffer = NULL;
    }
}

static void test_registered_command_metadata(void) {
    const CofiTabProvider *p = registered_proc_provider();

    ASSERT_TRUE("proc provider registered", p != NULL);
    ASSERT_TRUE("proc provider has dynamic tab", p->tab_mode >= TAB_COUNT);
    ASSERT_TRUE("proc command primary", strcmp(s_proc_command.primary, "proc") == 0);
    ASSERT_TRUE("proc command alias", strcmp(s_proc_command.aliases[0], "ps") == 0);
    ASSERT_TRUE("proc command help", strcmp(s_proc_command.help_format, "proc, ps") == 0);
    ASSERT_TRUE("proc command description",
                strcmp(s_proc_command.description, "Switch to process manager tab") == 0);
    ASSERT_TRUE("proc command handler registered", s_proc_command.handler != NULL);
    ASSERT_TRUE("proc command keeps open", s_proc_command.keeps_open_on_hotkey_auto == 1);
}

static void test_command_handler_surfaces_tab(void) {
    AppData app;
    const CofiTabProvider *provider = registered_proc_provider();
    setup_app(&app);
    reset_capture();

    gboolean result = s_proc_command.handler(&app, NULL, "");

    ASSERT_TRUE("proc command without args returns false", result == FALSE);
    ASSERT_TRUE("proc command without args exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("proc command without args surfaces once", g_surface_tab_calls == 1);
    ASSERT_TRUE("proc command without args surfaces proc tab",
                provider && g_last_surface_tab == (TabMode)provider->tab_mode);
    ASSERT_TRUE("proc command without args preserves origin", app.prefix_origin_tab == TAB_WINDOWS);
    ASSERT_TRUE("proc command without args does not execute action", g_execute_action_calls == 0);
    teardown_app(&app);
}

static void test_command_handler_with_args_still_surfaces_tab(void) {
    AppData app;
    const CofiTabProvider *provider = registered_proc_provider();
    setup_app(&app);
    reset_capture();

    gboolean result = s_proc_command.handler(&app, NULL, "firefox");

    ASSERT_TRUE("proc command with args returns false", result == FALSE);
    ASSERT_TRUE("proc command with args exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("proc command with args surfaces proc tab", g_surface_tab_calls == 1 &&
                provider && g_last_surface_tab == (TabMode)provider->tab_mode);
    ASSERT_TRUE("proc command with args does not execute action", g_execute_action_calls == 0);
    teardown_app(&app);
}

static void test_enter_pressed_delegates_action(void) {
    AppData app;
    const CofiTabProvider *p = registered_proc_provider();
    setup_app(&app);
    reset_capture();

    CofiActionStatus status = p->on_enter_pressed(&app, 0, 0, "kill 123", GDK_SHIFT_MASK);

    ASSERT_TRUE("proc enter handled hide", status == COFI_HANDLED_HIDE);
    ASSERT_TRUE("proc enter executes action", g_execute_action_calls == 1);
    ASSERT_TRUE("proc enter passes entry text", strcmp(g_last_entry_text, "kill 123") == 0);
    ASSERT_TRUE("proc enter passes modifier state", g_last_modifier_state == GDK_SHIFT_MASK);
    teardown_app(&app);
}

static void test_enter_pressed_noop_on_failed_action(void) {
    AppData app;
    const CofiTabProvider *p = registered_proc_provider();
    setup_app(&app);
    reset_capture();
    g_execute_action_result = FALSE;

    CofiActionStatus status = p->on_enter_pressed(&app, 0, 0, "bad", 0);

    ASSERT_TRUE("proc enter failed action is noop", status == COFI_NO_OP);
    ASSERT_TRUE("proc enter attempted action", g_execute_action_calls == 1);
    teardown_app(&app);
}

static void test_command_args_contract(void) {
    AppData app;
    const CofiTabProvider *p = registered_proc_provider();
    setup_app(&app);

    ASSERT_TRUE("proc empty command args no-op",
                p->on_command_args(&app, "") == COFI_NO_OP);
    ASSERT_TRUE("proc nonempty command args keep",
                p->on_command_args(&app, "firefox") == COFI_HANDLED_KEEP);
    teardown_app(&app);
}

int main(void) {
    printf("proc_provider behavioral tests\n");
    printf("==============================\n\n");

    test_registered_command_metadata();
    test_command_handler_surfaces_tab();
    test_command_handler_with_args_still_surfaces_tab();
    test_enter_pressed_delegates_action();
    test_enter_pressed_noop_on_failed_action();
    test_command_args_contract();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
