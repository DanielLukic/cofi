#include <stdio.h>
#include <string.h>
#include "commands/command_parser.h"
#include "commands/command_registry.h"
#include "commands/core_commands.h"

static int tests_passed = 0;
static int tests_failed = 0;

static gboolean noop_handler(AppData *app, WindowInfo *window, const char *args) {
    (void)app;
    (void)window;
    (void)args;
    return TRUE;
}

static const CommandSpec s_provider_commands[] = {
    {.primary = "sessions", .aliases = {"session", NULL}, .owner_provider_id = "sessions", .handler = noop_handler},
    {.primary = "profiles", .aliases = {"chrome", "browser", "browsers", NULL}, .owner_provider_id = "profiles", .handler = noop_handler},
    {.primary = "calc", .aliases = {"ca", NULL}, .owner_provider_id = "calc", .handler = noop_handler},
    {.primary = "run", .aliases = {"r", NULL}, .owner_provider_id = "run", .handler = noop_handler},
    {.primary = "sinks", .aliases = {"sink", NULL}, .owner_provider_id = "sinks", .handler = noop_handler},
    {.primary = "proc", .aliases = {"ps", NULL}, .owner_provider_id = "proc", .handler = noop_handler},
    {.primary = "projects", .aliases = {"project", "tmux", "tx", "zj", "zellij"}, .owner_provider_id = "projects", .handler = noop_handler},
    {.primary = "workspaces", .aliases = {"ws", NULL}, .owner_provider_id = "workspaces", .handler = noop_handler},
    {.primary = "harpoon", .aliases = {"hp", NULL}, .owner_provider_id = "harpoon", .handler = noop_handler},
    {.primary = "names", .aliases = {"nm", NULL}, .owner_provider_id = "names", .handler = noop_handler},
    {.primary = "rules", .aliases = {"rs", NULL}, .owner_provider_id = "rules", .handler = noop_handler},
    {.primary = "config", .aliases = {"conf", "cfg", NULL}, .owner_provider_id = "config", .handler = noop_handler},
    {.primary = "hotkeys", .aliases = {"hotkey", "hk", NULL}, .owner_provider_id = "hotkeys", .handler = noop_handler},
    {.primary = "apps", .aliases = {"applications", "app", NULL}, .owner_provider_id = "apps", .handler = noop_handler},
};

