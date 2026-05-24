#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/app_data.h"
#include "../src/geom_rule_sync.h"
#include "../src/match_entry.h"
#include "../src/rules_config.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(desc, cond) do { \
    if (!(cond)) { \
        printf("FAIL: %s\n", (desc)); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } \
} while (0)

static void set_test_home(void) {
    char tmpdir[] = "/tmp/cofi_geom_rule_sync_XXXXXX";
    char *dir = mkdtemp(tmpdir);
    if (!dir) {
        return;
    }
    setenv("HOME", dir, 1);
    char path[512];
    snprintf(path, sizeof(path), "%s/.config", dir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi", dir);
    mkdir(path, 0755);
}

static void init_test_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    match_entry_manager_init(&app->matching);
    init_rules_config(&app->rules_config);
}

static void add_layout_with_pattern(AppData *app, int match_id, const char *pattern, bool disabled) {
    int idx = app->matching.count++;
    app->matching.entries[idx].match_id = match_id;
    g_strlcpy(app->matching.entries[idx].original_title, pattern,
              sizeof(app->matching.entries[idx].original_title));
    app->layouts.records[app->layouts.count++] = (LayoutRecord){
        .match_id = match_id,
        .x = 10,
        .y = 20,
        .width = 300,
        .height = 200,
        .desktop = 1,
        .restore_desktop = true,
        .disabled = disabled
    };
}

static int count_geom_rules(const RulesConfig *config, const char *pattern) {
    int count = 0;
    for (int i = 0; i < config->count; i++) {
        if (strcmp(config->rules[i].tag, "geom") == 0 &&
            strcmp(config->rules[i].pattern, pattern) == 0 &&
            rule_commands_contain_segment(config->rules[i].commands, "rl")) {
            count++;
        }
    }
    return count;
}

static void test_refcount_create_and_delete(void) {
    AppData app;
    init_test_app(&app);
    add_layout_with_pattern(&app, 1, "My App", false);
    add_layout_with_pattern(&app, 2, "My App", false);

    geom_rule_sync_for_pattern(&app, "My App");
    ASSERT_TRUE("two enabled layouts create one tagged rule",
                count_geom_rules(&app.rules_config, "My App") == 1);

    app.layouts.records[0].disabled = true;
    geom_rule_sync_for_pattern(&app, "My App");
    ASSERT_TRUE("disabling one still keeps tagged rule",
                count_geom_rules(&app.rules_config, "My App") == 1);

    app.layouts.records[1].disabled = true;
    geom_rule_sync_for_pattern(&app, "My App");
    ASSERT_TRUE("disabling both deletes tagged rule",
                count_geom_rules(&app.rules_config, "My App") == 0);
}

static void test_delete_layout_refcount_behavior(void) {
    AppData app;
    init_test_app(&app);
    add_layout_with_pattern(&app, 1, "My App", false);
    add_layout_with_pattern(&app, 2, "My App", false);
    geom_rule_sync_for_pattern(&app, "My App");

    app.layouts.records[0] = app.layouts.records[1];
    app.layouts.count = 1;
    geom_rule_sync_for_pattern(&app, "My App");
    ASSERT_TRUE("deleting one of two keeps tagged rule",
                count_geom_rules(&app.rules_config, "My App") == 1);

    app.layouts.count = 0;
    geom_rule_sync_for_pattern(&app, "My App");
    ASSERT_TRUE("deleting sole layout removes tagged rule",
                count_geom_rules(&app.rules_config, "My App") == 0);
}

static void test_disable_enable_roundtrip(void) {
    AppData app;
    init_test_app(&app);
    add_layout_with_pattern(&app, 1, "Editor", false);

    geom_rule_sync_for_pattern(&app, "Editor");
    ASSERT_TRUE("enabled has tagged rule",
                count_geom_rules(&app.rules_config, "Editor") == 1);

    app.layouts.records[0].disabled = true;
    geom_rule_sync_for_pattern(&app, "Editor");
    ASSERT_TRUE("disabled deletes tagged rule",
                count_geom_rules(&app.rules_config, "Editor") == 0);

    app.layouts.records[0].disabled = false;
    geom_rule_sync_for_pattern(&app, "Editor");
    ASSERT_TRUE("re-enabled recreates tagged rule",
                count_geom_rules(&app.rules_config, "Editor") == 1);
}

static void test_untagged_rule_untouched(void) {
    AppData app;
    init_test_app(&app);
    add_layout_with_pattern(&app, 1, "Browser", false);
    add_rule(&app.rules_config, "Browser", "rl");

    geom_rule_sync_for_pattern(&app, "Browser");
    ASSERT_TRUE("untagged rl rule remains", app.rules_config.count == 2);
    ASSERT_TRUE("sync still manages tagged rule independently",
                count_geom_rules(&app.rules_config, "Browser") == 1);
}

static void test_startup_heal_create_and_delete(void) {
    AppData app;
    init_test_app(&app);
    add_layout_with_pattern(&app, 1, "alpha", false);
    add_layout_with_pattern(&app, 2, "beta", true);
    add_rule(&app.rules_config, "orphan", "rl");
    g_strlcpy(app.rules_config.rules[0].tag, "geom", sizeof(app.rules_config.rules[0].tag));

    geom_rule_sync_all_layout_patterns(&app);

    ASSERT_TRUE("startup creates missing tagged rule for enabled layout",
                count_geom_rules(&app.rules_config, "alpha") == 1);
    ASSERT_TRUE("startup keeps no tagged rule for fully-disabled pattern",
                count_geom_rules(&app.rules_config, "beta") == 0);
    ASSERT_TRUE("startup removes orphan tagged rule",
                count_geom_rules(&app.rules_config, "orphan") == 0);
}

static void test_wildcard_pattern_verbatim(void) {
    AppData app;
    init_test_app(&app);
    add_layout_with_pattern(&app, 1, "*foo*", false);
    geom_rule_sync_for_pattern(&app, "*foo*");

    ASSERT_TRUE("wildcard pattern stored verbatim",
                strcmp(app.rules_config.rules[0].pattern, "*foo*") == 0);
}

int main(void) {
    printf("Geom rule sync tests\n");
    printf("====================\n\n");

    set_test_home();
    test_refcount_create_and_delete();
    test_delete_layout_refcount_behavior();
    test_disable_enable_roundtrip();
    test_untagged_rule_untouched();
    test_startup_heal_create_and_delete();
    test_wildcard_pattern_verbatim();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
