#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/app/app_data.h"
#include "cli/cli_args.h"
#include "commands/command_api.h"
#include "config/config.h"
#include "daemon/daemon_socket.h"

static int pass = 0;
static int fail = 0;
static int stub_command_count = 0;
static int register_builtin_plugins_calls = 0;
static int generate_help_calls = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

char *generate_command_help_text(HelpFormat format, int width) {
    generate_help_calls++;
    (void)format;
    (void)width;
    return strdup("help");
}

int cofi_command_count(void) {
    return stub_command_count;
}

void cofi_command_registry_reset(void) {
    stub_command_count = 0;
}

void cofi_register_builtin_plugins(void) {
    register_builtin_plugins_calls++;
    stub_command_count = 40;
}

static void test_parse_run_flag_sets_startup_mode(void) {
    AppData app = {0};
    char *log_file = NULL;
    int log_enabled = 1;
    int alignment_specified = 0;
    int close_on_focus_loss_specified = 0;
    int log_level_specified = 0;

    init_config_defaults(&app.config);

    char *argv[] = {
        (char *)"cofi",
        (char *)"--run",
        NULL
    };

    int result = parse_command_line(2, argv, &app, &log_file, &log_enabled,
                                    &alignment_specified,
                                    &close_on_focus_loss_specified,
                                    &log_level_specified);

    ASSERT_TRUE("parse --run succeeds", result == 0);
    ASSERT_TRUE("parse --run sets start_in_run_mode", app.start_in_run_mode == 1);
    ASSERT_TRUE("parse --run leaves command mode flag unset", app.start_in_command_mode == 0);
    ASSERT_TRUE("parse --run sets delegate opcode", app.startup_delegate_opcode == COFI_OPCODE_RUN);

    free(log_file);
}

static void test_help_commands_registers_commands_before_render(void) {
    AppData app = {0};
    char *log_file = NULL;
    int log_enabled = 1;
    int alignment_specified = 0;
    int close_on_focus_loss_specified = 0;
    int log_level_specified = 0;
    char *argv[] = {
        (char *)"cofi",
        (char *)"-H",
        NULL
    };

    stub_command_count = 0;
    register_builtin_plugins_calls = 0;
    generate_help_calls = 0;
    init_config_defaults(&app.config);

    int result = parse_command_line(2, argv, &app, &log_file, &log_enabled,
                                    &alignment_specified,
                                    &close_on_focus_loss_specified,
                                    &log_level_specified);

    ASSERT_TRUE("parse -H exits with help-commands code", result == 4);
    ASSERT_TRUE("parse -H registers builtin commands when registry empty",
                register_builtin_plugins_calls == 1);
    ASSERT_TRUE("parse -H renders help once", generate_help_calls == 1);
    ASSERT_TRUE("parse -H populates command registry before rendering",
                stub_command_count > 0);

    free(log_file);
}

int main(void) {
    test_parse_run_flag_sets_startup_mode();
    test_help_commands_registers_commands_before_render();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
