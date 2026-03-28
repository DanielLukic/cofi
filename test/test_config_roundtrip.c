#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/config.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_INT(name, expected, actual) do { \
    if ((expected) != (actual)) { \
        printf("FAIL: %s — expected %d, got %d\n", name, expected, actual); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", name); \
        tests_passed++; \
    } \
} while (0)

#define ASSERT_STR(name, expected, actual) do { \
    if (strcmp((expected), (actual)) != 0) { \
        printf("FAIL: %s — expected '%s', got '%s'\n", name, expected, actual); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", name); \
        tests_passed++; \
    } \
} while (0)

// Test 1: defaults round-trip (init → save → load → assert defaults)
static void test_defaults_roundtrip(void) {
    CofiConfig original, loaded;
    init_config_defaults(&original);
    save_config(&original);
    load_config(&loaded);

    ASSERT_INT("defaults: close_on_focus_loss", original.close_on_focus_loss, loaded.close_on_focus_loss);
    ASSERT_INT("defaults: alignment", original.alignment, loaded.alignment);
    ASSERT_INT("defaults: workspaces_per_row", original.workspaces_per_row, loaded.workspaces_per_row);
    ASSERT_INT("defaults: tile_columns", original.tile_columns, loaded.tile_columns);
    ASSERT_INT("defaults: digit_slot_mode", original.digit_slot_mode, loaded.digit_slot_mode);
    ASSERT_INT("defaults: slot_overlay_duration_ms", original.slot_overlay_duration_ms, loaded.slot_overlay_duration_ms);
    ASSERT_INT("defaults: ripple_enabled", original.ripple_enabled, loaded.ripple_enabled);
    ASSERT_STR("defaults: hotkey_windows", original.hotkey_windows, loaded.hotkey_windows);
    ASSERT_STR("defaults: hotkey_command", original.hotkey_command, loaded.hotkey_command);
    ASSERT_STR("defaults: hotkey_workspaces", original.hotkey_workspaces, loaded.hotkey_workspaces);
}

// Test 2: non-default values round-trip
static void test_nondefault_roundtrip(void) {
    CofiConfig original, loaded;
    init_config_defaults(&original);

    // Set every field to a non-default value
    original.close_on_focus_loss = 0;
    original.alignment = ALIGN_BOTTOM_RIGHT;
    original.workspaces_per_row = 4;
    original.tile_columns = 3;
    original.digit_slot_mode = DIGIT_MODE_PER_WORKSPACE;
    original.slot_overlay_duration_ms = 1500;
    original.ripple_enabled = 0;
    strncpy(original.hotkey_windows, "Mod4+w", sizeof(original.hotkey_windows) - 1);
    strncpy(original.hotkey_command, "Mod4+space", sizeof(original.hotkey_command) - 1);
    strncpy(original.hotkey_workspaces, "", sizeof(original.hotkey_workspaces) - 1);

    save_config(&original);
    load_config(&loaded);

    ASSERT_INT("nondefault: close_on_focus_loss", 0, loaded.close_on_focus_loss);
    ASSERT_INT("nondefault: alignment", ALIGN_BOTTOM_RIGHT, loaded.alignment);
    ASSERT_INT("nondefault: workspaces_per_row", 4, loaded.workspaces_per_row);
    ASSERT_INT("nondefault: tile_columns", 3, loaded.tile_columns);
    ASSERT_INT("nondefault: digit_slot_mode", DIGIT_MODE_PER_WORKSPACE, loaded.digit_slot_mode);
    ASSERT_INT("nondefault: slot_overlay_duration_ms", 1500, loaded.slot_overlay_duration_ms);
    ASSERT_INT("nondefault: ripple_enabled", 0, loaded.ripple_enabled);
    ASSERT_STR("nondefault: hotkey_windows", "Mod4+w", loaded.hotkey_windows);
    ASSERT_STR("nondefault: hotkey_command", "Mod4+space", loaded.hotkey_command);
    ASSERT_STR("nondefault: hotkey_workspaces", "", loaded.hotkey_workspaces);
}

// Test 3: all alignment values round-trip
static void test_all_alignments(void) {
    WindowAlignment alignments[] = {
        ALIGN_CENTER, ALIGN_TOP, ALIGN_TOP_LEFT, ALIGN_TOP_RIGHT,
        ALIGN_LEFT, ALIGN_RIGHT, ALIGN_BOTTOM, ALIGN_BOTTOM_LEFT, ALIGN_BOTTOM_RIGHT
    };
    const char *names[] = {
        "center", "top", "top_left", "top_right",
        "left", "right", "bottom", "bottom_left", "bottom_right"
    };
    int count = sizeof(alignments) / sizeof(alignments[0]);

    for (int i = 0; i < count; i++) {
        CofiConfig original, loaded;
        init_config_defaults(&original);
        original.alignment = alignments[i];
        save_config(&original);
        load_config(&loaded);

        char label[64];
        snprintf(label, sizeof(label), "alignment: %s", names[i]);
        ASSERT_INT(label, alignments[i], loaded.alignment);
    }
}

// Test 4: all digit slot modes round-trip
static void test_all_digit_modes(void) {
    DigitSlotMode modes[] = { DIGIT_MODE_DEFAULT, DIGIT_MODE_PER_WORKSPACE, DIGIT_MODE_WORKSPACES };
    const char *names[] = { "default", "per-workspace", "workspaces" };
    int count = sizeof(modes) / sizeof(modes[0]);

    for (int i = 0; i < count; i++) {
        CofiConfig original, loaded;
        init_config_defaults(&original);
        original.digit_slot_mode = modes[i];
        save_config(&original);
        load_config(&loaded);

        char label[64];
        snprintf(label, sizeof(label), "digit_mode: %s", names[i]);
        ASSERT_INT(label, modes[i], loaded.digit_slot_mode);
    }
}

int main(void) {
    // Use temp dir so we don't clobber real config
    char tmpdir[] = "/tmp/cofi_test_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        fprintf(stderr, "Failed to create temp dir\n");
        return 1;
    }
    setenv("HOME", tmpdir, 1);

    printf("Config round-trip tests\n");
    printf("=======================\n\n");

    test_defaults_roundtrip();
    test_nondefault_roundtrip();
    test_all_alignments();
    test_all_digit_modes();

    printf("\n=====================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);

    // Cleanup
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);

    return tests_failed > 0 ? 1 : 0;
}