static void register_provider_commands(void) {
    for (size_t i = 0; i < sizeof(s_provider_commands) / sizeof(s_provider_commands[0]); i++) {
        cofi_register_command(&s_provider_commands[i]);
    }
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

    assert_true("compact tm0 resolves to tm",
                parse_command_for_execution("tm0", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "tm") == 0 && strcmp(arg, "0") == 0);

    assert_true("compact mw+ resolves to mw",
                parse_command_for_execution("mw+", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "mw") == 0 && strcmp(arg, "+") == 0);

    assert_true("hotkey alias resolves to hotkeys",
                parse_command_for_execution("hotkey Mod4+w show windows", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "hotkeys") == 0 && strcmp(arg, "Mod4+w show windows") == 0);

    assert_true("hk alias resolves to hotkeys",
                parse_command_for_execution("hk Mod4+w show windows", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "hotkeys") == 0 && strcmp(arg, "Mod4+w show windows") == 0);

    assert_true("app alias resolves to apps",
                parse_command_for_execution("app fire", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "apps") == 0 && strcmp(arg, "fire") == 0);

    assert_true("session alias resolves to sessions",
                parse_command_for_execution("session marco | restart", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "sessions") == 0 && strcmp(arg, "marco | restart") == 0);

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

    assert_true("provider alias zellij resolves to projects",
                parse_command_for_execution("zellij work api", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "projects") == 0 && strcmp(arg, "work api") == 0);

    assert_true("provider alias ws resolves to workspaces",
                parse_command_for_execution("ws", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "workspaces") == 0 && strcmp(arg, "") == 0);

    assert_true("provider alias hp resolves to harpoon",
                parse_command_for_execution("hp", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "harpoon") == 0 && strcmp(arg, "") == 0);

    assert_true("provider alias nm resolves to names",
                parse_command_for_execution("nm", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "names") == 0 && strcmp(arg, "") == 0);

    assert_true("provider alias rs resolves to rules",
                parse_command_for_execution("rs", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "rules") == 0 && strcmp(arg, "") == 0);

    assert_true("provider alias conf resolves to config",
                parse_command_for_execution("conf", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "config") == 0 && strcmp(arg, "") == 0);

    assert_true("provider alias cfg resolves to config",
                parse_command_for_execution("cfg", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "config") == 0 && strcmp(arg, "") == 0);
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

typedef struct {
    int seen;
    int fail_at;
    const char *expected[8];
} SegmentVisitState;

static gboolean record_segment_visit(const char *segment, void *user_data) {
    SegmentVisitState *state = user_data;
    if (state->expected[state->seen] && strcmp(segment, state->expected[state->seen]) != 0) {
        return FALSE;
    }

    state->seen++;
    if (state->fail_at > 0 && state->seen >= state->fail_at) {
        return FALSE;
    }
    return TRUE;
}

static void test_visit_command_segments(void) {
    SegmentVisitState order_state = {
        .seen = 0,
        .fail_at = 0,
        .expected = {"tm0", "mw+", "jw2", NULL},
    };

    gboolean ok = visit_command_segments("  tm0, , mw+ , jw2  ",
                                         record_segment_visit,
                                         &order_state);
    assert_true("visit segments in stored order", ok && order_state.seen == 3);

    SegmentVisitState stop_state = {
        .seen = 0,
        .fail_at = 2,
        .expected = {"tm0", "mw+", "jw2", NULL},
    };

    ok = visit_command_segments("tm0,mw+,jw2", record_segment_visit, &stop_state);
    assert_true("visit stops on first visitor failure", !ok && stop_state.seen == 2);
}

typedef struct {
    const char *segment;
    const char *primary;
    const char *arg;
} ExpectedResolvedSegment;

typedef struct {
    const ExpectedResolvedSegment *expected;
    int count;
    int seen;
} ResolveChainState;

static gboolean verify_resolved_segment(const char *segment, void *user_data) {
    ResolveChainState *state = user_data;
    if (state->seen >= state->count) {
        return FALSE;
    }

    const ExpectedResolvedSegment *expected = &state->expected[state->seen];
    char primary[64] = {0};
    char arg[64] = {0};

    if (strcmp(segment, expected->segment) != 0) {
        return FALSE;
    }
    if (!parse_command_for_execution(segment, primary, arg, sizeof(primary), sizeof(arg))) {
        return FALSE;
    }
    if (strcmp(primary, expected->primary) != 0 || strcmp(arg, expected->arg) != 0) {
        return FALSE;
    }

    state->seen++;
    return TRUE;
}

static void assert_rule_chain(const char *label, const char *command,
                              const ExpectedResolvedSegment *expected,
                              int count) {
    ResolveChainState state = {
        .expected = expected,
        .count = count,
        .seen = 0,
    };

    gboolean ok = visit_command_segments(command, verify_resolved_segment, &state);
    assert_true(label, ok && state.seen == count);
}

static void test_tfd826_compound_rule_examples(void) {
    const ExpectedResolvedSegment tm0_mw_on[] = {
        {.segment = "tm0", .primary = "tm", .arg = "0"},
        {.segment = "mw+", .primary = "mw", .arg = "+"},
    };
    assert_rule_chain("TFD-826 tm0,mw+ parses ordered segments",
                      "tm0,mw+", tm0_mw_on, 2);

    const ExpectedResolvedSegment tm1_mw_on[] = {
        {.segment = "tm1", .primary = "tm", .arg = "1"},
        {.segment = "mw+", .primary = "mw", .arg = "+"},
    };
    assert_rule_chain("TFD-826 tm1,mw+ parses ordered segments",
                      "tm1,mw+", tm1_mw_on, 2);

    const ExpectedResolvedSegment tm1_tile_left_sticky_on[] = {
        {.segment = "tm1", .primary = "tm", .arg = "1"},
        {.segment = "tl", .primary = "tw", .arg = "l"},
        {.segment = "ew+", .primary = "ew", .arg = "+"},
    };
    assert_rule_chain("TFD-826 tm1,tl,ew+ parses ordered segments",
                      "tm1,tl,ew+", tm1_tile_left_sticky_on, 3);

    const ExpectedResolvedSegment mw_off_tm1[] = {
        {.segment = "mw-", .primary = "mw", .arg = "-"},
        {.segment = "tm1", .primary = "tm", .arg = "1"},
    };
    assert_rule_chain("TFD-826 mw-,tm1 parses reverse-order segments",
                      "mw-,tm1", mw_off_tm1, 2);
}

int main(void) {
    printf("Command parser execution-path tests\n");
    printf("===================================\n\n");

    cofi_command_registry_reset();
    cofi_register_core_commands();
    register_provider_commands();

    test_parse_command_for_execution_alias_resolution();
    test_next_command_segment();
    test_visit_command_segments();
    test_tfd826_compound_rule_examples();

    printf("\n===================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);
    return tests_failed ? 1 : 0;
}
