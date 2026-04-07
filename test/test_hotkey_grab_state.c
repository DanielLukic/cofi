#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/hotkeys.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("  PASS: %s\n", name); pass++; } \
    else { printf("  FAIL: %s\n", name); fail++; } \
} while (0)

static void test_init_hotkey_grab_state_resets_fields(void) {
    printf("\n--- init_hotkey_grab_state resets fields ---\n");

    HotkeyGrabState state;
    memset(&state, 0xAB, sizeof(state));

    init_hotkey_grab_state(&state);

    ASSERT_TRUE("grabbed_count reset to zero", state.grabbed_count == 0);
    ASSERT_TRUE("first key name cleared", state.grabbed_hotkeys[0].key_name[0] == '\0');
    ASSERT_TRUE("first command cleared", state.grabbed_hotkeys[0].command[0] == '\0');
}

static void test_app_data_exposes_hotkey_grab_state(void) {
    printf("\n--- AppData exposes hotkey_grab_state ---\n");

    AppData app;
    memset(&app, 0, sizeof(app));

    app.hotkey_grab_state.grabbed_count = 7;
    init_hotkey_grab_state(&app.hotkey_grab_state);

    ASSERT_TRUE("AppData owns resettable grab state", app.hotkey_grab_state.grabbed_count == 0);
}

int main(void) {
    printf("Hotkey grab state tests\n");
    printf("=======================\n");

    test_init_hotkey_grab_state_resets_fields();
    test_app_data_exposes_hotkey_grab_state();

    printf("\n=== Summary: %d/%d passed ===\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
