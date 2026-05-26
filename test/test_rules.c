#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "../src/rules_config.h"
#include "../src/rules.h"

static int tests_passed = 0;
static int tests_failed = 0;
static MatchEntryManager g_matching;

static RuleMatch check_rule_match_for_title(const Rule *rule, RuleState *state, int rule_index,
                                            Window id, const char *title) {
    WindowInfo window = {0};
    window.id = id;
    strncpy(window.title, title ? title : "", sizeof(window.title) - 1);
    window.title[sizeof(window.title) - 1] = '\0';
    window.class_name[0] = '\0';
    window.instance[0] = '\0';
    window.type[0] = '\0';

    if (rule->match_id <= 0 || match_entry_find_index_by_match_id(&g_matching, rule->match_id) < 0) {
        int match_id = matching_find_or_create_pattern_entry(&g_matching, rule->pattern);
        ((Rule *)rule)->match_id = match_id;
    }

    return check_rule_match(rule, state, rule_index, &g_matching, &window);
}

#define check_rule_match(rule, state, rule_index, id, title) \
    check_rule_match_for_title((rule), (state), (rule_index), (id), (title))
#define load_rules_config(config) load_rules_config((config), &g_matching)
#define save_rules_config(config) save_rules_config((config), &g_matching)

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
    ASSERT_FALSE("run_at_start defaults false on add", config.rules[0].run_at_start);

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
    add_rule(&original, "*quote\"slash\\<script>*", "rl,echo \"hi\"");
    original.rules[1].run_at_start = 1;
    strncpy(original.rules[1].tag, "geom", sizeof(original.rules[1].tag) - 1);
    original.rules[1].tag[sizeof(original.rules[1].tag) - 1] = '\0';

    ASSERT_INT("save", 1, save_rules_config(&original));

    init_rules_config(&loaded);
    ASSERT_INT("load", 1, load_rules_config(&loaded));
    ASSERT_INT("loaded count", 4, loaded.count);
    ASSERT_STR("loaded pattern 0", "*htop*Terminal", loaded.rules[0].pattern);
    ASSERT_STR("loaded commands 0", "sb,ab,ew", loaded.rules[0].commands);
    ASSERT_STR("loaded pattern 1", "*Firefox*", loaded.rules[1].pattern);
    ASSERT_STR("loaded commands 1", "ew", loaded.rules[1].commands);
    ASSERT_TRUE("loaded run_at_start 1", loaded.rules[1].run_at_start);
    ASSERT_STR("loaded tag 1", "geom", loaded.rules[1].tag);
    ASSERT_STR("loaded pattern 2", "Tsunami*Thunderbird*", loaded.rules[2].pattern);
    ASSERT_STR("loaded commands 2", "sb", loaded.rules[2].commands);
    ASSERT_STR("loaded pattern 3", "*quote\"slash\\<script>*", loaded.rules[3].pattern);
    ASSERT_STR("loaded commands 3", "rl,echo \"hi\"", loaded.rules[3].commands);
    ASSERT_FALSE("loaded run_at_start defaults false when saved false", loaded.rules[0].run_at_start);
    ASSERT_FALSE("loaded run_at_start remains false on third rule", loaded.rules[2].run_at_start);
    ASSERT_STR("loaded empty tag defaults to empty string", "", loaded.rules[0].tag);

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

static void test_load_legacy_file_defaults_run_at_start_false(void) {
    char tmpdir[] = "/tmp/cofi_rules_legacy_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        printf("FAIL: mkdtemp\n");
        tests_failed++;
        return;
    }
    setenv("HOME", tmpdir, 1);

    char path[600];
    snprintf(path, sizeof(path), "%s/.config", tmpdir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi", tmpdir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi/rules.json", tmpdir);

    FILE *file = fopen(path, "w");
    if (!file) {
        printf("FAIL: open legacy rules.json\n");
        tests_failed++;
        return;
    }
    fprintf(file,
            "{\n"
            "  \"rules\": [\n"
            "    {\n"
            "      \"pattern\": \"*term*\",\n"
            "      \"commands\": \"sb on\"\n"
            "    }\n"
            "  ]\n"
            "}\n");
    fclose(file);

    RulesConfig config;
    init_rules_config(&config);
    ASSERT_INT("load legacy file", 1, load_rules_config(&config));
    ASSERT_INT("legacy count", 1, config.count);
    ASSERT_FALSE("legacy run_at_start defaults false", config.rules[0].run_at_start);
    ASSERT_STR("legacy tag defaults empty", "", config.rules[0].tag);
    ASSERT_TRUE("legacy migration sets match_id", config.rules[0].match_id > 0);

    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);
}

