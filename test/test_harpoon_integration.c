#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../src/harpoon.h"
#include "../src/harpoon_config.h"
#include "../src/match_entry.h"
#include "../src/match_entry_config.h"
#include "../src/utils.h"

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

static void set_test_home(const char *suffix) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/cofi-harpoon-int-%s-%ld", suffix, (long)getpid());
    mkdir(path, 0755);
    setenv("HOME", path, 1);
}

static void wire_harpoon_context(HarpoonManager *harpoon,
                                 MatchEntryManager *matching,
                                 WindowInfo *windows,
                                 int *window_count) {
    harpoon->matching = matching;
    harpoon->windows = windows;
    harpoon->window_count = window_count;
}

static void test_glob_slot_survives_title_drift_but_exact_does_not(void) {
    set_test_home("glob-drift");

    MatchEntryManager matching;
    HarpoonManager harpoon;
    WindowInfo windows[MAX_WINDOWS] = {0};
    int window_count = 1;

    match_entry_manager_init(&matching);
    init_harpoon_manager(&harpoon);
    wire_harpoon_context(&harpoon, &matching, windows, &window_count);

    windows[0] = make_window(0x111, "cofi | main - Terminal", "Kitty", "kitty", "Normal");
    assign_window_to_slot(&harpoon, 3, &windows[0]);
    ASSERT_TRUE("slot assigned", harpoon.slots[3].assigned == 1);

    int idx = match_entry_find_index_by_match_id(&matching, harpoon.slots[3].match_id);
    ASSERT_TRUE("match entry exists", idx >= 0);

    safe_string_copy(matching.entries[idx].original_title, "cofi*Terminal",
                     sizeof(matching.entries[idx].original_title));
    matching.entries[idx].match_mode = TITLE_MATCH_MODE_GLOB;
    matching.entries[idx].bound_x11_id = 0;
    matching.entries[idx].assigned = 1;

    windows[0] = make_window(0x222, "cofi | feature - Terminal", "Kitty", "kitty", "Normal");
    Window resolved = get_slot_window(&harpoon, 3);
    ASSERT_TRUE("glob entry rebinds after title drift", resolved == 0x222);

    matching.entries[idx].match_mode = TITLE_MATCH_MODE_EXACT;
    matching.entries[idx].bound_x11_id = 0;
    matching.entries[idx].assigned = 1;
    resolved = get_slot_window(&harpoon, 3);
    ASSERT_TRUE("exact entry does not match drifted title", resolved == 0);
}

static void test_slot_rebinds_after_window_reopens_with_new_id(void) {
    set_test_home("reopen");

    MatchEntryManager matching;
    HarpoonManager harpoon;
    WindowInfo windows[MAX_WINDOWS] = {0};
    int window_count = 1;

    match_entry_manager_init(&matching);
    init_harpoon_manager(&harpoon);
    wire_harpoon_context(&harpoon, &matching, windows, &window_count);

    windows[0] = make_window(0x333, "SampleApp", "SampleApp", "sampleapp", "Normal");
    assign_window_to_slot(&harpoon, 4, &windows[0]);
    ASSERT_TRUE("slot assigned for reopen test", harpoon.slots[4].assigned == 1);

    int idx = match_entry_find_index_by_match_id(&matching, harpoon.slots[4].match_id);
    ASSERT_TRUE("reopen entry exists", idx >= 0);

    matching.entries[idx].bound_x11_id = 0;
    matching.entries[idx].assigned = 1;
    windows[0] = make_window(0x444, "SampleApp", "SampleApp", "sampleapp", "Normal");

    Window resolved = get_slot_window(&harpoon, 4);
    ASSERT_TRUE("slot resolves to reopened window id", resolved == 0x444);
    ASSERT_TRUE("entry updated with reopened id", matching.entries[idx].bound_x11_id == 0x444);
}

static void test_match_id_persists_and_reloads_with_rebind(void) {
    set_test_home("persist");

    MatchEntryManager matching;
    HarpoonManager harpoon;
    WindowInfo windows[MAX_WINDOWS] = {0};
    int window_count = 1;

    match_entry_manager_init(&matching);
    init_harpoon_manager(&harpoon);
    wire_harpoon_context(&harpoon, &matching, windows, &window_count);

    windows[0] = make_window(0x555, "Persisted", "PersistApp", "persist", "Normal");
    assign_window_to_slot(&harpoon, 2, &windows[0]);
    int saved_match_id = harpoon.slots[2].match_id;
    ASSERT_TRUE("saved match_id assigned", saved_match_id > 0);

    save_match_entries(&matching);
    save_harpoon_slots(&harpoon);

    MatchEntryManager loaded_matching;
    HarpoonManager loaded_harpoon;
    WindowInfo loaded_windows[MAX_WINDOWS] = {0};
    int loaded_count = 1;

    match_entry_manager_init(&loaded_matching);
    init_harpoon_manager(&loaded_harpoon);
    wire_harpoon_context(&loaded_harpoon, &loaded_matching, loaded_windows, &loaded_count);

    load_match_entries(&loaded_matching);
    load_harpoon_slots(&loaded_harpoon);

    ASSERT_TRUE("slot match_id persisted in harpoon json",
                loaded_harpoon.slots[2].match_id == saved_match_id);
    int idx = match_entry_find_index_by_match_id(&loaded_matching, saved_match_id);
    ASSERT_TRUE("matching.json contains persisted match_id", idx >= 0);

    loaded_matching.entries[idx].bound_x11_id = 0;
    loaded_matching.entries[idx].assigned = 1;
    loaded_windows[0] = make_window(0x666, "Persisted", "PersistApp", "persist", "Normal");
    Window resolved = get_slot_window(&loaded_harpoon, 2);
    ASSERT_TRUE("reloaded slot rebinds to live window", resolved == 0x666);
}

int main(void) {
    printf("Harpoon integration tests\n");
    printf("=========================\n\n");

    test_glob_slot_survives_title_drift_but_exact_does_not();
    test_slot_rebinds_after_window_reopens_with_new_id();
    test_match_id_persists_and_reloads_with_rebind();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
