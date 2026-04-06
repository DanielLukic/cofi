#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gdk/gdk.h>

#include "../src/app_data.h"
#include "../src/overlay_manager.h"
#include "../src/overlay_hotkey_add.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("  PASS: %s\n", name); pass++; } \
    else { printf("  FAIL: %s\n", name); fail++; } \
} while (0)

static GdkEventKey make_key_event(guint keyval, GdkModifierType state) {
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = keyval;
    event.state = state;
    return event;
}

static void test_overlay_should_focus_name_entry(void) {
    printf("\n--- overlay_should_focus_name_entry ---\n");

    ASSERT_TRUE("name assign focuses entry", overlay_should_focus_name_entry(OVERLAY_NAME_ASSIGN));
    ASSERT_TRUE("name edit focuses entry", overlay_should_focus_name_entry(OVERLAY_NAME_EDIT));
    ASSERT_TRUE("config edit focuses entry", overlay_should_focus_name_entry(OVERLAY_CONFIG_EDIT));
    ASSERT_TRUE("hotkey add focuses entry", overlay_should_focus_name_entry(OVERLAY_HOTKEY_ADD));
    ASSERT_TRUE("hotkey edit focuses entry", overlay_should_focus_name_entry(OVERLAY_HOTKEY_EDIT));

    ASSERT_TRUE("tiling does not focus entry", !overlay_should_focus_name_entry(OVERLAY_TILING));
    ASSERT_TRUE("harpoon edit does not focus entry", !overlay_should_focus_name_entry(OVERLAY_HARPOON_EDIT));
    ASSERT_TRUE("workspace rename does not focus entry", !overlay_should_focus_name_entry(OVERLAY_WORKSPACE_RENAME));
}

static void test_hotkey_add_capture_policy(void) {
    printf("\n--- overlay_hotkey_add_should_capture_event ---\n");

    GdkEventKey esc = make_key_event(GDK_KEY_Escape, 0);
    ASSERT_TRUE("Escape is not capture", !overlay_hotkey_add_should_capture_event(&esc));

    GdkEventKey enter = make_key_event(GDK_KEY_Return, 0);
    ASSERT_TRUE("plain Enter is not capture", !overlay_hotkey_add_should_capture_event(&enter));

    GdkEventKey ctrl_enter = make_key_event(GDK_KEY_Return, GDK_CONTROL_MASK);
    ASSERT_TRUE("Ctrl+Enter captures", overlay_hotkey_add_should_capture_event(&ctrl_enter));

    GdkEventKey plain_a = make_key_event(GDK_KEY_a, 0);
    ASSERT_TRUE("plain printable does not capture", !overlay_hotkey_add_should_capture_event(&plain_a));

    GdkEventKey shift_a = make_key_event(GDK_KEY_A, GDK_SHIFT_MASK);
    ASSERT_TRUE("Shift+printable does not capture", !overlay_hotkey_add_should_capture_event(&shift_a));

    GdkEventKey shift_tab = make_key_event(GDK_KEY_Tab, GDK_SHIFT_MASK);
    ASSERT_TRUE("Shift+Tab captures", overlay_hotkey_add_should_capture_event(&shift_tab));

    GdkEventKey f1 = make_key_event(GDK_KEY_F1, 0);
    ASSERT_TRUE("F1 captures", overlay_hotkey_add_should_capture_event(&f1));
}

int main(void) {
    printf("Overlay dispatch tests\n");
    printf("======================\n");

    test_overlay_should_focus_name_entry();
    test_hotkey_add_capture_policy();

    printf("\n=== Summary: %d/%d passed ===\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