static void test_load_rules_json_with_special_chars(void) {
    char tmpdir[] = "/tmp/cofi_rules_special_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        printf("FAIL: mkdtemp\n");
        tests_failed++;
        return;
    }
    setenv("HOME", tmpdir, 1);

    char path[600];
    snprintf(path, sizeof(path), "%s/.config", tmpdir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi", tmpdir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi/rules.json", tmpdir);

    FILE *file = fopen(path, "w");
    if (!file) {
        printf("FAIL: open special rules.json\n");
        tests_failed++;
        return;
    }
    fprintf(file,
            "{\n"
            "  \"rules\": [\n"
            "    {\n"
            "      \"pattern\": \"*term?$HOME<script>\",\n"
            "      \"commands\": \"rl,ew+,ab+\",\n"
            "      \"run_at_start\": true,\n"
            "      \"tag\": \"geom\",\n"
            "      \"future_field\": \"ignored\"\n"
            "    },\n"
            "    {\n"
            "      \"pattern\": \"browser*&docs?\",\n"
            "      \"commands\": \"sb off, aot on\",\n"
            "      \"run_at_start\": false\n"
            "    }\n"
            "  ],\n"
            "  \"unknown_root\": true\n"
            "}\n");
    fclose(file);

    RulesConfig config;
    init_rules_config(&config);
    ASSERT_INT("load special char rules file", 1, load_rules_config(&config));
    ASSERT_INT("special char count", 2, config.count);
    ASSERT_STR("special pattern 0", "*term?$HOME<script>", config.rules[0].pattern);
    ASSERT_STR("special commands 0", "rl,ew+,ab+", config.rules[0].commands);
    ASSERT_TRUE("special run_at_start 0", config.rules[0].run_at_start);
    ASSERT_STR("special tag 0", "geom", config.rules[0].tag);
    ASSERT_STR("special pattern 1", "browser*&docs?", config.rules[1].pattern);
    ASSERT_STR("special commands 1", "sb off, aot on", config.rules[1].commands);
    ASSERT_FALSE("special run_at_start 1", config.rules[1].run_at_start);
    ASSERT_STR("special tag 1 defaults empty", "", config.rules[1].tag);

    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);
}

static void test_load_skips_rules_missing_required_fields(void) {
    char tmpdir[] = "/tmp/cofi_rules_missing_required_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        printf("FAIL: mkdtemp\n");
        tests_failed++;
        return;
    }
    setenv("HOME", tmpdir, 1);

    char path[600];
    snprintf(path, sizeof(path), "%s/.config", tmpdir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi", tmpdir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi/rules.json", tmpdir);

    FILE *file = fopen(path, "w");
    if (!file) {
        printf("FAIL: open missing-required rules.json\n");
        tests_failed++;
        return;
    }
    fprintf(file,
            "{\n"
            "  \"rules\": [\n"
            "    {\"commands\": \"rl\", \"run_at_start\": true},\n"
            "    {\"pattern\": \"*missing commands*\"},\n"
            "    {\"pattern\": \"*valid*\", \"commands\": \"sb\", \"run_at_start\": true}\n"
            "  ]\n"
            "}\n");
    fclose(file);

    RulesConfig config;
    init_rules_config(&config);
    ASSERT_INT("load skips missing required fields", 1, load_rules_config(&config));
    ASSERT_INT("missing required leaves only valid rule", 1, config.count);
    ASSERT_STR("valid rule pattern survives", "*valid*", config.rules[0].pattern);
    ASSERT_STR("valid rule commands survives", "sb", config.rules[0].commands);
    ASSERT_TRUE("valid rule run_at_start survives", config.rules[0].run_at_start);
    ASSERT_TRUE("valid rule migrated to match_id", config.rules[0].match_id > 0);

    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);
}

static void test_load_corrupt_json_returns_empty(void) {
    char tmpdir[] = "/tmp/cofi_rules_corrupt_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        printf("FAIL: mkdtemp\n");
        tests_failed++;
        return;
    }
    setenv("HOME", tmpdir, 1);

    char path[600];
    snprintf(path, sizeof(path), "%s/.config", tmpdir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi", tmpdir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi/rules.json", tmpdir);

    FILE *file = fopen(path, "w");
    if (!file) {
        printf("FAIL: open corrupt rules.json\n");
        tests_failed++;
        return;
    }
    fprintf(file, "{\"rules\": [");
    fclose(file);

    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*preexisting*", "rl");
    ASSERT_INT("load corrupt json succeeds empty", 1, load_rules_config(&config));
    ASSERT_INT("corrupt json leaves config empty", 0, config.count);

    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);
}

