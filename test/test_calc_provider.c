#include <stdio.h>
#include <string.h>

#include "../src/calc_provider.c"

static int pass = 0;
static int fail = 0;
static int exit_command_mode_calls = 0;
static int enter_modal_calls = 0;
static const CofiTabProvider *last_modal_provider = NULL;

void exit_command_mode(AppData *app) {
    (void)app;
    exit_command_mode_calls++;
}

void cofi_enter_modal(AppData *app, const CofiTabProvider *provider) {
    (void)app;
    enter_modal_calls++;
    last_modal_provider = provider;
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

    const CofiTabProvider *provider = cofi_get_provider_for_command("ca");
    ASSERT_TRUE("calc command alias resolves to provider", provider != NULL);
    ASSERT_STR("calc command primary", provider->primary_cmd, "calc");
    ASSERT_STR("calc command help", provider->command_help_format, "calc, ca");
    ASSERT_STR("calc command description", provider->command_description, "Switch to calculator");
    ASSERT_TRUE("calc command handler registered", provider->command_handler != NULL);
    ASSERT_TRUE("calc command keeps open", provider->command_keeps_open_on_hotkey_auto == 1);
}

static void test_command_handler_enters_modal_and_evaluates_args(void) {
    AppData app = {0};
    cofi_registry_reset();
    calc_provider_register();

    const CofiTabProvider *provider = cofi_get_provider_for_command("calc");
    ASSERT_TRUE("calc provider registered for command handler test", provider != NULL);
    if (!provider || !provider->command_handler) return;

    exit_command_mode_calls = 0;
    enter_modal_calls = 0;
    last_modal_provider = NULL;

    gboolean result = provider->command_handler(&app, NULL, "1+1");

    ASSERT_TRUE("calc command returns false", result == FALSE);
    ASSERT_TRUE("calc command exits command mode", exit_command_mode_calls == 1);
    ASSERT_TRUE("calc command enters modal", enter_modal_calls == 1);
    ASSERT_TRUE("calc command uses calc provider modal", last_modal_provider == provider);
    ASSERT_TRUE("calc command sets prefix claim", app.active_prefix_claim == '=');
    ASSERT_TRUE("calc command evaluates args", app.calc_mode.count == 1);
    ASSERT_STR("calc command stores result", app.calc_mode.entries[0].result, "2");
}

static void test_command_handler_records_invalid_expression(void) {
    AppData app = {0};
    cofi_registry_reset();
    calc_provider_register();

    const CofiTabProvider *provider = cofi_get_provider_for_command("calc");
    ASSERT_TRUE("calc provider registered for invalid command test", provider != NULL);
    if (!provider || !provider->command_handler) return;

    provider->command_handler(&app, NULL, "foo");

    ASSERT_TRUE("invalid calc command pushes history", app.calc_mode.count == 1);
    ASSERT_STR("invalid calc command stores expression", app.calc_mode.entries[0].expr, "foo");
    ASSERT_STR("invalid calc command stores error result",
               app.calc_mode.entries[0].result, "[error: bad expression]");
}

static void test_command_handler_without_args_only_enters_modal(void) {
    AppData app = {0};
    cofi_registry_reset();
    calc_provider_register();

    const CofiTabProvider *provider = cofi_get_provider_for_command("calc");
    ASSERT_TRUE("calc provider registered for empty command test", provider != NULL);
    if (!provider || !provider->command_handler) return;

    exit_command_mode_calls = 0;
    enter_modal_calls = 0;

    provider->command_handler(&app, NULL, "");

    ASSERT_TRUE("calc command without args exits command mode", exit_command_mode_calls == 1);
    ASSERT_TRUE("calc command without args enters modal", enter_modal_calls == 1);
    ASSERT_TRUE("calc command without args does not evaluate", app.calc_mode.count == 0);
}

int main(void) {
    test_error_result_column_is_wide_enough();
    test_registered_command_metadata();
    test_command_handler_enters_modal_and_evaluates_args();
    test_command_handler_records_invalid_expression();
    test_command_handler_without_args_only_enters_modal();

    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
