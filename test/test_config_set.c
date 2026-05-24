#include <stdio.h>
#include <string.h>
#include "../src/config.h"

// This is the function we'll implement — pure logic, no AppData/X11 needed.
// Returns 1 on success, 0 on error. Error message written to err_buf.
int apply_config_setting(CofiConfig *config, const char *key, const char *value,
                         char *err_buf, size_t err_size);

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_OK(desc, config, key, value) do { \
    char err[256] = {0}; \
    if (!apply_config_setting(&(config), (key), (value), err, sizeof(err))) { \
        printf("FAIL: %s — expected OK, got error: %s\n", (desc), err); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } \
} while (0)

#define ASSERT_ERR(desc, config, key, value) do { \
    char err[256] = {0}; \
    if (apply_config_setting(&(config), (key), (value), err, sizeof(err))) { \
        printf("FAIL: %s — expected error, got OK\n", (desc)); \
        tests_failed++; \
    } else { \
        printf("PASS: %s (err: %s)\n", (desc), err); \
        tests_passed++; \
    } \
} while (0)

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

static void test_boolean_fields(void) {
    CofiConfig c;
    init_config_defaults(&c);

    ASSERT_OK("set close_on_focus_loss true", c, "close_on_focus_loss", "true");
    ASSERT_INT("close_on_focus_loss is 1", 1, c.close_on_focus_loss);

    ASSERT_OK("set close_on_focus_loss false", c, "close_on_focus_loss", "false");
    ASSERT_INT("close_on_focus_loss is 0", 0, c.close_on_focus_loss);

    ASSERT_OK("set ripple_enabled false", c, "ripple_enabled", "false");
    ASSERT_INT("ripple_enabled is 0", 0, c.ripple_enabled);

    ASSERT_OK("set ripple_enabled true", c, "ripple_enabled", "true");
    ASSERT_INT("ripple_enabled is 1", 1, c.ripple_enabled);

    // Aliases: on/off, 1/0
    ASSERT_OK("set ripple_enabled on", c, "ripple_enabled", "on");
    ASSERT_INT("ripple_enabled on", 1, c.ripple_enabled);

    ASSERT_OK("set ripple_enabled off", c, "ripple_enabled", "off");
    ASSERT_INT("ripple_enabled off", 0, c.ripple_enabled);

    ASSERT_OK("set ripple_enabled 1", c, "ripple_enabled", "1");
    ASSERT_INT("ripple_enabled 1", 1, c.ripple_enabled);

    ASSERT_OK("set ripple_enabled 0", c, "ripple_enabled", "0");
    ASSERT_INT("ripple_enabled 0", 0, c.ripple_enabled);

    ASSERT_OK("set show_all_tabs true", c, "show_all_tabs", "true");
    ASSERT_INT("show_all_tabs is 1", 1, c.show_all_tabs);

    ASSERT_OK("set show_all_tabs false", c, "show_all_tabs", "false");
    ASSERT_INT("show_all_tabs is 0", 0, c.show_all_tabs);

    ASSERT_ERR("set show_all_tabs garbage", c, "show_all_tabs", "garbage");

    ASSERT_OK("set rules.show_all_tags true", c, "rules.show_all_tags", "true");
    ASSERT_INT("rules_show_all_tags is 1", 1, c.rules_show_all_tags);

    ASSERT_OK("set rules.show_all_tags false", c, "rules.show_all_tags", "false");
    ASSERT_INT("rules_show_all_tags is 0", 0, c.rules_show_all_tags);

    ASSERT_ERR("set rules.show_all_tags garbage", c, "rules.show_all_tags", "garbage");
    ASSERT_OK("set legacy rules_show_all_tags still accepted", c, "rules_show_all_tags", "true");
    ASSERT_INT("legacy rules_show_all_tags sets value", 1, c.rules_show_all_tags);

    ASSERT_ERR("set close_on_focus_loss garbage", c, "close_on_focus_loss", "garbage");
}

