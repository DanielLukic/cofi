#include <stdio.h>
#include <string.h>
#include <X11/Xutil.h>

#include "x11/size_hints.h"

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

static WindowSizeHints make_hints(void) {
    WindowSizeHints hints;

    memset(&hints, 0, sizeof(hints));
    hints.min_width = 1;
    hints.min_height = 1;
    hints.max_width = 10000;
    hints.max_height = 10000;
    hints.width_inc = 1;
    hints.height_inc = 1;
    return hints;
}

static void test_resize_increment_does_not_snap_height(void) {
    int x = 0;
    int y = 0;
    int width = 1200;
    int height = 2018;
    WindowSizeHints hints = make_hints();

    hints.flags = PBaseSize | PResizeInc;
    hints.base_height = 4;
    hints.height_inc = 48;

    ensure_size_hints_satisfied(&x, &y, &width, &height, &hints);

    ASSERT_INT("height stays unsnapped when only resize increment applies", 2018, height);
}

static void test_minimum_height_still_overrides_increment_case(void) {
    int x = 0;
    int y = 0;
    int width = 1200;
    int height = 2018;
    WindowSizeHints hints = make_hints();

    hints.flags = PBaseSize | PResizeInc | PMinSize;
    hints.base_height = 4;
    hints.height_inc = 48;
    hints.min_height = 2200;

    ensure_size_hints_satisfied(&x, &y, &width, &height, &hints);

    ASSERT_INT("minimum height still clamps", 2200, height);
}

static void test_clear_resize_increment_hints_clears_flag_and_values(void) {
    XSizeHints hints;

    memset(&hints, 0, sizeof(hints));
    hints.flags = PResizeInc;
    hints.width_inc = 20;
    hints.height_inc = 48;

    clear_resize_increment_hints_from_xsizehints(&hints);

    ASSERT_INT("resize increment flag cleared", 0, (int)(hints.flags & PResizeInc));
    ASSERT_INT("width increment cleared", 0, hints.width_inc);
    ASSERT_INT("height increment cleared", 0, hints.height_inc);
}

static void test_clear_resize_increment_hints_is_noop_without_flag(void) {
    XSizeHints hints;

    memset(&hints, 0, sizeof(hints));
    hints.flags = PMinSize;
    hints.min_width = 100;
    hints.min_height = 200;
    hints.width_inc = 20;
    hints.height_inc = 48;

    clear_resize_increment_hints_from_xsizehints(&hints);

    ASSERT_INT("flags unchanged without resize increment", PMinSize, (int)hints.flags);
    ASSERT_INT("width increment unchanged without flag", 20, hints.width_inc);
    ASSERT_INT("height increment unchanged without flag", 48, hints.height_inc);
    ASSERT_INT("min width unchanged without flag", 100, hints.min_width);
    ASSERT_INT("min height unchanged without flag", 200, hints.min_height);
}

int main(void) {
    printf("Size hints tests\n");
    printf("================\n\n");

    test_resize_increment_does_not_snap_height();
    test_minimum_height_still_overrides_increment_case();
    test_clear_resize_increment_hints_clears_flag_and_values();
    test_clear_resize_increment_hints_is_noop_without_flag();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
