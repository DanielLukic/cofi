#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/rules_replay.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

static int g_exec_calls;
static char g_exec_log[16][128];

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

gboolean execute_command_background(const char *command, AppData *app, WindowInfo *window) {
    (void)app;
    if (g_exec_calls < 16) {
        snprintf(g_exec_log[g_exec_calls], sizeof(g_exec_log[g_exec_calls]),
                 "%s@0x%lx", command, window ? window->id : 0);
    }
    g_exec_calls++;
    return TRUE;
}

static void reset_exec_log(void) {
    g_exec_calls = 0;
    memset(g_exec_log, 0, sizeof(g_exec_log));
}

static void test_replay_selected_rule_matches_all_windows(void) {
    AppData app;
    memset(&app, 0, sizeof(app));

    app.window_count = 3;
    app.windows[0].id = 0x100;
    strcpy(app.windows[0].title, "Terminal - htop");
    app.windows[1].id = 0x200;
    strcpy(app.windows[1].title, "Firefox");
    app.windows[2].id = 0x300;
    strcpy(app.windows[2].title, "Terminal - logs");

    app.rules_config.count = 1;
    strcpy(app.rules_config.rules[0].pattern, "*Terminal*");
    strcpy(app.rules_config.rules[0].commands, "sb on");

    app.filtered_rules_count = 1;
    app.filtered_rule_indices[0] = 0;
    app.selection.rules_index = 0;

    reset_exec_log();
    gboolean handled = replay_selected_filtered_rule(&app);

    ASSERT_TRUE("selected replay handled", handled == TRUE);
    ASSERT_TRUE("selected replay executes for all matching windows", g_exec_calls == 2);
    ASSERT_TRUE("selected replay first match", strcmp(g_exec_log[0], "sb on@0x100") == 0);
    ASSERT_TRUE("selected replay second match", strcmp(g_exec_log[1], "sb on@0x300") == 0);
}

static void test_replay_all_rules_uses_stored_order(void) {
    AppData app;
    memset(&app, 0, sizeof(app));

    app.window_count = 2;
    app.windows[0].id = 0x111;
    strcpy(app.windows[0].title, "Terminal");
    app.windows[1].id = 0x222;
    strcpy(app.windows[1].title, "Firefox");

    app.rules_config.count = 2;
    strcpy(app.rules_config.rules[0].pattern, "*Terminal*");
    strcpy(app.rules_config.rules[0].commands, "first");
    strcpy(app.rules_config.rules[1].pattern, "*Firefox*");
    strcpy(app.rules_config.rules[1].commands, "second");

    reset_exec_log();
    int replayed = replay_all_rules_against_open_windows(&app);

    ASSERT_TRUE("replay all returns match count", replayed == 2);
    ASSERT_TRUE("replay all first execution uses first rule", strcmp(g_exec_log[0], "first@0x111") == 0);
    ASSERT_TRUE("replay all second execution uses second rule", strcmp(g_exec_log[1], "second@0x222") == 0);
}

static void test_replay_does_not_mutate_transition_rule_state(void) {
    AppData app;
    memset(&app, 0, sizeof(app));

    app.window_count = 1;
    app.windows[0].id = 0x777;
    strcpy(app.windows[0].title, "Terminal");

    app.rules_config.count = 1;
    strcpy(app.rules_config.rules[0].pattern, "*Terminal*");
    strcpy(app.rules_config.rules[0].commands, "sb on");

    app.rule_state.count = 3;
    app.rule_state.windows[0].id = 0x1;
    app.rule_state.windows[0].matched = true;

    reset_exec_log();
    replay_all_rules_against_open_windows(&app);

    ASSERT_TRUE("rule replay keeps rule_state.count unchanged", app.rule_state.count == 3);
    ASSERT_TRUE("rule replay keeps existing rule_state entry unchanged", app.rule_state.windows[0].id == 0x1 && app.rule_state.windows[0].matched == true);
}

int main(void) {
    printf("Rules replay tests\n");
    printf("==================\n\n");

    test_replay_selected_rule_matches_all_windows();
    test_replay_all_rules_uses_stored_order();
    test_replay_does_not_mutate_transition_rule_state();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
