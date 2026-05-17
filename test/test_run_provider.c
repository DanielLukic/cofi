#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/cofi_tab_provider.h"
#include "../src/command_registry.h"

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

static int g_detach_calls;
static char g_last_detach_command[256];
static gboolean g_detach_result = TRUE;
static int g_update_display_calls;
static int g_exit_command_mode_calls;
static int g_hide_window_calls;
static int g_enter_modal_calls;
static const CofiTabProvider *g_last_modal_provider;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

gboolean detach_launch_shell(const char *command) {
    g_detach_calls++;
    g_strlcpy(g_last_detach_command, command ? command : "", sizeof(g_last_detach_command));
    return g_detach_result;
}

void update_display(AppData *app) {
    (void)app;
    g_update_display_calls++;
}

void exit_command_mode(AppData *app) {
    (void)app;
    g_exit_command_mode_calls++;
}

void hide_window(AppData *app) {
    (void)app;
    g_hide_window_calls++;
}

void cofi_enter_modal(AppData *app, const CofiTabProvider *provider) {
    (void)app;
    g_enter_modal_calls++;
    g_last_modal_provider = provider;
}

int cofi_register_command(const CommandSpec *spec) {
    return spec ? 0 : -1;
}

int get_max_display_lines_dynamic(AppData *app) {
    (void)app;
    return 10;
}

#include "../src/cofi_tab_provider.c"
#include "../src/run_mode.c"
#include "../src/run_provider.c"
#include "../src/selection.c"

static void setup_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    app->entry = gtk_entry_new();
    app->current_tab = TAB_RUN;
    app->selection.provider_index = 0;
    init_run_mode(&app->run_mode);
}

static const CofiTabProvider *registered_run_provider(void) {
    cofi_registry_reset();
    run_provider_register();
    return cofi_get_provider_for_tab(TAB_RUN);
}

static void seed_history(AppData *app) {
    add_run_history_entry(&app->run_mode, "echo one");
    add_run_history_entry(&app->run_mode, "echo two");
    add_run_history_entry(&app->run_mode, "echo three");
}

static void reset_launch_capture(void) {
    g_detach_calls = 0;
    g_last_detach_command[0] = '\0';
    g_detach_result = TRUE;
}

static void test_selection_changed_fills_entry_from_history(void) {
    AppData app;
    registered_run_provider();
    setup_app(&app);
    seed_history(&app);

    move_selection_up(&app);
    ASSERT_TRUE("selection up moves to index 1", app.selection.provider_index == 1);
    ASSERT_TRUE("selection up fills selected history",
                strcmp(gtk_entry_get_text(GTK_ENTRY(app.entry)), "echo two") == 0);

    move_selection_up(&app);
    ASSERT_TRUE("second selection up moves to index 2", app.selection.provider_index == 2);
    ASSERT_TRUE("second selection up fills selected history",
                strcmp(gtk_entry_get_text(GTK_ENTRY(app.entry)), "echo one") == 0);

    move_selection_down(&app);
    ASSERT_TRUE("selection down moves to index 1", app.selection.provider_index == 1);
    ASSERT_TRUE("selection down fills selected history",
                strcmp(gtk_entry_get_text(GTK_ENTRY(app.entry)), "echo two") == 0);
}

static void test_selection_changed_invalid_index_clears_entry(void) {
    AppData app;
    const CofiTabProvider *p = registered_run_provider();
    setup_app(&app);
    seed_history(&app);
    gtk_entry_set_text(GTK_ENTRY(app.entry), "stale");

    p->on_selection_changed(&app, -1);

    ASSERT_TRUE("invalid selection clears entry",
                strcmp(gtk_entry_get_text(GTK_ENTRY(app.entry)), "") == 0);
    ASSERT_TRUE("invalid selection does not detach", g_detach_calls == 0);
}

static void test_empty_enter_reexecutes_selected_history_without_dup(void) {
    AppData app;
    const CofiTabProvider *p = registered_run_provider();
    setup_app(&app);
    add_run_history_entry(&app.run_mode, "echo foo");
    int before_count = app.run_mode.history_count;
    reset_launch_capture();

    CofiActionStatus status = p->on_enter_pressed(&app, 0, 0, "", 0);

    ASSERT_TRUE("empty enter returns hide", status == COFI_HANDLED_HIDE);
    ASSERT_TRUE("empty enter launches selected history", g_detach_calls == 1);
    ASSERT_TRUE("empty enter launches exact command",
                strcmp(g_last_detach_command, "echo foo") == 0);
    ASSERT_TRUE("empty enter does not duplicate history",
                app.run_mode.history_count == before_count);
}

