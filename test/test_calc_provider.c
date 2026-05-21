#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/calc_provider.c"

static int pass = 0;
static int fail = 0;
static int exit_command_mode_calls = 0;
static int enter_modal_calls = 0;
static const CofiTabProvider *last_modal_provider = NULL;
static int update_display_calls = 0;
static int clipboard_set_calls = 0;
static char clipboard_text[CALC_RESULT_LEN];

int cofi_register_command(const CommandSpec *spec) {
    return spec ? 0 : -1;
}

void exit_command_mode(AppData *app) {
    (void)app;
    exit_command_mode_calls++;
}

void cofi_enter_modal(AppData *app, const CofiTabProvider *provider) {
    (void)app;
    enter_modal_calls++;
    last_modal_provider = provider;
}

void update_display(AppData *app) {
    (void)app;
    update_display_calls++;
}

GtkClipboard *gtk_clipboard_get(GdkAtom selection) {
    (void)selection;
    return (GtkClipboard *)0x1;
}

void gtk_clipboard_set_text(GtkClipboard *clipboard, const gchar *text, gint len) {
    (void)clipboard;
    (void)len;
    clipboard_set_calls++;
    g_strlcpy(clipboard_text, text ? text : "", sizeof(clipboard_text));
}

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else       { printf("FAIL: %s\n", name); fail++; } \
} while (0)

#define ASSERT_STR(name, a, b) \
    ASSERT_TRUE(name, strcmp((a), (b)) == 0)

static void test_error_result_column_is_wide_enough(void) {
    AppData app = {0};
    CofiRowCells row = {0};
    const char *error = "[error: bad expression]";

    g_strlcpy(app.calc_mode.entries[0].expr, "bad", CALC_EXPR_LEN);
    g_strlcpy(app.calc_mode.entries[0].result, error, CALC_RESULT_LEN);
    app.calc_mode.count = 1;

    calc_format_row(&app, 0, &row);

    ASSERT_STR("calc provider exposes error result", row.cells[0].text, error);
    ASSERT_TRUE("calc provider error result fits column",
                row.cells[0].width_hint >= (int)strlen(error));
}

static void test_registered_command_metadata(void) {
    cofi_registry_reset();
    calc_provider_register();

    ASSERT_STR("calc command primary", s_calc_command.primary, "calc");
    ASSERT_STR("calc command help", s_calc_command.help_format, "calc, ca");
    ASSERT_STR("calc command description", s_calc_command.description, "Switch to calculator");
    ASSERT_TRUE("calc command handler registered", s_calc_command.handler != NULL);
    ASSERT_TRUE("calc command keeps open", s_calc_command.keeps_open_on_hotkey_auto == 1);
}

static void test_command_handler_enters_modal_and_evaluates_args(void) {
    AppData app = {0};
    cofi_registry_reset();
    calc_provider_register();
    const CofiTabProvider *registered_provider = cofi_get_provider(s_calc_provider_id);

    ASSERT_TRUE("calc command handler registered for test", s_calc_command.handler != NULL);
    if (!s_calc_command.handler) return;

    exit_command_mode_calls = 0;
    enter_modal_calls = 0;
    last_modal_provider = NULL;

    gboolean result = s_calc_command.handler(&app, NULL, "1+1");

    ASSERT_TRUE("calc command returns false", result == FALSE);
    ASSERT_TRUE("calc command exits command mode", exit_command_mode_calls == 1);
    ASSERT_TRUE("calc command enters modal", enter_modal_calls == 1);
    ASSERT_TRUE("calc command uses registered provider modal",
                last_modal_provider == registered_provider);
    ASSERT_TRUE("calc command uses dynamic tab provider",
                registered_provider && registered_provider->tab_mode >= TAB_COUNT);
    ASSERT_TRUE("calc command sets prefix claim", app.active_prefix_claim == '=');
    ASSERT_TRUE("calc command evaluates args", app.calc_mode.count == 1);
    ASSERT_STR("calc command stores result", app.calc_mode.entries[0].result, "2");
}