static void test_load_legacy_empty_pattern_skips_without_creating_entry(void) {
    char tmpdir[] = "/tmp/cofi_rules_empty_pattern_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        printf("FAIL: mkdtemp\n");
        tests_failed++;
        return;
    }
    setenv("HOME", tmpdir, 1);

    char path[600];
    snprintf(path, sizeof(path), "%s/.config", tmpdir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi", tmpdir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi/rules.json", tmpdir);

    FILE *file = fopen(path, "w");
    if (!file) {
        printf("FAIL: open empty-pattern rules.json\n");
        tests_failed++;
        return;
    }
    fprintf(file,
            "{\n"
            "  \"rules\": [\n"
            "    {\"pattern\": \"\", \"commands\": \"rl\"},\n"
            "    {\"commands\": \"rl\"}\n"
            "  ]\n"
            "}\n");
    fclose(file);

    int before = g_matching.count;
    RulesConfig config;
    init_rules_config(&config);
    ASSERT_INT("empty pattern rule skipped", 1, load_rules_config(&config));
    ASSERT_INT("no valid rules loaded", 0, config.count);
    ASSERT_INT("no match entry created for empty pattern", before, g_matching.count);

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

    RuleMatch match = check_rule_match(&config.rules[0], &state, 0, 0x3344, "root@host htop - Terminal");
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
    RuleMatch match = check_rule_match(&config.rules[0], &state, 0, 0x1234, "root@~ htop — Terminal");
    ASSERT_TRUE("matching title fires", match.should_fire);
    ASSERT_STR("commands to fire", "sb,ab,ew", match.commands);
}

static void test_no_fire_on_non_matching_title(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*htop*", "sb,ab,ew");

    RuleState state;
    init_rule_state(&state);

    RuleMatch match = check_rule_match(&config.rules[0], &state, 0, 0x1234, "root@~ — Terminal");
    ASSERT_FALSE("non-matching title does not fire", match.should_fire);
}

static void test_no_refire_same_title(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*htop*", "sb,ab,ew");

    RuleState state;
    init_rule_state(&state);

    // First match fires
    RuleMatch m1 = check_rule_match(&config.rules[0], &state, 0, 0x1234, "root@~ htop — Terminal");
    ASSERT_TRUE("first match fires", m1.should_fire);

    // Same title again — should NOT fire
    RuleMatch m2 = check_rule_match(&config.rules[0], &state, 0, 0x1234, "root@~ htop — Terminal");
    ASSERT_FALSE("same title does not refire", m2.should_fire);
}

static void test_startup_suppression_still_seeds_matched_state(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*htop*", "sb,ab,ew");

    RuleState state;
    init_rule_state(&state);

    // Startup scan: the matcher sees a fire-once transition and seeds matched=true.
    RuleMatch startup = check_rule_match(&config.rules[0], &state, 0, 0x1234,
                                         "root@~ htop — Terminal");
    ASSERT_TRUE("startup match would fire before suppression", startup.should_fire);

    // Post-startup event on the same still-present window must now suppress.
    RuleMatch post_startup = check_rule_match(&config.rules[0], &state, 0, 0x1234,
                                              "root@~ htop — Terminal");
    ASSERT_FALSE("post-startup recheck stays suppressed after startup seeding",
                 post_startup.should_fire);
}

static void test_refire_after_title_changes_away_and_back(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*htop*", "sb,ab,ew");

    RuleState state;
    init_rule_state(&state);

    // First match fires
    RuleMatch m1 = check_rule_match(&config.rules[0], &state, 0, 0x1234, "root@~ htop — Terminal");
    ASSERT_TRUE("first match fires", m1.should_fire);

    // Title changes to non-matching — resets state
    RuleMatch m2 = check_rule_match(&config.rules[0], &state, 0, 0x1234, "root@~ — Terminal");
    ASSERT_FALSE("non-matching does not fire", m2.should_fire);

    // Title changes back to matching — should fire again
    RuleMatch m3 = check_rule_match(&config.rules[0], &state, 0, 0x1234, "root@~ htop — Terminal");
    ASSERT_TRUE("re-match fires again", m3.should_fire);
}