static void test_command_args_launch_and_add_history(void) {
    AppData app;
    const CofiTabProvider *p = registered_run_provider();
    setup_app(&app);
    reset_launch_capture();

    CofiActionStatus status = p->on_command_args(&app, "echo arg");

    ASSERT_TRUE(":run args returns hide", status == COFI_HANDLED_HIDE);
    ASSERT_TRUE(":run args launches command", g_detach_calls == 1);
    ASSERT_TRUE(":run args launch text captured",
                strcmp(g_last_detach_command, "echo arg") == 0);
    ASSERT_TRUE(":run args adds history",
                app.run_mode.history_count == 1 &&
                strcmp(app.run_mode.history[0], "echo arg") == 0);
}

static void test_empty_command_args_surface_path_is_noop(void) {
    AppData app;
    const CofiTabProvider *p = registered_run_provider();
    setup_app(&app);
    reset_launch_capture();

    CofiActionStatus status = p->on_command_args(&app, "");

    ASSERT_TRUE(":run empty args returns no-op for surface path", status == COFI_NO_OP);
    ASSERT_TRUE(":run empty args does not launch", g_detach_calls == 0);
    ASSERT_TRUE(":run empty args does not add history", app.run_mode.history_count == 0);
}

static void test_registered_command_metadata(void) {
    const CofiTabProvider *p = registered_run_provider();

    ASSERT_TRUE("run provider registered", p != NULL);
    ASSERT_TRUE("run command primary", strcmp(s_run_command.primary, "run") == 0);
    ASSERT_TRUE("run command alias", strcmp(s_run_command.aliases[0], "r") == 0);
    ASSERT_TRUE("run command help", strcmp(s_run_command.help_format, "run, r") == 0);
    ASSERT_TRUE("run command description",
                strcmp(s_run_command.description, "Switch to run mode") == 0);
    ASSERT_TRUE("run command handler registered", s_run_command.handler != NULL);
    ASSERT_TRUE("run command keeps open", s_run_command.keeps_open_on_hotkey_auto == 1);
}

static void test_command_handler_launches_args_and_hides(void) {
    AppData app;
    registered_run_provider();
    setup_app(&app);
    reset_launch_capture();
    g_exit_command_mode_calls = 0;
    g_hide_window_calls = 0;

    gboolean result = s_run_command.handler(&app, NULL, "xterm");

    ASSERT_TRUE("run command with args returns false", result == FALSE);
    ASSERT_TRUE("run command with args exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("run command with args launches once", g_detach_calls == 1);
    ASSERT_TRUE("run command with args captures command",
                strcmp(g_last_detach_command, "xterm") == 0);
    ASSERT_TRUE("run command with args adds history",
                app.run_mode.history_count == 1 &&
                strcmp(app.run_mode.history[0], "xterm") == 0);
    ASSERT_TRUE("run command with args hides after launch", g_hide_window_calls == 1);
}

static void test_command_handler_without_args_enters_modal(void) {
    AppData app;
    registered_run_provider();
    setup_app(&app);
    g_exit_command_mode_calls = 0;
    g_hide_window_calls = 0;
    g_enter_modal_calls = 0;
    g_last_modal_provider = NULL;
    reset_launch_capture();

    gboolean result = s_run_command.handler(&app, NULL, "");

    ASSERT_TRUE("run command without args returns false", result == FALSE);
    ASSERT_TRUE("run command without args exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("run command without args does not launch", g_detach_calls == 0);
    ASSERT_TRUE("run command without args does not hide", g_hide_window_calls == 0);
    ASSERT_TRUE("run command without args sets prefix claim", app.active_prefix_claim == '!');
    ASSERT_TRUE("run command without args enters modal", g_enter_modal_calls == 1);
    ASSERT_TRUE("run command without args uses module provider modal",
                g_last_modal_provider == &s_run_provider);
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) {
        printf("SKIP: GTK unavailable\n");
        return 0;
    }

    printf("run_provider behavioral tests\n");
    printf("=============================\n\n");

    test_selection_changed_fills_entry_from_history();
    test_selection_changed_invalid_index_clears_entry();
    test_empty_enter_reexecutes_selected_history_without_dup();
    test_command_args_launch_and_add_history();
    test_empty_command_args_surface_path_is_noop();
    test_registered_command_metadata();
    test_command_handler_launches_args_and_hides();
    test_command_handler_without_args_enters_modal();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
