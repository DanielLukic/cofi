#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../src/frame_extents.h"
#include "../src/layout_store.h"
#include "../src/match_entry.h"
#include "../src/match_entry_config.h"
#include "../src/window_geometry_matching.h"
#include "../src/window_matcher.h"
#include "../src/x11_utils.h"
#include "../src/utils.h"

WindowInfo *get_selected_window(AppData *app) { (void)app; return NULL; }
static int g_geom_x = 11;
static int g_geom_y = 22;
static int g_geom_w = 333;
static int g_geom_h = 444;
static int g_geom_desktop = 5;
gboolean get_window_geometry(Display *display, Window window, int *x, int *y, int *width, int *height) {
    (void)display;
    (void)window;
    if (x) *x = g_geom_x;
    if (y) *y = g_geom_y;
    if (width) *width = g_geom_w;
    if (height) *height = g_geom_h;
    return TRUE;
}
int get_window_desktop(Display *display, Window window) { (void)display; (void)window; return g_geom_desktop; }
void move_window_to_desktop(Display *display, Window window, int desktop_index) {
    (void)display; (void)window; (void)desktop_index;
}
gboolean get_window_state(Display *display, Window window, const char *state_atom_name) {
    (void)display;
    (void)window;
    (void)state_atom_name;
    return FALSE;
}
void set_window_state(Display *display, Window window, const char *state_atom_name,
                      WindowStateAction action) {
    (void)display;
    (void)window;
    (void)state_atom_name;
    (void)action;
}
int get_current_desktop(Display *display) { (void)display; return 0; }
void switch_to_desktop(Display *display, int desktop) { (void)display; (void)desktop; }
int get_frame_extents(Display *display, Window window, FrameExtents *extents) {
    (void)display; (void)window;
    if (extents) memset(extents, 0, sizeof(*extents));
    return 0;
}
void request_frame_extents(Display *display, Window window) { (void)display; (void)window; }
void xmove_resize_frame_aware(Display *display, Window window,
                               int frame_x, int frame_y, int width, int height) {
    (void)display; (void)window; (void)frame_x; (void)frame_y; (void)width; (void)height;
}
int matching_run_gc(AppData *app) { (void)app; return 0; }
int geom_rule_sync_for_pattern(AppData *app, const char *pattern) {
    (void)app;
    (void)pattern;
    return 1;
}

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_INT(desc, expected, actual) do { \
    if ((expected) != (actual)) { \
        printf("FAIL: %s - expected %d, got %d\n", (desc), (expected), (actual)); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } \
} while (0)

#define ASSERT_STR(desc, expected, actual) do { \
    if (strcmp((expected), (actual)) != 0) { \
        printf("FAIL: %s - expected '%s', got '%s'\n", (desc), (expected), (actual)); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } \
} while (0)

#define ASSERT_NULL(desc, actual) do { \
    if ((actual) != NULL) { \
        printf("FAIL: %s - expected NULL\n", (desc)); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } \
} while (0)

#define ASSERT_NOT_NULL(desc, actual) do { \
    if ((actual) == NULL) { \
        printf("FAIL: %s - expected non-NULL\n", (desc)); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } \
} while (0)

static WindowInfo make_window(Window id, const char *title, const char *class_name,
                               const char *instance, const char *type) {
    WindowInfo w = {0};
    w.id = id;
    safe_string_copy(w.title, title, MAX_TITLE_LEN);
    safe_string_copy(w.class_name, class_name, MAX_CLASS_LEN);
    safe_string_copy(w.instance, instance, MAX_CLASS_LEN);
    safe_string_copy(w.type, type, 16);
    w.desktop = 0;
    return w;
}

