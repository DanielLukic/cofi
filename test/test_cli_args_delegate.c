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

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

char *generate_command_help_text(HelpFormat format, int width) {
    (void)format;
    (void)width;
    return strdup("help");
}

int cofi_command_count(void) {
    return 1;
}

void cofi_command_registry_reset(void) {
}

void cofi_register_builtin_plugins(void) {
}

static int parse_args(AppData *app, int argc, char **argv) {
    char *log_file = NULL;
    int log_enabled = 1;
    int alignment_specified = 0;
    int close_on_focus_loss_specified = 0;
    int log_level_specified = 0;

    init_config_defaults(&app->config);
    int rc = parse_command_line(argc, argv, app, &log_file, &log_enabled,
                                &alignment_specified,
                                &close_on_focus_loss_specified,
                                &log_level_specified);
    free(log_file);
    return rc;
}

static void test_windows_flag_sets_delegate_opcode(void) {
    AppData app = {0};
    char *argv[] = {(char *)"cofi", (char *)"--windows", NULL};

    int rc = parse_args(&app, 2, argv);
    ASSERT_TRUE("parse --windows succeeds", rc == 0);
    ASSERT_TRUE("--windows maps to opcode windows", app.startup_delegate_opcode == COFI_OPCODE_WINDOWS);
    ASSERT_TRUE("--windows keeps tab windows", app.current_tab == TAB_WINDOWS);
}

static void test_command_flag_sets_command_mode_delegate(void) {
    AppData app = {0};
    char *argv[] = {(char *)"cofi", (char *)"--command", NULL};

    int rc = parse_args(&app, 2, argv);
    ASSERT_TRUE("parse --command succeeds", rc == 0);
    ASSERT_TRUE("--command maps to opcode command", app.startup_delegate_opcode == COFI_OPCODE_COMMAND);
    ASSERT_TRUE("--command sets start_in_command_mode", app.start_in_command_mode == 1);
    ASSERT_TRUE("--command clears run mode", app.start_in_run_mode == 0);
}

static void test_last_delegate_flag_wins(void) {
    AppData app = {0};
    char *argv[] = {(char *)"cofi", (char *)"--workspaces", (char *)"--names", NULL};

    int rc = parse_args(&app, 3, argv);
    ASSERT_TRUE("parse multi-delegate succeeds", rc == 0);
    ASSERT_TRUE("last delegate flag wins", app.startup_delegate_opcode == COFI_OPCODE_NAMES);
    ASSERT_TRUE("last delegate leaves startup tab at windows", app.current_tab == TAB_WINDOWS);
}

static void test_delegate_flags_prepare_startup_mode_when_becoming_daemon(void) {
    struct {
        const char *flag;
        uint8_t opcode;
        TabMode tab;
        int command_mode;
        int run_mode;
    } cases[] = {
        {"--windows", COFI_OPCODE_WINDOWS, TAB_WINDOWS, 0, 0},
        {"--workspaces", COFI_OPCODE_WORKSPACES, TAB_WINDOWS, 0, 0},
        {"--harpoon", COFI_OPCODE_HARPOON, TAB_WINDOWS, 0, 0},
        {"--names", COFI_OPCODE_NAMES, TAB_WINDOWS, 0, 0},
        {"--applications", COFI_OPCODE_APPLICATIONS, TAB_WINDOWS, 0, 0},
        {"--command", COFI_OPCODE_COMMAND, TAB_WINDOWS, 1, 0},
        {"--run", COFI_OPCODE_RUN, TAB_WINDOWS, 0, 1}
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        AppData app = {0};
        char *argv[] = {(char *)"cofi", (char *)cases[i].flag, NULL};

        int rc = parse_args(&app, 2, argv);

        char label[160];
        snprintf(label, sizeof(label), "%s parse succeeds", cases[i].flag);
        ASSERT_TRUE(label, rc == 0);

        snprintf(label, sizeof(label), "%s startup opcode set", cases[i].flag);
        ASSERT_TRUE(label, app.startup_delegate_opcode == cases[i].opcode &&
                           app.startup_delegate_opcode != COFI_OPCODE_RESERVED);

        snprintf(label, sizeof(label), "%s startup tab set", cases[i].flag);
        ASSERT_TRUE(label, app.current_tab == cases[i].tab);

        snprintf(label, sizeof(label), "%s command-mode startup flag", cases[i].flag);
        ASSERT_TRUE(label, app.start_in_command_mode == cases[i].command_mode);

        snprintf(label, sizeof(label), "%s run-mode startup flag", cases[i].flag);
        ASSERT_TRUE(label, app.start_in_run_mode == cases[i].run_mode);
    }
}

