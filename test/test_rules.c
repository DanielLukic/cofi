#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/rules_config.h"
#include "../src/rules.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_INT(desc, expected, actual) do { \
    if ((expected) != (actual)) { \
        printf("FAIL: %s — expected %d, got %d\n", (desc), (expected), (actual)); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } \
} while (0)

#define ASSERT_STR(desc, expected, actual) do { \
    if (strcmp((expected), (actual)) != 0) { \
        printf("FAIL: %s — expected '%s', got '%s'\n", (desc), (expected), (actual)); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } \
} while (0)

#define ASSERT_TRUE(desc, actual) do { \
    if (!(actual)) { \
        printf("FAIL: %s — expected true\n", (desc)); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } \
} while (0)

#define ASSERT_FALSE(desc, actual) do { \
    if ((actual)) { \
        printf("FAIL: %s — expected false\n", (desc)); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } \
} while (0)

// ========== Config load/save tests ==========

static void test_init(void) {
    RulesConfig config;
    init_rules_config(&config);
    ASSERT_INT("init count is 0", 0, config.count);
}

static void test_add_rule(void) {
    RulesConfig config;
    init_rules_config(&config);

    ASSERT_INT("add first", 1, add_rule(&config, "*htop*", "sb,ab,ew"));
    ASSERT_INT("count after add", 1, config.count);
    ASSERT_STR("first pattern", "*htop*", config.rules[0].pattern);
    ASSERT_STR("first commands", "sb,ab,ew", config.rules[0].commands);

    ASSERT_INT("add second", 1, add_rule(&config, "*Firefox*", "ew"));
    ASSERT_INT("count after second", 2, config.count);
}

static void test_remove_rule(void) {
    RulesConfig config;
    init_rules_config(&config);

    add_rule(&config, "*htop*", "sb,ab,ew");
    add_rule(&config, "*Firefox*", "ew");
    ASSERT_INT("count before remove", 2, config.count);

    ASSERT_INT("remove existing", 1, remove_rule(&config, 0));
    ASSERT_INT("count after remove", 1, config.count);
    ASSERT_STR("remaining is Firefox", "*Firefox*", config.rules[0].pattern);

    ASSERT_INT("remove out of bounds", 0, remove_rule(&config, 5));
    ASSERT_INT("remove negative", 0, remove_rule(&config, -1));
}

static void test_save_load_roundtrip(void) {
    char tmpdir[] = "/tmp/cofi_rules_test_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        printf("FAIL: mkdtemp\n");
        tests_failed++;
        return;
    }
    setenv("HOME", tmpdir, 1);

    RulesConfig original, loaded;
    init_rules_config(&original);

    add_rule(&original, "*htop*Terminal", "sb,ab,ew");
    add_rule(&original, "*Firefox*", "ew");
    add_rule(&original, "Tsunami*Thunderbird*", "sb");

    ASSERT_INT("save", 1, save_rules_config(&original));

    init_rules_config(&loaded);
    ASSERT_INT("load", 1, load_rules_config(&loaded));
    ASSERT_INT("loaded count", 3, loaded.count);
    ASSERT_STR("loaded pattern 0", "*htop*Terminal", loaded.rules[0].pattern);
    ASSERT_STR("loaded commands 0", "sb,ab,ew", loaded.rules[0].commands);
    ASSERT_STR("loaded pattern 1", "*Firefox*", loaded.rules[1].pattern);
    ASSERT_STR("loaded commands 1", "ew", loaded.rules[1].commands);
    ASSERT_STR("loaded pattern 2", "Tsunami*Thunderbird*", loaded.rules[2].pattern);
    ASSERT_STR("loaded commands 2", "sb", loaded.rules[2].commands);

    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);
}

static void test_load_missing_file(void) {
    char tmpdir[] = "/tmp/cofi_rules_empty_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        printf("FAIL: mkdtemp\n");
        tests_failed++;
        return;
    }
    setenv("HOME", tmpdir, 1);

    RulesConfig config;
    init_rules_config(&config);
    // load should succeed (return 1) with 0 rules when file doesn't exist
    ASSERT_INT("load missing file", 1, load_rules_config(&config));
    ASSERT_INT("count is 0", 0, config.count);

    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);
}

// ========== Rule matching state machine tests ==========

static void test_match_fires_with_explicit_state_args(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*htop*Terminal", "sb on,ab on,ew off,aot on");

    RuleState state;
    init_rule_state(&state);

    RuleMatch match = check_rule_match(&config.rules[0], &state, 0x3344, "root@host htop - Terminal");
    ASSERT_TRUE("matching title with explicit state args fires", match.should_fire);
    ASSERT_STR("explicit state args preserved", "sb on,ab on,ew off,aot on", match.commands);
}

static void test_match_fires_on_matching_title(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*htop*", "sb,ab,ew");

    RuleState state;
    init_rule_state(&state);

    // Window appears with matching title
    RuleMatch match = check_rule_match(&config.rules[0], &state, 0x1234, "root@~ htop — Terminal");
    ASSERT_TRUE("matching title fires", match.should_fire);
    ASSERT_STR("commands to fire", "sb,ab,ew", match.commands);
}

