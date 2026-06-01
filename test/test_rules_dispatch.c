#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "core/utils/utils.h"
#include "rules/rules_dispatch.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

static int g_exec_calls;
static char g_exec_log[16][128];

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
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

static int add_dispatch_rule(AppData *app, const char *pattern, const char *commands,
                             bool run_at_start, bool new_only) {
    int rule_index = app->rules_config.count++;
    Rule *rule = &app->rules_config.rules[rule_index];
    memset(rule, 0, sizeof(*rule));
    safe_string_copy(rule->pattern, pattern, sizeof(rule->pattern));
    safe_string_copy(rule->commands, commands, sizeof(rule->commands));
    rule->run_at_start = run_at_start;
    rule->new_only = new_only;
    rule->once = false;
    rule->match_id = matching_create_pattern_entry(&app->matching, pattern);
    return rule_index;
}

static void init_dispatch_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    match_entry_manager_init(&app->matching);
    init_rule_state(&app->rule_state);
    init_rule_breaker(&app->rule_breaker);
}

static void add_window(AppData *app, Window id, const char *title) {
    WindowInfo *window = &app->windows[app->window_count++];
    memset(window, 0, sizeof(*window));
    window->id = id;
    safe_string_copy(window->title, title, sizeof(window->title));
}

static void test_rules_apply_client_list_uses_new_window_gate(void) {
    AppData app;
    init_dispatch_app(&app);
    app.initial_window_population_done = TRUE;
    add_window(&app, 0x100, "Terminal");
    add_window(&app, 0x200, "Firefox");
    add_dispatch_rule(&app, "*Terminal*", "terminal-cmd", false, false);
    add_dispatch_rule(&app, "*Firefox*", "firefox-new", false, true);

    Window added[] = {0x200};
    reset_exec_log();
    rules_apply(&app, RULE_TRIGGER_CLIENT_LIST, added, 1);

    ASSERT_TRUE("client-list dispatch executes matching normal rule",
                strcmp(g_exec_log[0], "terminal-cmd@0x100") == 0);
    ASSERT_TRUE("client-list dispatch executes new-only rule for added window",
                strcmp(g_exec_log[1], "firefox-new@0x200") == 0);
    ASSERT_TRUE("client-list dispatch call count", g_exec_calls == 2);
}

static void test_rules_apply_startup_suppresses_non_startup_rules(void) {
    AppData app;
    init_dispatch_app(&app);
    app.initial_window_population_done = FALSE;
    add_window(&app, 0x100, "Terminal");
    add_dispatch_rule(&app, "*Terminal*", "no-startup", false, false);
    add_dispatch_rule(&app, "*Terminal*", "startup", true, false);

    reset_exec_log();
    rules_apply(&app, RULE_TRIGGER_STARTUP, NULL, 0);

    ASSERT_TRUE("startup suppresses non-run_at_start rule", g_exec_calls == 1);
    ASSERT_TRUE("startup dispatches run_at_start rule",
                strcmp(g_exec_log[0], "startup@0x100") == 0);
}

static void test_rules_apply_reentry_guard_is_noop(void) {
    AppData app;
    init_dispatch_app(&app);
    app.initial_window_population_done = TRUE;
    app.in_rule_dispatch = TRUE;
    add_window(&app, 0x100, "Terminal");
    add_dispatch_rule(&app, "*Terminal*", "cmd", false, false);

    reset_exec_log();
    rules_apply(&app, RULE_TRIGGER_CLIENT_LIST, NULL, 0);

    ASSERT_TRUE("rules_apply re-entry guard skips dispatch", g_exec_calls == 0);
}

static void test_rules_apply_for_title_change_dispatches_matching_window(void) {
    AppData app;
    init_dispatch_app(&app);
    app.initial_window_population_done = TRUE;
    add_window(&app, 0x100, "Terminal - htop");
    add_window(&app, 0x200, "Firefox");
    add_dispatch_rule(&app, "*htop*", "htop-cmd", false, false);

    reset_exec_log();
    rules_apply_for_title_change(&app, 0x100);

    ASSERT_TRUE("title-change dispatch fires matching rule", g_exec_calls == 1);
    ASSERT_TRUE("title-change dispatch targets requested window",
                strcmp(g_exec_log[0], "htop-cmd@0x100") == 0);
}

static void test_rules_apply_for_title_change_ignores_nonmatching_window(void) {
    AppData app;
    init_dispatch_app(&app);
    app.initial_window_population_done = TRUE;
    add_window(&app, 0x100, "Terminal");
    add_dispatch_rule(&app, "*Firefox*", "firefox-cmd", false, false);

    reset_exec_log();
    rules_apply_for_title_change(&app, 0x100);

    ASSERT_TRUE("title-change dispatch skips nonmatching title", g_exec_calls == 0);
}

static void test_rules_apply_for_title_change_reentry_guard_is_noop(void) {
    AppData app;
    init_dispatch_app(&app);
    app.initial_window_population_done = TRUE;
    app.in_rule_dispatch = TRUE;
    add_window(&app, 0x100, "Terminal - htop");
    add_dispatch_rule(&app, "*htop*", "htop-cmd", false, false);

    reset_exec_log();
    rules_apply_for_title_change(&app, 0x100);

    ASSERT_TRUE("title-change re-entry guard skips dispatch", g_exec_calls == 0);
    ASSERT_TRUE("title-change re-entry guard does not seed match state",
                app.rule_state.count == 0);
}

int main(void) {
    printf("Rules dispatch tests\n");
    printf("====================\n\n");

    test_rules_apply_client_list_uses_new_window_gate();
    test_rules_apply_startup_suppresses_non_startup_rules();
    test_rules_apply_reentry_guard_is_noop();
    test_rules_apply_for_title_change_dispatches_matching_window();
    test_rules_apply_for_title_change_ignores_nonmatching_window();
    test_rules_apply_for_title_change_reentry_guard_is_noop();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
