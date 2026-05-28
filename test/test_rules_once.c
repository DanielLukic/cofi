#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "rules/rules.h"
#include "rules/rules_replay.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

gboolean execute_command_background(const char *command, AppData *app, WindowInfo *window) {
    (void)command;
    (void)app;
    (void)window;
    return TRUE;
}

int rules_selected_config_index(AppData *app) {
    if (!app || app->filtered_rules_count <= 0) return -1;
    return app->filtered_rule_indices[app->selection.provider_index];
}

static void seed_window(WindowInfo *w, Window id, const char *title) {
    memset(w, 0, sizeof(*w));
    w->id = id;
    g_strlcpy(w->title, title, sizeof(w->title));
    g_strlcpy(w->class_name, "ClassA", sizeof(w->class_name));
    g_strlcpy(w->instance, "instA", sizeof(w->instance));
    g_strlcpy(w->type, "Normal", sizeof(w->type));
}

static void seed_rule(AppData *app, int idx, const char *pattern, const char *commands) {
    Rule *rule = &app->rules_config.rules[idx];
    if (app->rules_config.count <= idx) app->rules_config.count = idx + 1;
    g_strlcpy(rule->pattern, pattern, sizeof(rule->pattern));
    g_strlcpy(rule->commands, commands, sizeof(rule->commands));
    rule->match_id = matching_find_or_create_pattern_entry(&app->matching, pattern);
    rule->once = true;
    rule->applied = 0;
}

static void test_fires_once_then_suppresses(void) {
    AppData app;
    RuleState state;
    WindowInfo w;
    memset(&app, 0, sizeof(app));
    init_rule_state(&state);
    match_entry_manager_init(&app.matching);
    seed_window(&w, 0x1234, "Terminal");
    seed_rule(&app, 0, "*Terminal*", "sb on");

    RuleMatch first = check_rule_match(&app.rules_config.rules[0], &state, 0, &app.matching, &w);
    RuleMatch second = check_rule_match(&app.rules_config.rules[0], &state, 0, &app.matching, &w);

    ASSERT_TRUE("first fire allowed", first.should_fire == true);
    ASSERT_TRUE("second fire suppressed", second.should_fire == false);
}

static void test_clear_on_window_close_rearms(void) {
    AppData app;
    RuleState state;
    WindowInfo w;
    Window live_ids[1];
    memset(&app, 0, sizeof(app));
    init_rule_state(&state);
    match_entry_manager_init(&app.matching);
    seed_window(&w, 0x4321, "Terminal");
    seed_rule(&app, 0, "*Terminal*", "sb on");

    RuleMatch first = check_rule_match(&app.rules_config.rules[0], &state, 0, &app.matching, &w);
    ASSERT_TRUE("first fire before close", first.should_fire == true);

    live_ids[0] = 0x7777;
    rules_clear_applied_for_dead_windows(&app.rules_config, live_ids, 1);
    rule_state_remove_window(&state, w.id);
    RuleMatch second = check_rule_match(&app.rules_config.rules[0], &state, 0, &app.matching, &w);
    ASSERT_TRUE("fire again after applied and transition state cleared", second.should_fire == true);
}

static void test_once_false_refires_only_after_leave_and_reenter(void) {
    AppData app;
    RuleState state;
    WindowInfo w;
    WindowInfo nonmatch;
    memset(&app, 0, sizeof(app));
    init_rule_state(&state);
    match_entry_manager_init(&app.matching);
    seed_window(&w, 0xABCD, "Terminal");
    seed_window(&nonmatch, 0xABCD, "Editor");
    seed_rule(&app, 0, "*Terminal*", "sb on");
    app.rules_config.rules[0].once = false;

    RuleMatch first = check_rule_match(&app.rules_config.rules[0], &state, 0, &app.matching, &w);
    RuleMatch second = check_rule_match(&app.rules_config.rules[0], &state, 0, &app.matching, &w);
    RuleMatch away = check_rule_match(&app.rules_config.rules[0], &state, 0, &app.matching, &nonmatch);
    RuleMatch third = check_rule_match(&app.rules_config.rules[0], &state, 0, &app.matching, &w);
    ASSERT_TRUE("once=false first fire", first.should_fire == true);
    ASSERT_TRUE("once=false continuous match suppressed", second.should_fire == false);
    ASSERT_TRUE("once=false non-match clears state without firing", away.should_fire == false);
    ASSERT_TRUE("once=false re-entering match fires", third.should_fire == true);
}

static void test_toggle_once_clears_applied(void) {
    Rule rule;
    memset(&rule, 0, sizeof(rule));
    rule.once = true;
    rule.applied = 42;
    rule_toggle_once(&rule);
    ASSERT_TRUE("toggle flips once", rule.once == false);
    ASSERT_TRUE("toggle clears applied", rule.applied == 0);
}

static void test_replay_respects_once(void) {
    AppData app;
    WindowInfo *w;
    memset(&app, 0, sizeof(app));
    match_entry_manager_init(&app.matching);

    app.window_count = 1;
    w = &app.windows[0];
    seed_window(w, 0xBEEF, "Terminal");
    seed_rule(&app, 0, "*Terminal*", "sb on");

    app.rules_config.rules[0].applied = w->id;
    ASSERT_TRUE("replay skipped when already applied", replay_rule_against_open_windows(&app, &app.rules_config.rules[0]) == 0);

    app.rules_config.rules[0].applied = 0;
    ASSERT_TRUE("replay fires when unapplied", replay_rule_against_open_windows(&app, &app.rules_config.rules[0]) == 1);
    ASSERT_TRUE("replay sets applied", app.rules_config.rules[0].applied == w->id);
}

int main(void) {
    printf("Rules once tests\n");
    printf("================\n\n");

    test_fires_once_then_suppresses();
    test_clear_on_window_close_rearms();
    test_once_false_refires_only_after_leave_and_reenter();
    test_toggle_once_clears_applied();
    test_replay_respects_once();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
