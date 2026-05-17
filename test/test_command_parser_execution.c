#include <stdio.h>
#include <string.h>
#include "../src/command_parser.h"
#include "../src/cofi_tab_provider.h"

static int tests_passed = 0;
static int tests_failed = 0;

static const char *profiles_aliases[] = {"chrome", "browser", "browsers", NULL};
static const char *calc_aliases[] = {"ca", NULL};
static const char *run_aliases[] = {"r", NULL};
static const char *sinks_aliases[] = {"sink", NULL};
static const char *proc_aliases[] = {"ps", NULL};
static const char *sessions_aliases[] = {"tmux", "tx", "zj", "zellij", NULL};
static const char *workspaces_aliases[] = {"ws", NULL};
static const char *harpoon_aliases[] = {"hp", NULL};
static const char *names_aliases[] = {"nm", NULL};
static const char *rules_aliases[] = {"rl", NULL};

static void register_profiles_provider(void) {
    CofiTabProvider provider;
    cofi_init_provider_defaults(&provider);
    provider.id = "profiles";
    provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    provider.primary_cmd = "profiles";
    provider.aliases = profiles_aliases;
    provider.command_handler = (CofiCommandHandler)1;
    cofi_register_tab_provider(&provider);
}

static void register_calc_provider(void) {
    CofiTabProvider provider;
    cofi_init_provider_defaults(&provider);
    provider.id = "calc";
    provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    provider.primary_cmd = "calc";
    provider.aliases = calc_aliases;
    provider.command_handler = (CofiCommandHandler)1;
    cofi_register_tab_provider(&provider);
}

static void register_run_provider(void) {
    CofiTabProvider provider;
    cofi_init_provider_defaults(&provider);
    provider.id = "run";
    provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    provider.primary_cmd = "run";
    provider.aliases = run_aliases;
    provider.command_handler = (CofiCommandHandler)1;
    cofi_register_tab_provider(&provider);
}

static void register_sinks_provider(void) {
    CofiTabProvider provider;
    cofi_init_provider_defaults(&provider);
    provider.id = "sinks";
    provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    provider.primary_cmd = "sinks";
    provider.aliases = sinks_aliases;
    provider.command_handler = (CofiCommandHandler)1;
    cofi_register_tab_provider(&provider);
}

static void register_proc_provider(void) {
    CofiTabProvider provider;
    cofi_init_provider_defaults(&provider);
    provider.id = "proc";
    provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    provider.primary_cmd = "proc";
    provider.aliases = proc_aliases;
    provider.command_handler = (CofiCommandHandler)1;
    cofi_register_tab_provider(&provider);
}

static void register_sessions_provider(void) {
    CofiTabProvider provider;
    cofi_init_provider_defaults(&provider);
    provider.id = "sessions";
    provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    provider.primary_cmd = "sessions";
    provider.aliases = sessions_aliases;
    provider.command_handler = (CofiCommandHandler)1;
    cofi_register_tab_provider(&provider);
}

static void register_workspaces_provider(void) {
    CofiTabProvider provider;
    cofi_init_provider_defaults(&provider);
    provider.id = "workspaces";
    provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    provider.primary_cmd = "workspaces";
    provider.aliases = workspaces_aliases;
    provider.command_handler = (CofiCommandHandler)1;
    cofi_register_tab_provider(&provider);
}

static void register_harpoon_provider(void) {
    CofiTabProvider provider;
    cofi_init_provider_defaults(&provider);
    provider.id = "harpoon";
    provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    provider.primary_cmd = "harpoon";
    provider.aliases = harpoon_aliases;
    provider.command_handler = (CofiCommandHandler)1;
    cofi_register_tab_provider(&provider);
}

static void register_names_provider(void) {
    CofiTabProvider provider;
    cofi_init_provider_defaults(&provider);
    provider.id = "names";
    provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    provider.primary_cmd = "names";
    provider.aliases = names_aliases;
    provider.command_handler = (CofiCommandHandler)1;
    cofi_register_tab_provider(&provider);
}

static void register_rules_provider(void) {
    CofiTabProvider provider;
    cofi_init_provider_defaults(&provider);
    provider.id = "rules";
    provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    provider.primary_cmd = "rules";
    provider.aliases = rules_aliases;
    provider.command_handler = (CofiCommandHandler)1;
    cofi_register_tab_provider(&provider);
}

