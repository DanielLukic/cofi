#include <stdio.h>

#include "../src/layout_patterns.h"
#include "../src/size_hints.h"
#include "../src/tiling.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_EQ_INT(msg, expected, actual) \
    do { \
        tests_run++; \
        if ((expected) == (actual)) { \
            tests_passed++; \
            printf("PASS: %s\n", msg); \
        } else { \
            printf("FAIL: %s (expected %d, got %d)\n", msg, (expected), (actual)); \
        } \
    } while (0)

int get_window_size_hints(Display *display, Window window, WindowSizeHints *hints) {
    (void)display;
    (void)window;
    (void)hints;
    return 0;
}

void apply_window_position(Display *display, Window window_id,
                           const TileGeometry *geometry,
                           const WindowSizeHints *size_hints) {
    (void)display;
    (void)window_id;
    (void)geometry;
    (void)size_hints;
}

static void test_single_window_gets_left_half(void) {
    WorkArea monitor = { .x = 100, .y = 50, .width = 1000, .height = 800 };
    Window windows[] = { 11 };
    LayoutTarget targets[4];

    int count = calculate_main_stack_targets(windows, 1, 11, &monitor, targets, 4);

    ASSERT_EQ_INT("single window target count", 1, count);
    ASSERT_EQ_INT("single window id", 11, (int)targets[0].window_id);
    ASSERT_EQ_INT("single window x", 100, targets[0].geometry.x);
    ASSERT_EQ_INT("single window y", 50, targets[0].geometry.y);
    ASSERT_EQ_INT("single window width", 500, targets[0].geometry.width);
    ASSERT_EQ_INT("single window height", 800, targets[0].geometry.height);
}

static void test_two_windows_primary_first(void) {
    WorkArea monitor = { .x = 0, .y = 0, .width = 1200, .height = 900 };
    Window windows[] = { 21, 22 };
    LayoutTarget targets[4];

    int count = calculate_main_stack_targets(windows, 2, 22, &monitor, targets, 4);

    ASSERT_EQ_INT("two windows target count", 2, count);
    ASSERT_EQ_INT("primary moved to first target", 22, (int)targets[0].window_id);
    ASSERT_EQ_INT("primary width half", 600, targets[0].geometry.width);
    ASSERT_EQ_INT("stack window id", 21, (int)targets[1].window_id);
    ASSERT_EQ_INT("stack window x", 600, targets[1].geometry.x);
    ASSERT_EQ_INT("stack window width", 600, targets[1].geometry.width);
    ASSERT_EQ_INT("stack window fills full height", 900, targets[1].geometry.height);
}

static void test_stack_windows_split_evenly_with_remainder(void) {
    WorkArea monitor = { .x = 10, .y = 20, .width = 1000, .height = 800 };
    Window windows[] = { 1, 2, 3, 4 };
    LayoutTarget targets[8];

    int count = calculate_main_stack_targets(windows, 4, 3, &monitor, targets, 8);

    ASSERT_EQ_INT("four windows target count", 4, count);
    ASSERT_EQ_INT("primary id preserved", 3, (int)targets[0].window_id);
    ASSERT_EQ_INT("stack first id", 1, (int)targets[1].window_id);
    ASSERT_EQ_INT("stack second id", 2, (int)targets[2].window_id);
    ASSERT_EQ_INT("stack third id", 4, (int)targets[3].window_id);
    ASSERT_EQ_INT("stack first y", 20, targets[1].geometry.y);
    ASSERT_EQ_INT("stack second y", 287, targets[2].geometry.y);
    ASSERT_EQ_INT("stack third y", 554, targets[3].geometry.y);
    ASSERT_EQ_INT("stack first height", 267, targets[1].geometry.height);
    ASSERT_EQ_INT("stack second height", 267, targets[2].geometry.height);
    ASSERT_EQ_INT("stack third height", 266, targets[3].geometry.height);
}

static void test_unknown_primary_falls_back_to_first_window(void) {
    WorkArea monitor = { .x = 0, .y = 0, .width = 900, .height = 600 };
    Window windows[] = { 31, 32, 33 };
    LayoutTarget targets[4];

    int count = calculate_main_stack_targets(windows, 3, 999, &monitor, targets, 4);

    ASSERT_EQ_INT("fallback target count", 3, count);
    ASSERT_EQ_INT("fallback primary is first window", 31, (int)targets[0].window_id);
}

int main(void) {
    printf("Layout pattern geometry tests\n");
    printf("=============================\n\n");

    test_single_window_gets_left_half();
    test_two_windows_primary_first();
    test_stack_windows_split_evenly_with_remainder();
    test_unknown_primary_falls_back_to_first_window();

    printf("\n=============================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_run == tests_passed ? 0 : 1;
}