static void test_init(void) {
    printf("\n--- match_entry_manager_init ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);
    ASSERT_INT("count starts at 0", 0, mgr.count);

    // NULL is safe
    match_entry_manager_init(NULL);
    printf("PASS: NULL init does not crash\n");
    tests_passed++;
}

static void test_assign_and_get(void) {
    printf("\n--- match_entry_assign_custom_name / match_entry_get_custom_name ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w = make_window(100, "Firefox - Home", "Firefox", "Navigator", "Normal");
    match_entry_assign_custom_name(&mgr, &w, "browser");

    ASSERT_INT("count after assign", 1, mgr.count);
    ASSERT_NOT_NULL("custom name not NULL", match_entry_get_custom_name(&mgr, 100));
    ASSERT_STR("custom name value", "browser", match_entry_get_custom_name(&mgr, 100));

    // Update existing
    match_entry_assign_custom_name(&mgr, &w, "web");
    ASSERT_INT("count unchanged after update", 1, mgr.count);
    ASSERT_STR("updated name", "web", match_entry_get_custom_name(&mgr, 100));

    // Non-existent window
    ASSERT_NULL("unassigned window returns NULL", match_entry_get_custom_name(&mgr, 999));

    // NULL/empty edge cases
    match_entry_assign_custom_name(&mgr, &w, "");
    ASSERT_INT("empty name ignored", 1, mgr.count);
    ASSERT_STR("empty assign preserves previous name", "web", match_entry_get_custom_name(&mgr, 100));

    mgr.entries[0].custom_name[0] = '\0';
    ASSERT_NULL("empty stored custom name returns NULL", match_entry_get_custom_name(&mgr, 100));
    safe_string_copy(mgr.entries[0].custom_name, "restored", MAX_TITLE_LEN);
    ASSERT_STR("non-empty stored custom name returns string", "restored",
               match_entry_get_custom_name(&mgr, 100));

    match_entry_assign_custom_name(&mgr, NULL, "test");
    ASSERT_INT("NULL window ignored", 1, mgr.count);

    match_entry_assign_custom_name(NULL, &w, "test");
    // Should not crash
    printf("PASS: NULL manager does not crash\n");
    tests_passed++;

    // Zero window id
    ASSERT_NULL("id 0 returns NULL", match_entry_get_custom_name(&mgr, 0));
    ASSERT_NULL("NULL manager returns NULL", match_entry_get_custom_name(NULL, 100));
}

static void test_is_window_already_named(void) {
    printf("\n--- match_entry_is_bound_window ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w = make_window(200, "Terminal", "gnome-terminal", "gnome-terminal", "Normal");
    ASSERT_INT("not named initially", 0, match_entry_is_bound_window(&mgr, 200));

    match_entry_assign_custom_name(&mgr, &w, "term");
    ASSERT_INT("named after assign", 1, match_entry_is_bound_window(&mgr, 200));
    ASSERT_INT("different window not named", 0, match_entry_is_bound_window(&mgr, 201));
}

static void test_find_by_index(void) {
    printf("\n--- match_entry_find_index_by_window ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w1 = make_window(10, "A", "ClassA", "instA", "Normal");
    WindowInfo w2 = make_window(20, "B", "ClassB", "instB", "Normal");
    match_entry_assign_custom_name(&mgr, &w1, "first");
    match_entry_assign_custom_name(&mgr, &w2, "second");

    ASSERT_INT("find first", 0, match_entry_find_index_by_window(&mgr, 10));
    ASSERT_INT("find second", 1, match_entry_find_index_by_window(&mgr, 20));
    ASSERT_INT("find missing", -1, match_entry_find_index_by_window(&mgr, 30));
    ASSERT_INT("find id 0", -1, match_entry_find_index_by_window(&mgr, 0));
    ASSERT_INT("find NULL mgr", -1, match_entry_find_index_by_window(NULL, 10));
}

static void test_find_by_name(void) {
    printf("\n--- match_entry_find_index_by_custom_name ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w1 = make_window(10, "A", "ClassA", "instA", "Normal");
    WindowInfo w2 = make_window(20, "B", "ClassB", "instB", "Normal");
    match_entry_assign_custom_name(&mgr, &w1, "alpha");
    match_entry_assign_custom_name(&mgr, &w2, "beta");

    ASSERT_INT("find alpha", 0, match_entry_find_index_by_custom_name(&mgr, "alpha"));
    ASSERT_INT("find beta", 1, match_entry_find_index_by_custom_name(&mgr, "beta"));
    ASSERT_INT("find missing", -1, match_entry_find_index_by_custom_name(&mgr, "gamma"));
    ASSERT_INT("find NULL name", -1, match_entry_find_index_by_custom_name(&mgr, NULL));
    ASSERT_INT("find NULL mgr", -1, match_entry_find_index_by_custom_name(NULL, "alpha"));
}

static void test_delete(void) {
    printf("\n--- match_entry_delete_custom_name ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w1 = make_window(10, "A", "ClassA", "instA", "Normal");
    WindowInfo w2 = make_window(20, "B", "ClassB", "instB", "Normal");
    WindowInfo w3 = make_window(30, "C", "ClassC", "instC", "Normal");
    match_entry_assign_custom_name(&mgr, &w1, "first");
    match_entry_assign_custom_name(&mgr, &w2, "second");
    match_entry_assign_custom_name(&mgr, &w3, "third");

    ASSERT_INT("count is 3", 3, mgr.count);

    // Delete middle
    match_entry_delete_custom_name(&mgr, 1);
    ASSERT_INT("count after delete middle", 2, mgr.count);
    ASSERT_STR("first still there", "first", mgr.entries[0].custom_name);
    ASSERT_STR("third shifted to index 1", "third", mgr.entries[1].custom_name);

    // Delete first
    match_entry_delete_custom_name(&mgr, 0);
    ASSERT_INT("count after delete first", 1, mgr.count);
    ASSERT_STR("third now at index 0", "third", mgr.entries[0].custom_name);

    // Delete last
    match_entry_delete_custom_name(&mgr, 0);
    ASSERT_INT("count after delete last", 0, mgr.count);

    // Edge cases: invalid indices
    match_entry_delete_custom_name(&mgr, -1);
    match_entry_delete_custom_name(&mgr, 0);
    match_entry_delete_custom_name(&mgr, 100);
    match_entry_delete_custom_name(NULL, 0);
    printf("PASS: invalid delete indices do not crash\n");
    tests_passed++;
}

static void test_update_name(void) {
    printf("\n--- match_entry_update_custom_name ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w = make_window(10, "A", "ClassA", "instA", "Normal");
    match_entry_assign_custom_name(&mgr, &w, "original");

    match_entry_update_custom_name(&mgr, 0, "updated");
    ASSERT_STR("name updated", "updated", mgr.entries[0].custom_name);
    ASSERT_INT("count unchanged", 1, mgr.count);

    // Edge cases
    match_entry_update_custom_name(&mgr, -1, "bad");
    match_entry_update_custom_name(&mgr, 99, "bad");
    match_entry_update_custom_name(&mgr, 0, NULL);
    match_entry_update_custom_name(NULL, 0, "bad");
    printf("PASS: invalid update args do not crash\n");
    tests_passed++;
}

static void test_get_by_index(void) {
    printf("\n--- match_entry_get_by_index ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w = make_window(10, "A", "ClassA", "instA", "Normal");
    match_entry_assign_custom_name(&mgr, &w, "test");

    ASSERT_NOT_NULL("valid index returns entry", match_entry_get_by_index(&mgr, 0));
    ASSERT_NULL("out of bounds returns NULL", match_entry_get_by_index(&mgr, 1));
    ASSERT_NULL("negative index returns NULL", match_entry_get_by_index(&mgr, -1));
    ASSERT_NULL("NULL manager returns NULL", match_entry_get_by_index(NULL, 0));
}

static void test_reassign_names(void) {
    printf("\n--- match_entry_reassign_live_windows ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    // Assign a name to window 100
    WindowInfo w_orig = make_window(100, "Terminal - bash", "gnome-terminal", "gnome-terminal-server", "Normal");
    match_entry_assign_custom_name(&mgr, &w_orig, "term");
    ASSERT_INT("assigned to 100", 1, mgr.entries[0].assigned);

    // Now window 100 is gone, but window 200 matches the same class/instance/type
    WindowInfo windows[2];
    windows[0] = make_window(200, "Terminal - bash", "gnome-terminal", "gnome-terminal-server", "Normal");
    windows[1] = make_window(300, "Firefox", "Firefox", "Navigator", "Normal");

    int changed = match_entry_reassign_live_windows(&mgr, windows, 2);
    ASSERT_INT("reassignment happened", 1, changed);
    ASSERT_INT("reassigned to 200", 1, (mgr.entries[0].bound_x11_id == 200));
    ASSERT_INT("still assigned", 1, mgr.entries[0].assigned);
    ASSERT_STR("name preserved", "term", mgr.entries[0].custom_name);

    // If window still exists, no reassignment needed
    changed = match_entry_reassign_live_windows(&mgr, windows, 2);
    ASSERT_INT("no change when window exists", 0, changed);

    // NULL safety
    ASSERT_INT("NULL manager", 0, (int)match_entry_reassign_live_windows(NULL, windows, 2));
    ASSERT_INT("NULL windows", 0, (int)match_entry_reassign_live_windows(&mgr, NULL, 2));
}

static void test_reassign_no_match(void) {
    printf("\n--- match_entry_reassign_live_windows (orphaned) ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w_orig = make_window(100, "Terminal", "gnome-terminal", "gnome-terminal-server", "Normal");
    match_entry_assign_custom_name(&mgr, &w_orig, "term");

    // No matching windows at all
    WindowInfo windows[1];
    windows[0] = make_window(200, "Firefox", "Firefox", "Navigator", "Normal");
    match_entry_reassign_live_windows(&mgr, windows, 1);

    ASSERT_INT("orphaned (unassigned)", 0, mgr.entries[0].assigned);
    ASSERT_STR("name still there", "term", mgr.entries[0].custom_name);
}

static void test_reassign_skip_already_named(void) {
    printf("\n--- match_entry_reassign_live_windows (skip already named) ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    // Two windows with same class
    WindowInfo w1 = make_window(100, "Terminal - tab1", "gnome-terminal", "gnome-terminal-server", "Normal");
    WindowInfo w2 = make_window(200, "Terminal - tab2", "gnome-terminal", "gnome-terminal-server", "Normal");
    match_entry_assign_custom_name(&mgr, &w1, "tab1");
    match_entry_assign_custom_name(&mgr, &w2, "tab2");

    // Now only window 200 remains - window 100 is gone
    WindowInfo remaining[1];
    remaining[0] = make_window(200, "Terminal - tab2", "gnome-terminal", "gnome-terminal-server", "Normal");

    match_entry_reassign_live_windows(&mgr, remaining, 1);

    // tab2 keeps window 200, tab1 becomes orphaned (not stolen from tab2)
    ASSERT_INT("tab2 still assigned", 1, mgr.entries[1].assigned);
    ASSERT_INT("tab2 still on 200", 1, (mgr.entries[1].bound_x11_id == 200));
    ASSERT_INT("tab1 orphaned", 0, mgr.entries[0].assigned);
}

static void test_wildcard_in_title(void) {
    printf("\n--- wildcard matching in title storage ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    // Capture escapes '*' to '.' so literal asterisks do not become broad '*' matches.
    WindowInfo w = make_window(100, "test*file", "Class", "inst", "Normal");
    match_entry_assign_custom_name(&mgr, &w, "myfile");

    ASSERT_STR("asterisk escaped to dot on capture", "test.file", mgr.entries[0].original_title);
}

static void test_reassign_wildcard_characterization(void) {
    printf("\n--- match_entry_reassign_live_windows wildcard characterization ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w = make_window(100, "term-*", "ClassA", "instA", "Normal");
    match_entry_assign_custom_name(&mgr, &w, "term");
    ASSERT_STR("capture escapes wildcard star", "term-.", mgr.entries[0].original_title);

    mgr.entries[0].bound_x11_id = 999;
    mgr.entries[0].assigned = 1;
    WindowInfo exact_candidate = make_window(200, "term-1", "ClassA", "instA", "Normal");
    int exact_changed = match_entry_reassign_live_windows(&mgr, &exact_candidate, 1);
    ASSERT_INT("wildcard mode reports changed when stale binding is cleared", 1, exact_changed);
    ASSERT_INT("escaped dot matches single-char suffix", 1, mgr.entries[0].assigned);

    mgr.entries[0].bound_x11_id = 998;
    mgr.entries[0].assigned = 1;
    safe_string_copy(mgr.entries[0].original_title, "term-*", MAX_TITLE_LEN);
    WindowInfo glob_candidate = make_window(300, "term-xyz", "ClassA", "instA", "Normal");
    ASSERT_INT("raw wildcard title matches candidate", 1, (int)match_entry_reassign_live_windows(&mgr, &glob_candidate, 1));
    ASSERT_INT("wildcard match rebinds matched window", 1, (mgr.entries[0].bound_x11_id == 300));
}

static void test_match_if_set_class_instance_type(void) {
    printf("\n--- match_entry_matches_window uses optional class/instance/type anchors ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w = make_window(100, "term-1", "ClassA", "instA", "Normal");
    match_entry_assign_custom_name(&mgr, &w, "term");
    ASSERT_STR("create captures class", "ClassA", mgr.entries[0].class_name);
    ASSERT_STR("create captures instance", "instA", mgr.entries[0].instance);
    ASSERT_STR("create captures type", "Normal", mgr.entries[0].type);

    WindowInfo same = make_window(200, "term-1", "ClassA", "instA", "Normal");
    ASSERT_INT("same title/class/instance/type matches", 1,
               (int)match_entry_matches_window(&mgr.entries[0], &same));

    WindowInfo wrong_class = make_window(201, "term-1", "ClassB", "instA", "Normal");
    ASSERT_INT("wrong class rejected when class anchor set", 0,
               (int)match_entry_matches_window(&mgr.entries[0], &wrong_class));

    WindowInfo wrong_instance = make_window(202, "term-1", "ClassA", "instB", "Normal");
    ASSERT_INT("wrong instance rejected when instance anchor set", 0,
               (int)match_entry_matches_window(&mgr.entries[0], &wrong_instance));

    WindowInfo wrong_type = make_window(203, "term-1", "ClassA", "instA", "Special");
    ASSERT_INT("wrong type rejected when type anchor set", 0,
               (int)match_entry_matches_window(&mgr.entries[0], &wrong_type));

    mgr.entries[0].class_name[0] = '\0';
    mgr.entries[0].instance[0] = '\0';
    mgr.entries[0].type[0] = '\0';
    ASSERT_INT("empty anchors become unconstrained", 1,
               (int)match_entry_matches_window(&mgr.entries[0], &wrong_class));
}

static void test_same_title_different_class_create_distinct_entries(void) {
    printf("\n--- same title but different class creates distinct entries ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo browser = make_window(100, "Dashboard", "Chromium", "chromium", "Normal");
    WindowInfo terminal = make_window(101, "Dashboard", "mate-terminal", "mate-terminal", "Normal");

    int a = matching_create_entry(&mgr, &browser);
    int b = matching_create_entry(&mgr, &terminal);

    ASSERT_INT("first entry created", 1, a > 0);
    ASSERT_INT("second entry created", 1, b > 0);
    ASSERT_INT("two entries exist", 2, mgr.count);
    ASSERT_INT("entries keep distinct classes", 1,
               strcmp(mgr.entries[0].class_name, mgr.entries[1].class_name) != 0);
}

static void set_test_home(const char *suffix) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/cofi-named-window-%s-%ld", suffix, (long)getpid());
    mkdir(path, 0755);
    setenv("HOME", path, 1);
}

static void test_matching_create_entry_always_creates(void) {
    printf("\n--- matching_create_entry always creates ---\n");

    MatchEntryManager manager;
    WindowInfo windows[MAX_WINDOWS] = {0};
    match_entry_manager_init(&manager);
    WindowInfo w = make_window(111, "Editor", "Code", "code", "Normal");
    windows[0] = w;

    int id1 = matching_create_entry(&manager, &w);
    int id2 = matching_create_entry(&manager, &w);

    ASSERT_INT("first capture succeeds", 1, id1 > 0);
    ASSERT_INT("second create returns new match id", 1, id2 > id1);
    ASSERT_INT("create stores two entries", 2, manager.count);
    ASSERT_STR("create captured class", "Code", manager.entries[0].class_name);
    ASSERT_STR("create captured instance", "code", manager.entries[0].instance);
    ASSERT_STR("create captured type", "Normal", manager.entries[0].type);
}

static void test_matching_create_entry_round_trips_to_source_window(void) {
    printf("\n--- matching_create_entry round-trips to source window ---\n");

    MatchEntryManager manager;
    match_entry_manager_init(&manager);

    WindowInfo plain = make_window(1001, "foo", "Bar", "bar", "Normal");
    int plain_id = matching_create_entry(&manager, &plain);
    ASSERT_INT("plain entry created", 1, plain_id > 0);
    ASSERT_INT("plain entry matches source window", 1,
               (int)match_entry_matches_window(&manager.entries[0], &plain));

    WindowInfo escaped = make_window(1002, "name*with.meta?chars", "EscClass", "esc", "Normal");
    int escaped_id = matching_create_entry(&manager, &escaped);
    ASSERT_INT("escaped entry created", 1, escaped_id > plain_id);
    ASSERT_INT("escaped entry matches source window", 1,
               (int)match_entry_matches_window(&manager.entries[1], &escaped));
}

static void test_match_id_persist_and_non_reuse(void) {
    printf("\n--- match_id persistence and non-reuse ---\n");

    set_test_home("ids");
    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w1 = make_window(10, "A", "ClassA", "instA", "Normal");
    WindowInfo w2 = make_window(20, "B", "ClassB", "instB", "Normal");
    match_entry_assign_custom_name(&mgr, &w1, "first");
    match_entry_assign_custom_name(&mgr, &w2, "second");
    int first_id = mgr.entries[0].match_id;
    int second_id = mgr.entries[1].match_id;

    match_entry_delete_custom_name(&mgr, 0);
    WindowInfo w3 = make_window(30, "C", "ClassC", "instC", "Normal");
    match_entry_assign_custom_name(&mgr, &w3, "third");
    int third_id = mgr.entries[1].match_id;

    ASSERT_INT("second id remains stable", second_id, mgr.entries[0].match_id);
    ASSERT_INT("new id is not reused", 1, third_id > second_id && third_id != first_id);

    save_match_entries(&mgr);
    MatchEntryManager loaded;
    load_match_entries(&loaded);
    ASSERT_INT("loaded count preserved", 2, loaded.count);
    ASSERT_INT("loaded id[0] stable", second_id, loaded.entries[0].match_id);
    ASSERT_INT("loaded id[1] stable", third_id, loaded.entries[1].match_id);
    ASSERT_INT("next_match_id advanced", 1, loaded.next_match_id > third_id);
}

static void test_load_repairs_malformed_or_duplicate_match_ids(void) {
    printf("\n--- load repairs malformed/duplicate match_id ---\n");

    set_test_home("repair");
    const char *home = getenv("HOME");
    char config_root[512];
    char config_dir[512];
    char config_path[512];
    snprintf(config_root, sizeof(config_root), "%s/.config", home);
    snprintf(config_dir, sizeof(config_dir), "%s/.config/cofi", home);
    snprintf(config_path, sizeof(config_path), "%s/.config/cofi/matching.json", home);
    mkdir(config_root, 0755);
    mkdir(config_dir, 0755);

    FILE *f = fopen(config_path, "w");
    ASSERT_NOT_NULL("test file opened", f);
    if (!f) return;
    fprintf(f,
            "{\n"
            "  \"next_match_id\": 1,\n"
            "  \"match_entries\": [\n"
            "    {\n"
            "      \"match_id\": 1,\n"
            "      \"bound_x11_id\": 100,\n"
            "      \"custom_name\": \"a\",\n"
            "      \"original_title\": \"A\",\n"
            "      \"class_name\": \"C\",\n"
            "      \"instance\": \"I\",\n"
            "      \"type\": \"Normal\",\n"
            "      \"match_mode\": \"EXACT\",\n"
            "      \"assigned\": 1\n"
            "    },\n"
            "    {\n"
            "      \"match_id\": 1,\n"
            "      \"bound_x11_id\": 200,\n"
            "      \"custom_name\": \"b\",\n"
            "      \"original_title\": \"B\",\n"
            "      \"class_name\": \"C\",\n"
            "      \"instance\": \"I\",\n"
            "      \"type\": \"Normal\",\n"
            "      \"match_mode\": \"EXACT\",\n"
            "      \"assigned\": 1\n"
            "    },\n"
            "    {\n"
            "      \"match_id\": 0,\n"
            "      \"bound_x11_id\": 300,\n"
            "      \"custom_name\": \"c\",\n"
            "      \"original_title\": \"C\",\n"
            "      \"class_name\": \"C\",\n"
            "      \"instance\": \"I\",\n"
            "      \"type\": \"Normal\",\n"
            "      \"match_mode\": \"EXACT\",\n"
            "      \"assigned\": 1\n"
            "    }\n"
            "  ]\n"
            "}\n");
    fclose(f);

    MatchEntryManager loaded;
    load_match_entries(&loaded);
    ASSERT_INT("loaded malformed/duplicate entries", 3, loaded.count);
    ASSERT_INT("id[0] positive", 1, loaded.entries[0].match_id > 0);
    ASSERT_INT("id[1] positive", 1, loaded.entries[1].match_id > 0);
    ASSERT_INT("id[2] positive", 1, loaded.entries[2].match_id > 0);
    ASSERT_INT("id[0] != id[1]", 1, loaded.entries[0].match_id != loaded.entries[1].match_id);
    ASSERT_INT("id[1] != id[2]", 1, loaded.entries[1].match_id != loaded.entries[2].match_id);
    ASSERT_INT("next_match_id monotonic", 1, loaded.next_match_id > loaded.entries[2].match_id);
}

static void test_load_missing_required_fields_and_missing_match_id(void) {
    printf("\n--- load handles missing required fields and missing match_id ---\n");

    set_test_home("missing-required");
    const char *home = getenv("HOME");
    char config_root[512];
    char config_dir[512];
    char config_path[512];
    snprintf(config_root, sizeof(config_root), "%s/.config", home);
    snprintf(config_dir, sizeof(config_dir), "%s/.config/cofi", home);
    snprintf(config_path, sizeof(config_path), "%s/.config/cofi/matching.json", home);
    mkdir(config_root, 0755);
    mkdir(config_dir, 0755);

    FILE *f = fopen(config_path, "w");
    ASSERT_NOT_NULL("missing-required fixture opened", f);
    if (!f) return;
    fprintf(f,
            "{\n"
            "  \"next_match_id\": 5,\n"
            "  \"match_entries\": [\n"
            "    {\n"
            "      \"custom_name\": \"missing-fields\",\n"
            "      \"original_title\": \"Only title\",\n"
            "      \"assigned\": 1\n"
            "    },\n"
            "    {\n"
            "      \"match_id\": 7,\n"
            "      \"bound_x11_id\": 333,\n"
            "      \"custom_name\": \"has-id\",\n"
            "      \"original_title\": \"Has id\",\n"
            "      \"class_name\": \"ClassA\",\n"
            "      \"instance\": \"instA\",\n"
            "      \"type\": \"Normal\",\n"
            "      \"match_mode\": \"GLOB\",\n"
            "      \"assigned\": 1\n"
            "    }\n"
            "  ]\n"
            "}\n");
    fclose(f);

    MatchEntryManager loaded;
    load_match_entries(&loaded);
    ASSERT_INT("two entries loaded even with missing fields", 2, loaded.count);
    ASSERT_INT("missing match_id repaired to positive", 1, loaded.entries[0].match_id > 0);
    ASSERT_INT("second entry id preserved", 7, loaded.entries[1].match_id);
    ASSERT_INT("next_match_id remains monotonic", 1, loaded.next_match_id > 7);
}

static void test_load_realistic_legacy_matching_json_shape(void) {
    printf("\n--- load realistic legacy matching.json shape ---\n");

    set_test_home("legacy-shape");
    const char *home = getenv("HOME");
    char config_root[512];
    char config_dir[512];
    char config_path[512];
    snprintf(config_root, sizeof(config_root), "%s/.config", home);
    snprintf(config_dir, sizeof(config_dir), "%s/.config/cofi", home);
    snprintf(config_path, sizeof(config_path), "%s/.config/cofi/matching.json", home);
    mkdir(config_root, 0755);
    mkdir(config_dir, 0755);

    FILE *f = fopen(config_path, "w");
    ASSERT_NOT_NULL("legacy-shape fixture opened", f);
    if (!f) return;
    fprintf(f,
            "{\n"
            "  \"next_match_id\": 42,\n"
            "  \"match_entries\": [\n"
            "    {\n"
            "      \"match_id\": 7,\n"
            "      \"bound_x11_id\": 12345,\n"
            "      \"custom_name\": \"editor-main\",\n"
            "      \"original_title\": \"Code - main.c\",\n"
            "      \"class_name\": \"Code\",\n"
            "      \"instance\": \"code\",\n"
            "      \"type\": \"Normal\",\n"
            "      \"match_mode\": \"EXACT\",\n"
            "      \"assigned\": 1\n"
            "    },\n"
            "    {\n"
            "      \"match_id\": 8,\n"
            "      \"bound_x11_id\": 67890,\n"
            "      \"custom_name\": \"term-*\",\n"
            "      \"original_title\": \"cofi*Terminal\",\n"
            "      \"class_name\": \"Alacritty\",\n"
            "      \"instance\": \"alacritty\",\n"
            "      \"type\": \"Normal\",\n"
            "      \"match_mode\": \"GLOB\",\n"
            "      \"assigned\": 0\n"
            "    }\n"
            "  ]\n"
            "}\n");
    fclose(f);

    MatchEntryManager loaded;
    load_match_entries(&loaded);
    ASSERT_INT("legacy-shape loaded two entries", 2, loaded.count);
    ASSERT_INT("legacy-shape preserves first id", 7, loaded.entries[0].match_id);
    ASSERT_INT("legacy-shape preserves second id", 8, loaded.entries[1].match_id);
    ASSERT_STR("legacy-shape keeps title", "Code - main.c", loaded.entries[0].original_title);

    save_match_entries(&loaded);
    char saved[8192];
    memset(saved, 0, sizeof(saved));
    FILE *saved_file = fopen(config_path, "r");
    ASSERT_NOT_NULL("legacy-shape saved file opened", saved_file);
    if (!saved_file) return;
    size_t n = fread(saved, 1, sizeof(saved) - 1, saved_file);
    fclose(saved_file);
    ASSERT_INT("legacy-shape saved bytes read", 1, n > 0);
    ASSERT_INT("legacy-shape preserves class_name on save", 1, strstr(saved, "\"class_name\"") != NULL);
    ASSERT_INT("legacy-shape preserves instance on save", 1, strstr(saved, "\"instance\"") != NULL);
    ASSERT_INT("legacy-shape preserves type on save", 1, strstr(saved, "\"type\"") != NULL);
}

static void test_save_load_roundtrip_special_chars(void) {
    printf("\n--- save/load roundtrip preserves special chars ---\n");

    set_test_home("special-chars");
    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w = make_window(501, "Title", "Class", "inst", "Normal");
    match_entry_assign_custom_name(&mgr, &w, "name + extras !@#$%^&*()");
    safe_string_copy(mgr.entries[0].original_title, "Orig [brackets] / path", MAX_TITLE_LEN);
    safe_string_copy(mgr.entries[0].class_name, "", sizeof(mgr.entries[0].class_name));
    safe_string_copy(mgr.entries[0].instance, "inst-special", sizeof(mgr.entries[0].instance));
    safe_string_copy(mgr.entries[0].type, "Normal", sizeof(mgr.entries[0].type));
    mgr.entries[0].assigned = 1;

    save_match_entries(&mgr);

    MatchEntryManager loaded;
    load_match_entries(&loaded);
    ASSERT_INT("roundtrip loads one entry", 1, loaded.count);
    ASSERT_STR("custom_name roundtrip", "name + extras !@#$%^&*()", loaded.entries[0].custom_name);
    ASSERT_STR("original_title roundtrip", "Orig [brackets] / path", loaded.entries[0].original_title);
    ASSERT_STR("class_name roundtrip empty", "", loaded.entries[0].class_name);
    ASSERT_STR("instance roundtrip populated", "inst-special", loaded.entries[0].instance);
    ASSERT_STR("type roundtrip populated", "Normal", loaded.entries[0].type);
}

static void test_bound_x11_id_validation_and_rebind(void) {
    printf("\n--- bound_x11_id ignored on load and rebound by title ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);
    WindowInfo captured = make_window(200, "Title-A", "ClassA", "instA", "Normal");
    match_entry_assign_custom_name(&mgr, &captured, "name");

    // Same id + drifted title is no longer trusted; rebind is title/mode-based.
    WindowInfo drifted = make_window(200, "Title-B", "ClassA", "instA", "Normal");
    int changed = match_entry_reassign_live_windows(&mgr, &drifted, 1);
    ASSERT_INT("drifted title clears stale binding", 1, changed);
    ASSERT_INT("binding cleared on drifted title", 0, (int)mgr.entries[0].bound_x11_id);
    ASSERT_INT("entry marked unassigned", 0, mgr.entries[0].assigned);

    // Reused id with wrong class should be dropped, then rebound by criteria.
    safe_string_copy(mgr.entries[0].original_title, "Wanted", MAX_TITLE_LEN);
    WindowInfo windows[2];
    windows[0] = make_window(200, "Wanted", "OtherClass", "instA", "Normal");
    windows[1] = make_window(300, "Wanted", "ClassA", "instA", "Normal");
    changed = match_entry_reassign_live_windows(&mgr, windows, 2);
    ASSERT_INT("class mismatch on reused id forces fallback rebind", 1, changed);
    ASSERT_INT("rebound to title+class matching window", 300, (int)mgr.entries[0].bound_x11_id);
}

static void test_startup_load_then_reassign_path(void) {
    printf("\n--- startup load->reassign path ---\n");

    set_test_home("startup");
    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);
    WindowInfo w = make_window(100, "Wanted", "ClassA", "instA", "Normal");
    match_entry_assign_custom_name(&mgr, &w, "name");
    save_match_entries(&mgr);

    MatchEntryManager loaded;
    load_match_entries(&loaded);
    ASSERT_INT("loaded one entry", 1, loaded.count);
    ASSERT_INT("loaded starts assigned", 1, loaded.entries[0].assigned);

    WindowInfo windows[1];
    windows[0] = make_window(300, "Wanted", "ClassA", "instA", "Normal");
    int changed = match_entry_reassign_live_windows(&loaded, windows, 1);
    ASSERT_INT("startup reassign changed binding", 1, changed);
    ASSERT_INT("startup rebound to live window", 300, (int)loaded.entries[0].bound_x11_id);
}

static void test_escaped_star_pattern_persists_as_single_char_wildcard(void) {
    printf("\n--- escaped star pattern persists as single-char wildcard ---\n");

    set_test_home("glob-persist");
    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w = make_window(100, "cofi*Terminal", "ClassA", "instA", "Normal");
    match_entry_assign_custom_name(&mgr, &w, "term");
    save_match_entries(&mgr);

    MatchEntryManager loaded;
    load_match_entries(&loaded);
    ASSERT_INT("loaded one entry", 1, loaded.count);
    ASSERT_STR("pattern persisted with escaped star", "cofi.Terminal", loaded.entries[0].original_title);

    loaded.entries[0].bound_x11_id = 999;
    loaded.entries[0].assigned = 1;
    WindowInfo changed_title = make_window(200, "cofi | main - Terminal", "ClassA", "instA", "Normal");
    int changed = match_entry_reassign_live_windows(&loaded, &changed_title, 1);
    ASSERT_INT("single-char wildcard does not match long drifted title", 0, loaded.entries[0].assigned);

    loaded.entries[0].bound_x11_id = 998;
    loaded.entries[0].assigned = 1;
    safe_string_copy(loaded.entries[0].original_title, "cofi.Terminal", MAX_TITLE_LEN);
    changed = match_entry_reassign_live_windows(&loaded, &changed_title, 1);
    ASSERT_INT("single-char wildcard pattern does not match changed title", 0, loaded.entries[0].assigned);
}

static void test_match_entry_collect_labeled_ids(void) {
    printf("\n--- match_entry_collect_labeled_ids returns only labeled entries ---\n");

    MatchEntryManager mgr;
    WindowInfo windows[MAX_WINDOWS] = {0};
    match_entry_manager_init(&mgr);

    WindowInfo unlabeled = make_window(100, "Unlabeled", "ClassA", "instA", "Normal");
    WindowInfo labeled_a = make_window(200, "Labeled A", "ClassB", "instB", "Normal");
    WindowInfo labeled_b = make_window(300, "Labeled B", "ClassC", "instC", "Normal");
    windows[0] = unlabeled;
    windows[1] = labeled_a;
    windows[2] = labeled_b;

    matching_create_entry(&mgr, &unlabeled);
    match_entry_assign_custom_name(&mgr, &labeled_a, "keep-a");
    match_entry_assign_custom_name(&mgr, &labeled_b, "keep-b");

    int referenced_ids[MAX_WINDOWS] = {0};
    int count = match_entry_collect_labeled_ids(&mgr, referenced_ids, MAX_WINDOWS);

    ASSERT_INT("two labeled ids collected", 2, count);
    ASSERT_INT("first labeled id collected", mgr.entries[1].match_id, referenced_ids[0]);
    ASSERT_INT("second labeled id collected", mgr.entries[2].match_id, referenced_ids[1]);
}

static void test_layout_save_persists_on_deduped_match_id(void) {
    printf("\n--- layout save persists on deduped match id ---\n");

    set_test_home("layout-persist");

    AppData app;
    memset(&app, 0, sizeof(app));
    match_entry_manager_init(&app.matching);
    layout_store_init(&app.layouts);
    app.display = (Display *)0x1;

    app.windows[0] = make_window(0x401, "Geom Window", "ClassGeom", "instGeom", "Normal");
    app.window_count = 1;

    ASSERT_INT("first layout save succeeds", TRUE,
               save_window_geometry_for_window(&app, &app.windows[0]));
    ASSERT_INT("second layout save succeeds", TRUE,
               save_window_geometry_for_window(&app, &app.windows[0]));
    ASSERT_INT("deduped capture keeps one match entry", 1, app.matching.count);
    ASSERT_INT("deduped layout save keeps one layout", 1, app.layouts.count);

    MatchEntryManager loaded_matching;
    LayoutStore loaded_layouts;
    load_match_entries(&loaded_matching);
    layout_store_init(&loaded_layouts);
    layout_store_load(&loaded_layouts);

    ASSERT_INT("reloaded one match entry", 1, loaded_matching.count);
    ASSERT_INT("reloaded one layout", 1, loaded_layouts.count);
    ASSERT_INT("layout stored against deduped match_id",
               loaded_matching.entries[0].match_id, loaded_layouts.records[0].match_id);
    ASSERT_INT("layout x persisted", g_geom_x, loaded_layouts.records[0].x);
    ASSERT_INT("layout y persisted", g_geom_y, loaded_layouts.records[0].y);
    ASSERT_INT("layout width persisted", g_geom_w, loaded_layouts.records[0].width);
    ASSERT_INT("layout height persisted", g_geom_h, loaded_layouts.records[0].height);
    ASSERT_INT("layout desktop persisted", g_geom_desktop, loaded_layouts.records[0].desktop);
    ASSERT_INT("layout max vert persisted", 0, loaded_layouts.records[0].maximized_vert);
    ASSERT_INT("layout max horz persisted", 0, loaded_layouts.records[0].maximized_horz);
    ASSERT_INT("layout fullscreen persisted", 0, loaded_layouts.records[0].fullscreen);
}

static void test_layout_restore_resolve_requires_live_binding_and_saved_layout(void) {
    printf("\n--- layout restore resolve ---\n");

    MatchEntryManager mgr;
    LayoutStore store;
    match_entry_manager_init(&mgr);
    layout_store_init(&store);

    WindowInfo w = make_window(0x444, "Restore Me", "ClassR", "instR", "Normal");
    match_entry_assign_custom_name(&mgr, &w, "restore");
    ASSERT_INT("layout record stored", TRUE,
               layout_store_set(&store, mgr.entries[0].match_id, 70, 80, 900, 700, 2,
                                true, false, true, false, true));

    WindowGeometryRestoreTarget target = {0};
    ASSERT_INT("resolve succeeds for live bound layout entry", TRUE,
               resolve_window_geometry_restore_target(&mgr, &store, mgr.entries[0].match_id, &target));
    ASSERT_INT("resolve returns bound window", (int)w.id, (int)target.window);
    ASSERT_INT("resolve returns x", 70, target.x);
    ASSERT_INT("resolve returns y", 80, target.y);
    ASSERT_INT("resolve returns width", 900, target.width);
    ASSERT_INT("resolve returns height", 700, target.height);
    ASSERT_INT("resolve returns desktop", 2, target.desktop);
    ASSERT_INT("resolve returns max vert", 1, target.maximized_vert);
    ASSERT_INT("resolve returns max horz", 0, target.maximized_horz);
    ASSERT_INT("resolve returns fullscreen", 1, target.fullscreen);
    ASSERT_INT("resolve returns desktop lock", 0, target.restore_desktop);
    ASSERT_INT("resolve returns disabled", 1, target.disabled);

    mgr.entries[0].assigned = 0;
    ASSERT_INT("resolve fails when entry is not live-bound", FALSE,
               resolve_window_geometry_restore_target(&mgr, &store, mgr.entries[0].match_id, &target));

    mgr.entries[0].assigned = 1;
    ASSERT_INT("layout cleared", TRUE, layout_store_clear(&store, mgr.entries[0].match_id));
    ASSERT_INT("resolve fails without saved layout", FALSE,
               resolve_window_geometry_restore_target(&mgr, &store, mgr.entries[0].match_id, &target));
}

static void test_match_entry_gc_is_pure_reference_check(void) {
    printf("\n--- match_entry_gc is pure reference check ---\n");

    MatchEntryManager mgr;
    WindowInfo windows[MAX_WINDOWS] = {0};
    match_entry_manager_init(&mgr);

    WindowInfo unlabeled = make_window(100, "Unlabeled", "ClassA", "instA", "Normal");
    WindowInfo labeled = make_window(200, "Labeled", "ClassB", "instB", "Normal");
    WindowInfo referenced = make_window(300, "Referenced", "ClassC", "instC", "Normal");
    windows[0] = unlabeled;
    windows[1] = labeled;
    windows[2] = referenced;

    int unlabeled_id = matching_create_entry(&mgr, &unlabeled);
    match_entry_assign_custom_name(&mgr, &labeled, "labeled");
    int referenced_id = matching_create_entry(&mgr, &referenced);

    ASSERT_INT("three entries captured before gc", 3, mgr.count);
    ASSERT_INT("gc removes every unreferenced entry", 3,
               match_entry_gc(&mgr, NULL, 0));
    ASSERT_INT("count after empty-reference gc", 0, mgr.count);
    ASSERT_INT("unlabeled entry removed", -1, match_entry_find_index_by_match_id(&mgr, unlabeled_id));
    ASSERT_INT("labeled entry removed", -1, match_entry_find_index_by_custom_name(&mgr, "labeled"));
    ASSERT_INT("unreferenced captured entry removed", -1, match_entry_find_index_by_match_id(&mgr, referenced_id));
}

static void test_match_entry_gc_removes_only_unreferenced_ids(void) {
    printf("\n--- match_entry_gc removes only unreferenced ids ---\n");

    MatchEntryManager mgr;
    WindowInfo windows[MAX_WINDOWS] = {0};
    match_entry_manager_init(&mgr);

    WindowInfo unlabeled = make_window(100, "Unlabeled", "ClassA", "instA", "Normal");
    WindowInfo labeled = make_window(200, "Labeled", "ClassB", "instB", "Normal");
    WindowInfo referenced = make_window(300, "Referenced", "ClassC", "instC", "Normal");
    windows[0] = unlabeled;
    windows[1] = labeled;
    windows[2] = referenced;

    int unlabeled_id = matching_create_entry(&mgr, &unlabeled);
    match_entry_assign_custom_name(&mgr, &labeled, "keep");
    int referenced_id = matching_create_entry(&mgr, &referenced);
    int referenced_ids[] = {mgr.entries[1].match_id, referenced_id};

    ASSERT_INT("three entries captured before gc", 3, mgr.count);
    ASSERT_INT("gc removes only unlabeled unreferenced entry", 1,
               match_entry_gc(&mgr, referenced_ids, 2));
    ASSERT_INT("count after gc", 2, mgr.count);
    ASSERT_INT("unlabeled entry removed", -1, match_entry_find_index_by_match_id(&mgr, unlabeled_id));
    ASSERT_INT("labeled entry kept", 1, match_entry_find_index_by_custom_name(&mgr, "keep") >= 0);
    ASSERT_INT("referenced entry kept", 1, match_entry_find_index_by_match_id(&mgr, referenced_id) >= 0);
}

int main(void) {
    printf("Named Window Manager Tests\n");
    printf("==========================\n");

    test_init();
    test_assign_and_get();
    test_is_window_already_named();
    test_find_by_index();
    test_find_by_name();
    test_delete();
    test_update_name();
    test_get_by_index();
    test_reassign_names();
    test_reassign_no_match();
    test_reassign_skip_already_named();
    test_wildcard_in_title();
    test_reassign_wildcard_characterization();
    test_match_if_set_class_instance_type();
    test_same_title_different_class_create_distinct_entries();
    test_matching_create_entry_always_creates();
    test_matching_create_entry_round_trips_to_source_window();
    test_match_id_persist_and_non_reuse();
    test_load_repairs_malformed_or_duplicate_match_ids();
    test_load_missing_required_fields_and_missing_match_id();
    test_load_realistic_legacy_matching_json_shape();
    test_save_load_roundtrip_special_chars();
    test_bound_x11_id_validation_and_rebind();
    test_startup_load_then_reassign_path();
    test_escaped_star_pattern_persists_as_single_char_wildcard();
    test_match_entry_collect_labeled_ids();
    test_layout_save_persists_on_deduped_match_id();
    test_layout_restore_resolve_requires_live_binding_and_saved_layout();
    test_match_entry_gc_is_pure_reference_check();
    test_match_entry_gc_removes_only_unreferenced_ids();

    printf("\n=====================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