static void assert_true(const char *name, int condition) {
    if (condition) {
        printf("PASS: %s\n", name);
        tests_passed++;
    } else {
        printf("FAIL: %s\n", name);
        tests_failed++;
    }
}

static void test_parse_command_for_execution_alias_resolution(void) {
    char cmd[64] = {0};
    char arg[64] = {0};

    assert_true("change-workspace resolves to cw",
                parse_command_for_execution("change-workspace 3", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "cw") == 0 && strcmp(arg, "3") == 0);

    assert_true("compact j5 resolves to jw",
                parse_command_for_execution("j5", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "jw") == 0 && strcmp(arg, "5") == 0);

    assert_true("compact js1 resolves to jump-slot",
                parse_command_for_execution("js1", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "jump-slot") == 0 && strcmp(arg, "1") == 0);

    assert_true("jump-slot 9 resolves to jump-slot",
                parse_command_for_execution("jump-slot 9", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "jump-slot") == 0 && strcmp(arg, "9") == 0);

    assert_true("tile alias tL resolves to tw",
                parse_command_for_execution("tL", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "tw") == 0 && strcmp(arg, "L") == 0);

    assert_true("hotkey alias resolves to hotkeys",
                parse_command_for_execution("hotkey Mod4+w show windows", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "hotkeys") == 0 && strcmp(arg, "Mod4+w show windows") == 0);

    assert_true("provider alias chrome resolves to profiles",
                parse_command_for_execution("chrome gs", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "profiles") == 0 && strcmp(arg, "gs") == 0);

    assert_true("provider alias ca resolves to calc",
                parse_command_for_execution("ca 1+1", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "calc") == 0 && strcmp(arg, "1+1") == 0);

    assert_true("provider alias r resolves to run",
                parse_command_for_execution("r xterm", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "run") == 0 && strcmp(arg, "xterm") == 0);

    assert_true("provider alias sink resolves to sinks",
                parse_command_for_execution("sink headphones", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "sinks") == 0 && strcmp(arg, "headphones") == 0);

    assert_true("provider alias ps resolves to proc",
                parse_command_for_execution("ps firefox", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "proc") == 0 && strcmp(arg, "firefox") == 0);

    assert_true("provider alias zellij resolves to sessions",
                parse_command_for_execution("zellij work api", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "sessions") == 0 && strcmp(arg, "work api") == 0);

    assert_true("provider alias ws resolves to workspaces",
                parse_command_for_execution("ws", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "workspaces") == 0 && strcmp(arg, "") == 0);

    assert_true("provider alias hp resolves to harpoon",
                parse_command_for_execution("hp", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "harpoon") == 0 && strcmp(arg, "") == 0);

    assert_true("provider alias nm resolves to names",
                parse_command_for_execution("nm", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "names") == 0 && strcmp(arg, "") == 0);

    assert_true("provider alias rl resolves to rules",
                parse_command_for_execution("rl", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "rules") == 0 && strcmp(arg, "") == 0);
}

static void test_next_command_segment(void) {
    char chain[128] = "  cw1,  tL , , jw2  ";
    char *cursor = chain;
    char segment[64] = {0};

    assert_true("first segment cw1", next_command_segment(&cursor, segment, sizeof(segment)) && strcmp(segment, "cw1") == 0);
    assert_true("second segment tL", next_command_segment(&cursor, segment, sizeof(segment)) && strcmp(segment, "tL") == 0);
    assert_true("third segment jw2", next_command_segment(&cursor, segment, sizeof(segment)) && strcmp(segment, "jw2") == 0);
    assert_true("no more segments", !next_command_segment(&cursor, segment, sizeof(segment)));
}

int main(void) {
    printf("Command parser execution-path tests\n");
    printf("===================================\n\n");

    cofi_registry_reset();
    register_profiles_provider();
    register_calc_provider();
    register_run_provider();
    register_sinks_provider();
    register_proc_provider();
    register_sessions_provider();
    register_workspaces_provider();
    register_harpoon_provider();
    register_names_provider();
    register_rules_provider();

    test_parse_command_for_execution_alias_resolution();
    test_next_command_segment();

    printf("\n===================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);
    return tests_failed ? 1 : 0;
}
