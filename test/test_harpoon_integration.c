#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../src/app_data.h"
#include "../src/harpoon.h"
#include "../src/harpoon_config.h"
#include "../src/layout_store.h"
#include "../src/match_entry.h"
#include "../src/match_entry_config.h"
#include "../src/matching_gc.h"
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

static int load_saved_matching_count(void) {
    MatchEntryManager loaded;
    load_match_entries(&loaded);
    return loaded.count;
}

static void init_gc_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    match_entry_manager_init(&app->matching);
    init_harpoon_manager(&app->harpoon);
    layout_store_init(&app->layouts);
    app->harpoon.matching = &app->matching;
    app->harpoon.windows = app->windows;
    app->harpoon.window_count = &app->window_count;
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

static void test_slot_rebinds_after_orphaned_reopen_cycle(void) {
    set_test_home("reopen-orphan");

    MatchEntryManager matching;
    HarpoonManager harpoon;
    WindowInfo windows[MAX_WINDOWS] = {0};
    int window_count = 1;

    match_entry_manager_init(&matching);
    init_harpoon_manager(&harpoon);
    wire_harpoon_context(&harpoon, &matching, windows, &window_count);

    windows[0] = make_window(0x777, "cofi", "Kitty", "kitty", "Normal");
    int match_id = matching_capture_or_get(&matching, windows, window_count, &windows[0]);
    ASSERT_TRUE("orphan cycle captured match entry", match_id > 0);

    int idx = match_entry_find_index_by_match_id(&matching, match_id);
    ASSERT_TRUE("orphan cycle entry exists", idx >= 0);

    safe_string_copy(matching.entries[idx].original_title, "cofi*",
                     sizeof(matching.entries[idx].original_title));
    matching.entries[idx].match_mode = TITLE_MATCH_MODE_GLOB;
    matching.entries[idx].assigned = 1;

    harpoon.slots[2].assigned = 1;
    harpoon.slots[2].match_id = match_id;

    window_count = 0;
    ASSERT_TRUE("close pass reports change",
                match_entry_reassign_live_windows(&matching, windows, window_count));
    ASSERT_TRUE("entry orphaned after close", matching.entries[idx].assigned == 0);
    ASSERT_TRUE("entry binding cleared after close", matching.entries[idx].bound_x11_id == 0);

    window_count = 1;
    windows[0] = make_window(0x888, "cofi reopened", "Kitty", "kitty", "Normal");
    ASSERT_TRUE("reopen pass reports change",
                match_entry_reassign_live_windows(&matching, windows, window_count));
    ASSERT_TRUE("entry rebound after reopen", matching.entries[idx].assigned == 1);
    ASSERT_TRUE("entry updated with reopened window id", matching.entries[idx].bound_x11_id == 0x888);
    ASSERT_TRUE("harpoon slot resolves reopened window", get_slot_window(&harpoon, 2) == 0x888);
}

static void test_unassign_gc_removes_unlabeled_unreferenced_entry(void) {
    set_test_home("gc-unassign");

    AppData app;
    init_gc_app(&app);
    app.window_count = 1;
    app.windows[0] = make_window(0x901, "Harpoon Me", "Kitty", "kitty", "Normal");
    assign_window_to_slot(&app.harpoon, 1, &app.windows[0]);
    ASSERT_TRUE("gc test created one match entry", app.matching.count == 1);
    ASSERT_TRUE("gc test entry is unlabeled", app.matching.entries[0].custom_name[0] == '\0');

    unassign_slot(&app.harpoon, 1);
    ASSERT_TRUE("gc removes unlabeled unreferenced entry",
                matching_run_gc(&app) == 1);
    ASSERT_TRUE("manager count drops to zero", app.matching.count == 0);

    save_match_entries(&app.matching);
    ASSERT_TRUE("matching.json saved empty after gc", load_saved_matching_count() == 0);
}

