#include <stdio.h>

#include "../src/window_appearance.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(name, cond) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
        printf("PASS: %s\n", name); \
    } else { \
        printf("FAIL: %s (line %d)\n", name, __LINE__); \
    } \
} while (0)

static WindowInfo make_window(Window id) {
    WindowInfo window = {0};
    window.id = id;
    return window;
}

static void test_collect_new_window_ids_detects_only_added_ids(void) {
    Window old_ids[] = {0x100, 0x200, 0x300};
    WindowInfo windows[] = {
        make_window(0x200),
        make_window(0x400),
        make_window(0x300),
        make_window(0x500),
    };
    Window new_ids[4] = {0};

    int count = collect_new_window_ids(old_ids, 3, windows, 4, new_ids, 4);
    ASSERT_TRUE("two ids detected as new", count == 2);
    ASSERT_TRUE("first new id preserves new-list order", new_ids[0] == 0x400);
    ASSERT_TRUE("second new id preserves new-list order", new_ids[1] == 0x500);
}

static void test_collect_new_window_ids_handles_empty_previous_snapshot(void) {
    WindowInfo windows[] = {
        make_window(0x111),
        make_window(0x222),
    };
    Window new_ids[2] = {0};

    int count = collect_new_window_ids(NULL, 0, windows, 2, new_ids, 2);
    ASSERT_TRUE("all ids are new with empty snapshot", count == 2);
    ASSERT_TRUE("first id copied", new_ids[0] == 0x111);
    ASSERT_TRUE("second id copied", new_ids[1] == 0x222);
}

static void test_collect_new_window_ids_respects_output_capacity(void) {
    Window old_ids[] = {0x100};
    WindowInfo windows[] = {
        make_window(0x200),
        make_window(0x300),
        make_window(0x400),
    };
    Window new_ids[2] = {0};

    int count = collect_new_window_ids(old_ids, 1, windows, 3, new_ids, 2);
    ASSERT_TRUE("new id list is capped", count == 2);
    ASSERT_TRUE("first capped id copied", new_ids[0] == 0x200);
    ASSERT_TRUE("second capped id copied", new_ids[1] == 0x300);
}

static void test_collect_new_window_ids_ignores_zero_ids(void) {
    Window old_ids[] = {0x100};
    WindowInfo windows[] = {
        make_window(0),
        make_window(0x100),
        make_window(0x200),
    };
    Window new_ids[3] = {0};

    int count = collect_new_window_ids(old_ids, 1, windows, 3, new_ids, 3);
    ASSERT_TRUE("zero and existing ids ignored", count == 1);
    ASSERT_TRUE("remaining new id copied", new_ids[0] == 0x200);
}

int main(void) {
    printf("Window appearance helper tests\n");
    printf("==============================\n\n");

    test_collect_new_window_ids_detects_only_added_ids();
    test_collect_new_window_ids_handles_empty_previous_snapshot();
    test_collect_new_window_ids_respects_output_capacity();
    test_collect_new_window_ids_ignores_zero_ids();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
