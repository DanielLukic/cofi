#include <stdio.h>
#include <string.h>

#include "harpoon/harpoon.h"
#include "matching/match_entry.h"
#include "core/utils/utils.h"

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

static WindowInfo make_window(Window id,
                              const char *title,
                              const char *class_name,
                              const char *instance,
                              const char *type) {
    WindowInfo w = {0};
    w.id = id;
    safe_string_copy(w.title, title, MAX_TITLE_LEN);
    safe_string_copy(w.class_name, class_name, MAX_CLASS_LEN);
    safe_string_copy(w.instance, instance, MAX_CLASS_LEN);
    safe_string_copy(w.type, type, sizeof(w.type));
    return w;
}

static void test_harpoon_resolve_after_client_list_change(void) {
    MatchEntryManager matching;
    HarpoonManager harpoon;
    WindowInfo windows[MAX_WINDOWS] = {0};
    int window_count = 1;

    match_entry_manager_init(&matching);
    init_harpoon_manager(&harpoon);
    harpoon.matching = &matching;
    harpoon.windows = windows;
    harpoon.window_count = &window_count;

    windows[0] = make_window(0x100, "SampleApp", "SampleApp", "sampleapp", "Normal");
    assign_window_to_slot(&harpoon, 3, &windows[0]);
    ASSERT_TRUE("slot assigned", harpoon.slots[3].assigned == 1);

    windows[0] = make_window(0x200, "SampleApp", "SampleApp", "sampleapp", "Normal");
    match_entry_reassign_live_windows(&matching, windows, window_count);
    Window resolved = get_slot_window(&harpoon, 3);
    ASSERT_TRUE("slot resolves to new window id", resolved == 0x200);
    ASSERT_TRUE("new window maps back to slot 3", get_window_slot(&harpoon, 0x200) == 3);
}

static void test_harpoon_does_not_rebind_mismatched_window(void) {
    MatchEntryManager matching;
    HarpoonManager harpoon;
    WindowInfo windows[MAX_WINDOWS] = {0};
    int window_count = 1;

    match_entry_manager_init(&matching);
    init_harpoon_manager(&harpoon);
    harpoon.matching = &matching;
    harpoon.windows = windows;
    harpoon.window_count = &window_count;

    windows[0] = make_window(0x300, "SampleApp", "SampleApp", "sampleapp", "Normal");
    assign_window_to_slot(&harpoon, 4, &windows[0]);
    ASSERT_TRUE("slot assigned for mismatch case", harpoon.slots[4].assigned == 1);

    windows[0] = make_window(0x400, "Other", "OtherClass", "other", "Normal");
    match_entry_reassign_live_windows(&matching, windows, window_count);
    Window resolved = get_slot_window(&harpoon, 4);
    ASSERT_TRUE("mismatched window does not resolve", resolved == 0);
    ASSERT_TRUE("mismatched window not mapped to slot", get_window_slot(&harpoon, 0x400) == -1);
}

int main(void) {
    printf("Event sequence tests\n");
    printf("====================\n\n");

    test_harpoon_resolve_after_client_list_change();
    test_harpoon_does_not_rebind_mismatched_window();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