static void test_no_fire_on_non_matching_title(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*htop*", "sb,ab,ew");

    RuleState state;
    init_rule_state(&state);

    RuleMatch match = check_rule_match(&config.rules[0], &state, 0x1234, "root@~ — Terminal");
    ASSERT_FALSE("non-matching title does not fire", match.should_fire);
}

static void test_no_refire_same_title(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*htop*", "sb,ab,ew");

    RuleState state;
    init_rule_state(&state);

    // First match fires
    RuleMatch m1 = check_rule_match(&config.rules[0], &state, 0x1234, "root@~ htop — Terminal");
    ASSERT_TRUE("first match fires", m1.should_fire);

    // Same title again — should NOT fire
    RuleMatch m2 = check_rule_match(&config.rules[0], &state, 0x1234, "root@~ htop — Terminal");
    ASSERT_FALSE("same title does not refire", m2.should_fire);
}

static void test_refire_after_title_changes_away_and_back(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*htop*", "sb,ab,ew");

    RuleState state;
    init_rule_state(&state);

    // First match fires
    RuleMatch m1 = check_rule_match(&config.rules[0], &state, 0x1234, "root@~ htop — Terminal");
    ASSERT_TRUE("first match fires", m1.should_fire);

    // Title changes to non-matching — resets state
    RuleMatch m2 = check_rule_match(&config.rules[0], &state, 0x1234, "root@~ — Terminal");
    ASSERT_FALSE("non-matching does not fire", m2.should_fire);

    // Title changes back to matching — should fire again
    RuleMatch m3 = check_rule_match(&config.rules[0], &state, 0x1234, "root@~ htop — Terminal");
    ASSERT_TRUE("re-match fires again", m3.should_fire);
}

static void test_different_title_still_matching(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*htop*", "sb,ab,ew");

    RuleState state;
    init_rule_state(&state);

    // First match
    RuleMatch m1 = check_rule_match(&config.rules[0], &state, 0x1234, "root@~ htop — Terminal");
    ASSERT_TRUE("first match fires", m1.should_fire);

    // Different title but still matches pattern — should NOT fire (still matching)
    RuleMatch m2 = check_rule_match(&config.rules[0], &state, 0x1234, "user@host htop — Terminal");
    ASSERT_FALSE("still-matching title does not refire", m2.should_fire);
}

static void test_multiple_windows_independent(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*htop*", "sb,ab,ew");

    RuleState state;
    init_rule_state(&state);

    // Window A matches
    RuleMatch m1 = check_rule_match(&config.rules[0], &state, 0x1111, "htop — Terminal A");
    ASSERT_TRUE("window A fires", m1.should_fire);

    // Window B matches — independent, should also fire
    RuleMatch m2 = check_rule_match(&config.rules[0], &state, 0x2222, "htop — Terminal B");
    ASSERT_TRUE("window B fires independently", m2.should_fire);

    // Window A again — should NOT fire
    RuleMatch m3 = check_rule_match(&config.rules[0], &state, 0x1111, "htop — Terminal A");
    ASSERT_FALSE("window A does not refire", m3.should_fire);
}

static void test_window_removed_resets_state(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*htop*", "sb,ab,ew");

    RuleState state;
    init_rule_state(&state);

    // Window matches
    RuleMatch m1 = check_rule_match(&config.rules[0], &state, 0x1234, "htop — Terminal");
    ASSERT_TRUE("first match fires", m1.should_fire);

    // Window closes — clear its state
    rule_state_remove_window(&state, 0x1234);

    // Same window ID reopens (X11 may reuse IDs) — should fire again
    RuleMatch m2 = check_rule_match(&config.rules[0], &state, 0x1234, "htop — Terminal");
    ASSERT_TRUE("fires again after window removed", m2.should_fire);
}

// ========== rule_state_prune_absent tests ==========

static void test_prune_absent_one_cycle_preserves_state(void) {
    RuleState state;
    init_rule_state(&state);
    Rule rule = {"*htop*", "sb"};

    RuleMatch m1 = check_rule_match(&rule, &state, 0x1234, "htop");
    ASSERT_TRUE("initial match fires", m1.should_fire);

    // One cycle absent — pending_prune set, state NOT removed
    Window empty[] = {};
    rule_state_prune_absent(&state, empty, 0);

    // Window reappears — matched is still true, so rule must NOT re-fire
    RuleMatch m2 = check_rule_match(&rule, &state, 0x1234, "htop");
    ASSERT_FALSE("fire-once survives one-cycle absence", m2.should_fire);
}