static void test_gc_keeps_labeled_entry_without_harpoon_reference(void) {
    set_test_home("gc-labeled");

    AppData app;
    init_gc_app(&app);
    app.window_count = 1;
    app.windows[0] = make_window(0x902, "Keep Me", "Kitty", "kitty", "Normal");
    assign_window_to_slot(&app.harpoon, 2, &app.windows[0]);
    safe_string_copy(app.matching.entries[0].custom_name, "named", sizeof(app.matching.entries[0].custom_name));

    unassign_slot(&app.harpoon, 2);
    ASSERT_TRUE("labeled entry survives gc", matching_run_gc(&app) == 0);
    ASSERT_TRUE("labeled entry remains present", app.matching.count == 1);
    ASSERT_TRUE("labeled entry name preserved", strcmp(app.matching.entries[0].custom_name, "named") == 0);
}

static void test_gc_keeps_layout_only_entry_without_harpoon_reference(void) {
    set_test_home("gc-layout");

    AppData app;
    init_gc_app(&app);
    app.window_count = 1;
    app.windows[0] = make_window(0x90A, "Keep Layout", "Kitty", "kitty", "Normal");
    assign_window_to_slot(&app.harpoon, 2, &app.windows[0]);
    ASSERT_TRUE("layout-only record stored", layout_store_set(&app.layouts,
                app.harpoon.slots[2].match_id, 1, 2, 300, 200, 4) == true);

    unassign_slot(&app.harpoon, 2);
    ASSERT_TRUE("layout-only entry survives gc", matching_run_gc(&app) == 0);
    ASSERT_TRUE("layout-only entry remains present", app.matching.count == 1);
    ASSERT_TRUE("layout id preserved", app.layouts.records[0].match_id == app.matching.entries[0].match_id);
}

static void test_gc_keeps_entry_referenced_by_live_harpoon_slot(void) {
    set_test_home("gc-referenced");

    AppData app;
    init_gc_app(&app);
    app.window_count = 1;
    app.windows[0] = make_window(0x903, "Still Referenced", "Kitty", "kitty", "Normal");
    assign_window_to_slot(&app.harpoon, 3, &app.windows[0]);

    ASSERT_TRUE("referenced unlabeled entry survives gc", matching_run_gc(&app) == 0);
    ASSERT_TRUE("referenced entry remains present", app.matching.count == 1);
    ASSERT_TRUE("referenced slot still resolves", get_slot_window(&app.harpoon, 3) == 0x903);
}

static void test_window_close_keeps_entry_while_slot_still_references_it(void) {
    set_test_home("gc-close");

    AppData app;
    init_gc_app(&app);
    app.window_count = 1;
    app.windows[0] = make_window(0x904, "Close Me", "Kitty", "kitty", "Normal");
    assign_window_to_slot(&app.harpoon, 4, &app.windows[0]);
    ASSERT_TRUE("close test entry created", app.matching.count == 1);

    app.window_count = 0;
    ASSERT_TRUE("close orphaning pass reports change",
                match_entry_reassign_live_windows(&app.matching, app.windows, app.window_count));
    ASSERT_TRUE("close leaves slot assigned", app.harpoon.slots[4].assigned == 1);
    ASSERT_TRUE("close clears live binding", app.matching.entries[0].bound_x11_id == 0);
    ASSERT_TRUE("close keeps entry during gc", matching_run_gc(&app) == 0);
    ASSERT_TRUE("close keeps entry in manager", app.matching.count == 1);
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
    test_slot_rebinds_after_orphaned_reopen_cycle();
    test_unassign_gc_removes_unlabeled_unreferenced_entry();
    test_gc_keeps_labeled_entry_without_harpoon_reference();
    test_gc_keeps_layout_only_entry_without_harpoon_reference();
    test_gc_keeps_entry_referenced_by_live_harpoon_slot();
    test_window_close_keeps_entry_while_slot_still_references_it();
    test_match_id_persists_and_reloads_with_rebind();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