static void test_different_title_still_matching(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*htop*", "sb,ab,ew");

    RuleState state;
    init_rule_state(&state);

    // First match
    RuleMatch m1 = check_rule_match(&config.rules[0], &state, 0, 0x1234, "root@~ htop — Terminal");
    ASSERT_TRUE("first match fires", m1.should_fire);

    // Different title but still matches pattern — should NOT fire (still matching)
    RuleMatch m2 = check_rule_match(&config.rules[0], &state, 0, 0x1234, "user@host htop — Terminal");
    ASSERT_FALSE("still-matching title does not refire", m2.should_fire);
}

static void test_multiple_windows_independent(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*htop*", "sb,ab,ew");

    RuleState state;
    init_rule_state(&state);

    // Window A matches
    RuleMatch m1 = check_rule_match(&config.rules[0], &state, 0, 0x1111, "htop — Terminal A");
    ASSERT_TRUE("window A fires", m1.should_fire);

    // Window B matches — independent, should also fire
    RuleMatch m2 = check_rule_match(&config.rules[0], &state, 0, 0x2222, "htop — Terminal B");
    ASSERT_TRUE("window B fires independently", m2.should_fire);

    // Window A again — should NOT fire
    RuleMatch m3 = check_rule_match(&config.rules[0], &state, 0, 0x1111, "htop — Terminal A");
    ASSERT_FALSE("window A does not refire", m3.should_fire);
}

static void test_window_removed_resets_state(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*htop*", "sb,ab,ew");

    RuleState state;
    init_rule_state(&state);

    // Window matches
    RuleMatch m1 = check_rule_match(&config.rules[0], &state, 0, 0x1234, "htop — Terminal");
    ASSERT_TRUE("first match fires", m1.should_fire);

    // Window closes — clear its state
    rule_state_remove_window(&state, 0x1234);

    // Same window ID reopens (X11 may reuse IDs) — should fire again
    RuleMatch m2 = check_rule_match(&config.rules[0], &state, 0, 0x1234, "htop — Terminal");
    ASSERT_TRUE("fires again after window removed", m2.should_fire);
}

// ========== rule_state_prune_absent tests ==========

static void test_prune_absent_removes_immediately(void) {
    RuleState state;
    init_rule_state(&state);
    Rule rule = {.pattern = "*htop*", .commands = "sb", .run_at_start = 0};

    check_rule_match(&rule, &state, 0, 0x1234, "htop");
    ASSERT_INT("state has one entry", 1, state.count);

    Window empty[] = {};
    rule_state_prune_absent(&state, empty, 0);
    ASSERT_INT("state empty after one absent cycle", 0, state.count);
}

static void test_prune_absent_present_window_kept(void) {
    RuleState state;
    init_rule_state(&state);
    Rule rule = {.pattern = "*htop*", .commands = "sb", .run_at_start = 0};

    check_rule_match(&rule, &state, 0, 0x1111, "htop A");
    check_rule_match(&rule, &state, 0, 0x2222, "htop B");

    Window only2[] = {0x2222};
    rule_state_prune_absent(&state, only2, 1);

    // 0x1111 removed immediately; 0x2222 kept
    RuleMatch m1 = check_rule_match(&rule, &state, 0, 0x1111, "htop A");
    ASSERT_TRUE("0x1111 fires again after removal", m1.should_fire);
    RuleMatch m2 = check_rule_match(&rule, &state, 0, 0x2222, "htop B");
    ASSERT_FALSE("0x2222 still suppressed (was present)", m2.should_fire);
}

// ========== Circuit breaker tests ==========

static void test_breaker_allows_up_to_limit(void) {
    RuleBreakerState breaker;
    init_rule_breaker(&breaker);
    // 10 fires within 1000ms — all must be allowed
    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE("fire within limit allowed",
            rule_breaker_should_fire(&breaker, 0, 0x1234, (int64_t)i * 50, "*htop*"));
    }
    // 11th fire in same window — must be suppressed
    ASSERT_FALSE("11th fire suppressed",
        rule_breaker_should_fire(&breaker, 0, 0x1234, 500, "*htop*"));
}

static void test_breaker_quiet_period_rearms(void) {
    RuleBreakerState breaker;
    init_rule_breaker(&breaker);
    // exhaust: fires at t=0,50,...,500 — 10th allowed, 11th triggers suppression
    for (int i = 0; i <= 10; i++) {
        rule_breaker_should_fire(&breaker, 0, 0x1234, (int64_t)i * 50, "*htop*");
    }
    // last_fire_ms=500; quiet for >2000ms → re-arm
    ASSERT_TRUE("re-armed after quiet period",
        rule_breaker_should_fire(&breaker, 0, 0x1234, 500 + 2001, "*htop*"));
    // Immediately after re-arm a second fire still allowed (new burst, count=2)
    ASSERT_TRUE("second fire after re-arm allowed",
        rule_breaker_should_fire(&breaker, 0, 0x1234, 500 + 2002, "*htop*"));
}