static void test_prune_absent_two_cycles_removes_state(void) {
    RuleState state;
    init_rule_state(&state);
    Rule rule = {"*htop*", "sb"};

    RuleMatch m1 = check_rule_match(&rule, &state, 0x1234, "htop");
    ASSERT_TRUE("initial match fires", m1.should_fire);

    Window empty[] = {};
    rule_state_prune_absent(&state, empty, 0); // cycle 1: pending_prune set
    rule_state_prune_absent(&state, empty, 0); // cycle 2: entry removed

    // Window reappears — state was removed, so it fires again (fresh entry)
    RuleMatch m2 = check_rule_match(&rule, &state, 0x1234, "htop");
    ASSERT_TRUE("re-fires after two-cycle absence", m2.should_fire);
}

static void test_prune_absent_reappearance_clears_pending(void) {
    RuleState state;
    init_rule_state(&state);
    Rule rule = {"*htop*", "sb"};

    check_rule_match(&rule, &state, 0x1234, "htop");

    // One cycle absent
    Window empty[] = {};
    rule_state_prune_absent(&state, empty, 0);

    // Reappears — pending_prune must be cleared
    Window live[] = {0x1234};
    rule_state_prune_absent(&state, live, 1);

    // Another absence cycle: since pending_prune was cleared, this is only
    // the first absence again — state should NOT be removed
    rule_state_prune_absent(&state, empty, 0);

    // Window comes back: matched still true, so no re-fire
    RuleMatch m = check_rule_match(&rule, &state, 0x1234, "htop");
    ASSERT_FALSE("reappear clears pending_prune; next absence is again one-cycle only", m.should_fire);
}

static void test_prune_absent_unmatched_window_removed_cleanly(void) {
    RuleState state;
    init_rule_state(&state);
    Rule rule = {"*htop*", "sb"};

    // Window appears but doesn't match — it's added to state with matched=false
    check_rule_match(&rule, &state, 0xAAAA, "bash");
    ASSERT_INT("state has one entry", 1, state.count);

    // Two cycles absent — should be removed
    Window empty[] = {};
    rule_state_prune_absent(&state, empty, 0);
    rule_state_prune_absent(&state, empty, 0);
    ASSERT_INT("state empty after two cycles", 0, state.count);
}

static void test_prune_absent_multiple_windows_independent(void) {
    RuleState state;
    init_rule_state(&state);
    Rule rule = {"*htop*", "sb"};

    check_rule_match(&rule, &state, 0x1111, "htop A");
    check_rule_match(&rule, &state, 0x2222, "htop B");

    // Only 0x1111 is absent
    Window only2[] = {0x2222};
    rule_state_prune_absent(&state, only2, 1);
    rule_state_prune_absent(&state, only2, 1); // second cycle: 0x1111 removed

    // 0x1111 fires again (state gone); 0x2222 still suppressed
    RuleMatch m1 = check_rule_match(&rule, &state, 0x1111, "htop A");
    ASSERT_TRUE("0x1111 fires again after two-cycle absence", m1.should_fire);
    RuleMatch m2 = check_rule_match(&rule, &state, 0x2222, "htop B");
    ASSERT_FALSE("0x2222 still suppressed (never absent)", m2.should_fire);
}

static void test_multiple_rules(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*htop*", "sb,ab,ew");
    add_rule(&config, "*Firefox*", "ew");

    RuleState state;
    init_rule_state(&state);

    // Window matches first rule only
    RuleMatch m1 = check_rule_match(&config.rules[0], &state, 0x1234, "htop — Terminal");
    ASSERT_TRUE("htop matches rule 0", m1.should_fire);
    RuleMatch m2 = check_rule_match(&config.rules[1], &state, 0x1234, "htop — Terminal");
    ASSERT_FALSE("htop does not match rule 1", m2.should_fire);

    // Another window matches second rule only
    RuleMatch m3 = check_rule_match(&config.rules[0], &state, 0x5678, "Firefox");
    ASSERT_FALSE("Firefox does not match rule 0", m3.should_fire);
    RuleMatch m4 = check_rule_match(&config.rules[1], &state, 0x5678, "Firefox");
    ASSERT_TRUE("Firefox matches rule 1", m4.should_fire);
}

int main(void) {
    printf("Rules tests\n");
    printf("===========\n\n");

    // Config tests
    printf("--- Config ---\n");
    test_init();
    test_add_rule();
    test_remove_rule();
    test_save_load_roundtrip();
    test_load_missing_file();

    // Matching state machine tests
    printf("\n--- Matching ---\n");
    test_match_fires_with_explicit_state_args();
    test_match_fires_on_matching_title();
    test_no_fire_on_non_matching_title();
    test_no_refire_same_title();
    test_refire_after_title_changes_away_and_back();
    test_different_title_still_matching();
    test_multiple_windows_independent();
    test_window_removed_resets_state();
    test_multiple_rules();

    // Transient-absence grace tests
    printf("\n--- Prune absent (grace-count) ---\n");
    test_prune_absent_one_cycle_preserves_state();
    test_prune_absent_two_cycles_removes_state();
    test_prune_absent_reappearance_clears_pending();
    test_prune_absent_unmatched_window_removed_cleanly();
    test_prune_absent_multiple_windows_independent();

    printf("\n=====================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
