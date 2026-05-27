#include <stdio.h>

#include "core/nav_keys/nav_keys.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); tests_passed++; } \
    else { printf("FAIL: %s\n", name); tests_failed++; } \
} while (0)

static GdkEventKey make_key(guint keyval, GdkModifierType state) {
    GdkEventKey event = {0};
    event.keyval = keyval;
    event.state = state;
    return event;
}

static void test_up_arrow_maps_to_nav_up(void) {
    GdkEventKey event = make_key(GDK_KEY_Up, 0);
    ASSERT_TRUE("Up arrow -> NAV_UP", nav_direction_from_key(&event) == NAV_UP);
}

static void test_down_arrow_maps_to_nav_down(void) {
    GdkEventKey event = make_key(GDK_KEY_Down, 0);
    ASSERT_TRUE("Down arrow -> NAV_DOWN", nav_direction_from_key(&event) == NAV_DOWN);
}

static void test_ctrl_k_maps_to_nav_up(void) {
    GdkEventKey event = make_key(GDK_KEY_k, GDK_CONTROL_MASK);
    ASSERT_TRUE("Ctrl+k -> NAV_UP", nav_direction_from_key(&event) == NAV_UP);
}

static void test_ctrl_j_maps_to_nav_down(void) {
    GdkEventKey event = make_key(GDK_KEY_j, GDK_CONTROL_MASK);
    ASSERT_TRUE("Ctrl+j -> NAV_DOWN", nav_direction_from_key(&event) == NAV_DOWN);
}

static void test_bare_j_is_nav_none(void) {
    GdkEventKey event = make_key(GDK_KEY_j, 0);
    ASSERT_TRUE("j (no Ctrl) -> NAV_NONE", nav_direction_from_key(&event) == NAV_NONE);
}

static void test_bare_k_is_nav_none(void) {
    GdkEventKey event = make_key(GDK_KEY_k, 0);
    ASSERT_TRUE("k (no Ctrl) -> NAV_NONE", nav_direction_from_key(&event) == NAV_NONE);
}

static void test_unrelated_key_is_nav_none(void) {
    GdkEventKey event = make_key(GDK_KEY_a, 0);
    ASSERT_TRUE("a -> NAV_NONE", nav_direction_from_key(&event) == NAV_NONE);
}

static void test_ctrl_j_with_extra_modifier_maps_to_nav_down(void) {
    GdkEventKey event = make_key(GDK_KEY_j, GDK_CONTROL_MASK | GDK_SHIFT_MASK);
    ASSERT_TRUE("Ctrl+Shift+j -> NAV_DOWN", nav_direction_from_key(&event) == NAV_DOWN);
}

int main(void) {
    printf("Nav key mapping tests\n");
    printf("=====================\n\n");

    test_up_arrow_maps_to_nav_up();
    test_down_arrow_maps_to_nav_down();
    test_ctrl_k_maps_to_nav_up();
    test_ctrl_j_maps_to_nav_down();
    test_bare_j_is_nav_none();
    test_bare_k_is_nav_none();
    test_unrelated_key_is_nav_none();
    test_ctrl_j_with_extra_modifier_maps_to_nav_down();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