static void test_breaker_burst_window_resets(void) {
    RuleBreakerState breaker;
    init_rule_breaker(&breaker);
    // 5 fires within burst window (t=0..200)
    for (int i = 0; i < 5; i++) {
        ASSERT_TRUE("early fire allowed",
            rule_breaker_should_fire(&breaker, 0, 0x1234, (int64_t)i * 50, "*htop*"));
    }
    // Fire at t=1100 — burst window expired; count resets to 1
    ASSERT_TRUE("fire after burst window reset allowed",
        rule_breaker_should_fire(&breaker, 0, 0x1234, 1100, "*htop*"));
    // 4 more fires in new window (total 5 in this window, well below limit)
    for (int i = 1; i <= 4; i++) {
        ASSERT_TRUE("fires in second window allowed",
            rule_breaker_should_fire(&breaker, 0, 0x1234, 1100 + (int64_t)i * 10, "*htop*"));
    }
    // Still under limit — no suppression
    ASSERT_TRUE("6th fire in second window still allowed",
        rule_breaker_should_fire(&breaker, 0, 0x1234, 1155, "*htop*"));
}

static void test_breaker_different_pairs_independent(void) {
    RuleBreakerState breaker;
    init_rule_breaker(&breaker);
    // Trigger suppression for rule 0 / window 0x1234
    for (int i = 0; i <= 10; i++) {
        rule_breaker_should_fire(&breaker, 0, 0x1234, (int64_t)i * 10, "*htop*");
    }
    // Different rule index: independent — must not be suppressed
    ASSERT_TRUE("different rule_index not suppressed",
        rule_breaker_should_fire(&breaker, 1, 0x1234, 0, "*htop*"));
    // Same rule, different window: independent
    ASSERT_TRUE("different window_id not suppressed",
        rule_breaker_should_fire(&breaker, 0, 0x5678, 0, "*htop*"));
}

static void test_breaker_null_safe(void) {
    ASSERT_TRUE("null breaker → fail open",
        rule_breaker_should_fire(NULL, 0, 0x1234, 0, "*htop*"));
}

// Regression: two rules must not stomp each other's matched flag.
// With old window-id-only keying, R1 checking a window set matched by R0 would
// execute the !matches&&matched→RESET branch, clearing R0's state and causing
// R0 to re-fire on the next evaluation cycle (the root cause of the storm).
static void test_two_rules_no_state_stomp(void) {
    Rule r0 = {.pattern = "*htop*", .commands = "ew", .run_at_start = 0};        // matches window W
    Rule r1 = {.pattern = "*Firefox*", .commands = "ew", .run_at_start = 0};     // does NOT match window W

    RuleState state;
    init_rule_state(&state);

    // R0 fires on W
    RuleMatch m0_first = check_rule_match(&r0, &state, 0, 0xAAAA, "htop");
    ASSERT_TRUE("R0 fires on first match", m0_first.should_fire);

    // R1 evaluates same window — must not touch R0's state
    RuleMatch m1 = check_rule_match(&r1, &state, 1, 0xAAAA, "htop");
    ASSERT_FALSE("R1 does not fire (no match)", m1.should_fire);

    // R0 must suppress — not re-fire because R1 stomped its flag
    RuleMatch m0_second = check_rule_match(&r0, &state, 0, 0xAAAA, "htop");
    ASSERT_FALSE("R0 suppressed: R1 did not stomp R0 state", m0_second.should_fire);
}

static void test_multiple_rules(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "*htop*", "sb,ab,ew");
    add_rule(&config, "*Firefox*", "ew");

    RuleState state;
    init_rule_state(&state);

    // Window matches first rule only
    RuleMatch m1 = check_rule_match(&config.rules[0], &state, 0, 0x1234, "htop — Terminal");
    ASSERT_TRUE("htop matches rule 0", m1.should_fire);
    RuleMatch m2 = check_rule_match(&config.rules[1], &state, 1, 0x1234, "htop — Terminal");
    ASSERT_FALSE("htop does not match rule 1", m2.should_fire);

    // Another window matches second rule only
    RuleMatch m3 = check_rule_match(&config.rules[0], &state, 0, 0x5678, "Firefox");
    ASSERT_FALSE("Firefox does not match rule 0", m3.should_fire);
    RuleMatch m4 = check_rule_match(&config.rules[1], &state, 1, 0x5678, "Firefox");
    ASSERT_TRUE("Firefox matches rule 1", m4.should_fire);
}

