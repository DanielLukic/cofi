#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/cli_args.h"
#include "../src/command_api.h"
#include "../src/config.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

char *generate_command_help_text(HelpFormat format) {
    (void)format;
    return strdup("help");
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

    free(log_file);
}

int main(void) {
    test_parse_run_flag_sets_startup_mode();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