static void test_integer_fields(void) {
    CofiConfig c;
    init_config_defaults(&c);

    ASSERT_OK("set workspaces_per_row 3", c, "workspaces_per_row", "3");
    ASSERT_INT("workspaces_per_row is 3", 3, c.workspaces_per_row);

    ASSERT_OK("set workspaces_per_row 0", c, "workspaces_per_row", "0");
    ASSERT_INT("workspaces_per_row is 0", 0, c.workspaces_per_row);

    ASSERT_OK("set tile_columns 3", c, "tile_columns", "3");
    ASSERT_INT("tile_columns is 3", 3, c.tile_columns);

    ASSERT_ERR("set tile_columns 4", c, "tile_columns", "4");

    ASSERT_OK("set slot_overlay_duration_ms 0", c, "slot_overlay_duration_ms", "0");
    ASSERT_INT("slot_overlay_duration_ms is 0", 0, c.slot_overlay_duration_ms);

    ASSERT_OK("set slot_overlay_duration_ms 2000", c, "slot_overlay_duration_ms", "2000");
    ASSERT_INT("slot_overlay_duration_ms is 2000", 2000, c.slot_overlay_duration_ms);

    ASSERT_ERR("set slot_overlay_duration_ms -1", c, "slot_overlay_duration_ms", "-1");
}

static void test_enum_fields(void) {
    CofiConfig c;
    init_config_defaults(&c);

    ASSERT_OK("set align top", c, "align", "top");
    ASSERT_INT("align is top", ALIGN_TOP, c.alignment);

    ASSERT_OK("set align bottom_right", c, "align", "bottom_right");
    ASSERT_INT("align is bottom_right", ALIGN_BOTTOM_RIGHT, c.alignment);

    ASSERT_ERR("set align garbage", c, "align", "garbage");

    ASSERT_OK("set digit_slot_mode per-workspace", c, "digit_slot_mode", "per-workspace");
    ASSERT_INT("digit_slot_mode is per-workspace", DIGIT_MODE_PER_WORKSPACE, c.digit_slot_mode);

    ASSERT_OK("set digit_slot_mode default", c, "digit_slot_mode", "default");
    ASSERT_INT("digit_slot_mode is default", DIGIT_MODE_DEFAULT, c.digit_slot_mode);

    ASSERT_ERR("set digit_slot_mode garbage", c, "digit_slot_mode", "garbage");
}

static void test_removed_hotkey_fields(void) {
    CofiConfig c;
    init_config_defaults(&c);

    ASSERT_ERR("set hotkey_windows rejected", c, "hotkey_windows", "Mod4+w");
    ASSERT_ERR("set hotkey_command rejected", c, "hotkey_command", "Mod4+space");
    ASSERT_ERR("set hotkey_workspaces rejected", c, "hotkey_workspaces", "Mod1+BackSpace");
}

static void test_disabled_providers_setting(void) {
    CofiConfig c;
    init_config_defaults(&c);

    ASSERT_STR("disabled_providers default empty", "", c.disabled_providers);
    ASSERT_OK("set disabled_providers list", c, "disabled_providers", "profiles,sinks");
    ASSERT_STR("disabled_providers stored", "profiles,sinks", c.disabled_providers);

    ASSERT_OK("clear disabled_providers", c, "disabled_providers", "");
    ASSERT_STR("disabled_providers cleared", "", c.disabled_providers);
}

static void test_unknown_key(void) {
    CofiConfig c;
    init_config_defaults(&c);

    ASSERT_ERR("set nonexistent_key value", c, "nonexistent_key", "value");
}

int main(void) {
    printf("Config :set and :config tests\n");
    printf("=============================\n\n");

    test_boolean_fields();
    test_integer_fields();
    test_enum_fields();
    test_removed_hotkey_fields();
    test_disabled_providers_setting();
    test_unknown_key();

    printf("\n=====================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