static void test_command_handler_records_invalid_expression(void) {
    AppData app = {0};
    cofi_registry_reset();
    calc_provider_register();

    ASSERT_TRUE("calc command handler registered for invalid command test",
                s_calc_command.handler != NULL);
    if (!s_calc_command.handler) return;

    s_calc_command.handler(&app, NULL, "foo");

    ASSERT_TRUE("invalid calc command pushes history", app.calc_mode.count == 1);
    ASSERT_STR("invalid calc command stores expression", app.calc_mode.entries[0].expr, "foo");
    ASSERT_STR("invalid calc command stores error result",
               app.calc_mode.entries[0].result, "[error: bad expression]");
}

static void test_command_handler_without_args_only_enters_modal(void) {
    AppData app = {0};
    cofi_registry_reset();
    calc_provider_register();

    ASSERT_TRUE("calc command handler registered for empty command test",
                s_calc_command.handler != NULL);
    if (!s_calc_command.handler) return;

    exit_command_mode_calls = 0;
    enter_modal_calls = 0;

    s_calc_command.handler(&app, NULL, "");

    ASSERT_TRUE("calc command without args exits command mode", exit_command_mode_calls == 1);
    ASSERT_TRUE("calc command without args enters modal", enter_modal_calls == 1);
    ASSERT_TRUE("calc command without args does not evaluate", app.calc_mode.count == 0);
}

static void test_ctrl_x_clears_and_persists_history(void) {
    AppData app = {0};
    GdkEventKey event = {0};
    CalcMode loaded = {0};

    g_strlcpy(app.calc_mode.entries[0].expr, "1+1", CALC_EXPR_LEN);
    g_strlcpy(app.calc_mode.entries[0].result, "2", CALC_RESULT_LEN);
    g_strlcpy(app.calc_mode.last_result, "2", CALC_RESULT_LEN);
    app.calc_mode.count = 1;
    calc_history_save(&app.calc_mode);
    update_display_calls = 0;

    event.keyval = GDK_KEY_x;
    event.state = GDK_CONTROL_MASK;
    gboolean handled = calc_handle_key(&event, &app);

    ASSERT_TRUE("Ctrl+X handled in calc tab", handled == TRUE);
    ASSERT_TRUE("Ctrl+X clears calc history count", app.calc_mode.count == 0);
    ASSERT_TRUE("Ctrl+X clears calc last_result", app.calc_mode.last_result[0] == '\0');
    ASSERT_TRUE("Ctrl+X refreshes display", update_display_calls == 1);

    calc_history_load(&loaded);
    ASSERT_TRUE("Ctrl+X persisted empty history", loaded.count == 0);
}

static void test_ctrl_c_uses_enter_copy_path(void) {
    AppData app = {0};
    GdkEventKey event = {0};

    app.selection.window_index = 0;
    g_strlcpy(app.command_mode.command_buffer, "1+1", sizeof(app.command_mode.command_buffer));
    clipboard_set_calls = 0;
    clipboard_text[0] = '\0';

    event.keyval = GDK_KEY_c;
    event.state = GDK_CONTROL_MASK;
    gboolean handled = calc_handle_key(&event, &app);

    ASSERT_TRUE("Ctrl+C handled in calc tab", handled == TRUE);
    ASSERT_TRUE("Ctrl+C triggers clipboard copy", clipboard_set_calls == 1);
    ASSERT_STR("Ctrl+C copied evaluated result", clipboard_text, "2");
}

int main(void) {
    char home_dir[128];
    snprintf(home_dir, sizeof(home_dir), "/tmp/cofi-calc-provider-test-%ld", (long)getpid());
    mkdir(home_dir, 0755);
    setenv("HOME", home_dir, 1);

    test_error_result_column_is_wide_enough();
    test_registered_command_metadata();
    test_command_handler_enters_modal_and_evaluates_args();
    test_command_handler_records_invalid_expression();
    test_command_handler_without_args_only_enters_modal();
    test_ctrl_x_clears_and_persists_history();
    test_ctrl_c_uses_enter_copy_path();

    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