// ========== rules_needs_restore_rule tests ==========

static void test_needs_restore_rule_empty_config(void) {
    RulesConfig config;
    init_rules_config(&config);
    ASSERT_TRUE("empty config always needs rule", rules_needs_restore_rule(&config, "My App"));
}

static void test_needs_restore_rule_matching_rl_rule_skips(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "My App", "rl");
    ASSERT_FALSE("exact match with rl skips creation", rules_needs_restore_rule(&config, "My App"));
}

static void test_needs_restore_rule_non_rl_commands_needs_rule(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "My App", "sb,ew");
    ASSERT_TRUE("rule without rl still needs restore rule", rules_needs_restore_rule(&config, "My App"));
}

static void test_needs_restore_rule_glob_covers_exact_title(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "cofi*", "rl");
    ASSERT_FALSE("glob with rl covers matching title", rules_needs_restore_rule(&config, "cofi | main"));
}

static void test_needs_restore_rule_non_matching_rl_needs_rule(void) {
    RulesConfig config;
    init_rules_config(&config);
    add_rule(&config, "Firefox*", "rl");
    ASSERT_TRUE("rl rule for different pattern needs new rule", rules_needs_restore_rule(&config, "My App"));
}

static void test_rule_commands_contain_segment_rl_variants(void) {
    ASSERT_TRUE("rl matches exact", rule_commands_contain_segment("rl", "rl"));
    ASSERT_TRUE("rl matches head", rule_commands_contain_segment("rl,foo", "rl"));
    ASSERT_TRUE("rl matches tail", rule_commands_contain_segment("foo,rl", "rl"));
    ASSERT_TRUE("rl matches with spacing", rule_commands_contain_segment("foo, rl", "rl"));
    ASSERT_TRUE("rl matches trimmed single", rule_commands_contain_segment(" rl ", "rl"));

    ASSERT_FALSE("does not match url", rule_commands_contain_segment("url", "rl"));
    ASSERT_FALSE("does not match rlx", rule_commands_contain_segment("rlx", "rl"));
    ASSERT_FALSE("does not match foorl", rule_commands_contain_segment("foorl", "rl"));
}

int main(void) {
    printf("Rules tests\n");
    printf("===========\n\n");
    match_entry_manager_init(&g_matching);

    // Config tests
    printf("--- Config ---\n");
    test_init();
    test_add_rule();
    test_remove_rule();
    test_save_load_roundtrip();
    test_load_missing_file();
    test_load_legacy_file_defaults_run_at_start_false();
    test_load_rules_json_with_special_chars();
    test_load_skips_rules_missing_required_fields();
    test_load_corrupt_json_returns_empty();
    test_load_legacy_empty_pattern_skips_without_creating_entry();

    // Matching state machine tests
    printf("\n--- Matching ---\n");
    test_match_fires_with_explicit_state_args();
    test_match_fires_on_matching_title();
    test_no_fire_on_non_matching_title();
    test_no_refire_same_title();
    test_startup_suppression_still_seeds_matched_state();
    test_refire_after_title_changes_away_and_back();
    test_different_title_still_matching();
    test_multiple_windows_independent();
    test_window_removed_resets_state();
    test_two_rules_no_state_stomp();
    test_multiple_rules();

    // Prune absent tests
    printf("\n--- Prune absent ---\n");
    test_prune_absent_removes_immediately();
    test_prune_absent_present_window_kept();

    // Restore rule dedup tests
    printf("\n--- Restore rule dedup ---\n");
    test_needs_restore_rule_empty_config();
    test_needs_restore_rule_matching_rl_rule_skips();
    test_needs_restore_rule_non_rl_commands_needs_rule();
    test_needs_restore_rule_glob_covers_exact_title();
    test_needs_restore_rule_non_matching_rl_needs_rule();
    test_rule_commands_contain_segment_rl_variants();

    // Circuit breaker tests
    printf("\n--- Circuit breaker ---\n");
    test_breaker_allows_up_to_limit();
    test_breaker_quiet_period_rearms();
    test_breaker_burst_window_resets();
    test_breaker_different_pairs_independent();
    test_breaker_null_safe();

    printf("\n=====================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