static void test_cli_flag_to_opcode_round_trip_names(void) {
    struct {
        const char *flag;
        uint8_t opcode;
        const char *name;
    } cases[] = {
        {"--windows", COFI_OPCODE_WINDOWS, "windows"},
        {"--workspaces", COFI_OPCODE_WORKSPACES, "workspaces"},
        {"--harpoon", COFI_OPCODE_HARPOON, "harpoon"},
        {"--matching", COFI_OPCODE_MATCHING, "matching"},
        {"--names", COFI_OPCODE_NAMES, "names"},
        {"--command", COFI_OPCODE_COMMAND, "command"},
        {"--run", COFI_OPCODE_RUN, "run"},
        {"--applications", COFI_OPCODE_APPLICATIONS, "applications"}
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        AppData app = {0};
        char *argv[] = {(char *)"cofi", (char *)cases[i].flag, NULL};

        int rc = parse_args(&app, 2, argv);
        char label[128];
        snprintf(label, sizeof(label), "%s maps to expected opcode", cases[i].flag);
        ASSERT_TRUE(label, rc == 0 && app.startup_delegate_opcode == cases[i].opcode);

        snprintf(label, sizeof(label), "%s opcode name roundtrip", cases[i].flag);
        ASSERT_TRUE(label, strcmp(daemon_socket_opcode_name(app.startup_delegate_opcode),
                                  cases[i].name) == 0);
    }
}

static void test_applications_flag_resets_apps_mode(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    app.apps_mode = APPS_MODE_PATH;

    char *argv[] = {(char *)"cofi", (char *)"--applications", NULL};
    parse_args(&app, 2, argv);

    ASSERT_TRUE("--applications resets apps_mode to DEFAULT",
                app.apps_mode == APPS_MODE_DEFAULT);
}

static void test_show_flag_sets_delegate_and_name(void) {
    AppData app = {0};
    char *argv[] = {(char *)"cofi", (char *)"--show", (char *)"emoji", NULL};

    int rc = parse_args(&app, 3, argv);
    ASSERT_TRUE("parse --show succeeds", rc == 0);
    ASSERT_TRUE("--show maps to opcode show-tab", app.startup_delegate_opcode == COFI_OPCODE_SHOW_TAB);
    ASSERT_TRUE("--show stores tab name", strcmp(app.startup_delegate_tab_name, "emoji") == 0);
}

static void test_show_unknown_still_sets_delegate_and_name(void) {
    AppData app = {0};
    char *argv[] = {(char *)"cofi", (char *)"--show", (char *)"unknown", NULL};

    int rc = parse_args(&app, 3, argv);
    ASSERT_TRUE("parse --show unknown succeeds", rc == 0);
    ASSERT_TRUE("--show unknown keeps show opcode", app.startup_delegate_opcode == COFI_OPCODE_SHOW_TAB);
    ASSERT_TRUE("--show unknown preserves name", strcmp(app.startup_delegate_tab_name, "unknown") == 0);
}

static void test_show_then_legacy_flag_legacy_wins(void) {
    AppData app = {0};
    char *argv[] = {(char *)"cofi", (char *)"--show", (char *)"emoji", (char *)"--applications", NULL};

    int rc = parse_args(&app, 4, argv);
    ASSERT_TRUE("parse --show then legacy succeeds", rc == 0);
    ASSERT_TRUE("legacy delegate wins after --show", app.startup_delegate_opcode == COFI_OPCODE_APPLICATIONS);
    ASSERT_TRUE("legacy delegate clears show name", app.startup_delegate_tab_name[0] == '\0');
}

int main(void) {
    test_windows_flag_sets_delegate_opcode();
    test_command_flag_sets_command_mode_delegate();
    test_last_delegate_flag_wins();
    test_delegate_flags_prepare_startup_mode_when_becoming_daemon();
    test_cli_flag_to_opcode_round_trip_names();
    test_applications_flag_resets_apps_mode();
    test_show_flag_sets_delegate_and_name();
    test_show_unknown_still_sets_delegate_and_name();
    test_show_then_legacy_flag_legacy_wins();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
