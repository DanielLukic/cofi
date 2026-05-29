#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "x11/frame_extents.h"
#include "geom/layout_store.h"
#include "matching/match_entry.h"
#include "matching/match_entry_config.h"
#include "geom/window_geometry_matching.h"
#include "matching/window_matcher.h"
#include "x11/x11_utils.h"
#include "core/utils/utils.h"

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
gboolean window_is_fullscreen(Display *display, Window window) {
    (void)display;
    (void)window;
    return FALSE;
}
gboolean window_is_maximized_horizontal(Display *display, Window window) {
    (void)display;
    (void)window;
    return FALSE;
}
gboolean window_is_maximized_vertical(Display *display, Window window) {
    (void)display;
    (void)window;
    return FALSE;
}
void set_window_fullscreen(Display *display, Window window, WindowStateAction action) {
    (void)display;
    (void)window;
    (void)action;
}
void set_window_maximized_horizontal(Display *display, Window window, WindowStateAction action) {
    (void)display;
    (void)window;
    (void)action;
}
void set_window_maximized_vertical(Display *display, Window window, WindowStateAction action) {
    (void)display;
    (void)window;
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
int geom_rule_sync_for_layout(AppData *app, int match_id) {
    (void)app;
    (void)match_id;
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

static void test_create_entry_captures_identity(void) {
    printf("\n--- matching_create_entry captures identity ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w = make_window(100, "Firefox - Home", "Firefox", "Navigator", "Normal");
    int match_id = matching_create_entry(&mgr, &w);

    ASSERT_INT("match id positive", 1, match_id > 0);
    ASSERT_INT("count after create", 1, mgr.count);
    ASSERT_STR("title captured", "Firefox - Home", mgr.entries[0].original_title);
    ASSERT_STR("class captured", "Firefox", mgr.entries[0].class_name);
    ASSERT_STR("instance captured", "Navigator", mgr.entries[0].instance);
    ASSERT_STR("type captured", "Normal", mgr.entries[0].type);
    ASSERT_INT("bound x11 id captured", 100, (int)mgr.entries[0].bound_x11_id);
    ASSERT_INT("assigned", 1, mgr.entries[0].assigned);
    ASSERT_INT("NULL create rejected", -1, matching_create_entry(NULL, &w));
    ASSERT_INT("NULL window rejected", -1, matching_create_entry(&mgr, NULL));
}

static void test_is_window_already_bound(void) {
    printf("\n--- match_entry_is_bound_window ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w = make_window(200, "Terminal", "gnome-terminal", "gnome-terminal", "Normal");
    ASSERT_INT("not bound initially", 0, match_entry_is_bound_window(&mgr, 200));

    matching_create_entry(&mgr, &w);
    ASSERT_INT("bound after create", 1, match_entry_is_bound_window(&mgr, 200));
    ASSERT_INT("different window not bound", 0, match_entry_is_bound_window(&mgr, 201));
}

static void test_find_by_index(void) {
    printf("\n--- match_entry_find_index_by_window ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w1 = make_window(10, "A", "ClassA", "instA", "Normal");
    WindowInfo w2 = make_window(20, "B", "ClassB", "instB", "Normal");
    matching_create_entry(&mgr, &w1);
    matching_create_entry(&mgr, &w2);

    ASSERT_INT("find first", 0, match_entry_find_index_by_window(&mgr, 10));
    ASSERT_INT("find second", 1, match_entry_find_index_by_window(&mgr, 20));
    ASSERT_INT("find missing", -1, match_entry_find_index_by_window(&mgr, 30));
    ASSERT_INT("find id 0", -1, match_entry_find_index_by_window(&mgr, 0));
    ASSERT_INT("find NULL mgr", -1, match_entry_find_index_by_window(NULL, 10));
}

static void test_delete(void) {
    printf("\n--- match_entry_delete ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w1 = make_window(10, "A", "ClassA", "instA", "Normal");
    WindowInfo w2 = make_window(20, "B", "ClassB", "instB", "Normal");
    WindowInfo w3 = make_window(30, "C", "ClassC", "instC", "Normal");
    matching_create_entry(&mgr, &w1);
    matching_create_entry(&mgr, &w2);
    matching_create_entry(&mgr, &w3);

    ASSERT_INT("count is 3", 3, mgr.count);

    // Delete middle
    match_entry_delete(&mgr, 1);
    ASSERT_INT("count after delete middle", 2, mgr.count);
    ASSERT_STR("first still there", "A", mgr.entries[0].original_title);
    ASSERT_STR("third shifted to index 1", "C", mgr.entries[1].original_title);

    // Delete first
    match_entry_delete(&mgr, 0);
    ASSERT_INT("count after delete first", 1, mgr.count);
    ASSERT_STR("third now at index 0", "C", mgr.entries[0].original_title);

    // Delete last
    match_entry_delete(&mgr, 0);
    ASSERT_INT("count after delete last", 0, mgr.count);

    // Edge cases: invalid indices
    match_entry_delete(&mgr, -1);
    match_entry_delete(&mgr, 0);
    match_entry_delete(&mgr, 100);
    match_entry_delete(NULL, 0);
    printf("PASS: invalid delete indices do not crash\n");
    tests_passed++;
}

static void test_delete_by_match_id(void) {
    printf("\n--- match_entry_delete_by_match_id ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w1 = make_window(10, "A", "ClassA", "instA", "Normal");
    WindowInfo w2 = make_window(20, "B", "ClassB", "instB", "Normal");
    WindowInfo w3 = make_window(30, "C", "ClassC", "instC", "Normal");
    int first_id = matching_create_entry(&mgr, &w1);
    int second_id = matching_create_entry(&mgr, &w2);
    int third_id = matching_create_entry(&mgr, &w3);

    ASSERT_INT("count is 3", 3, mgr.count);
    match_entry_delete_by_match_id(&mgr, second_id);
    ASSERT_INT("count drops after match-id delete", 2, mgr.count);
    ASSERT_INT("deleted id is gone", -1, match_entry_find_index_by_match_id(&mgr, second_id));
    ASSERT_INT("first id remains", 1, match_entry_find_index_by_match_id(&mgr, first_id) >= 0);
    ASSERT_INT("third id remains", 1, match_entry_find_index_by_match_id(&mgr, third_id) >= 0);
    ASSERT_STR("third shifted but id retained", "C", mgr.entries[1].original_title);

    match_entry_delete_by_match_id(&mgr, 999);
    match_entry_delete_by_match_id(&mgr, -1);
    match_entry_delete_by_match_id(NULL, first_id);
    ASSERT_INT("missing match-id delete is idempotent", 2, mgr.count);
}

static void test_get_by_index(void) {
    printf("\n--- match_entry_get_by_index ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w = make_window(10, "A", "ClassA", "instA", "Normal");
    matching_create_entry(&mgr, &w);

    ASSERT_NOT_NULL("valid index returns entry", match_entry_get_by_index(&mgr, 0));
    ASSERT_NULL("out of bounds returns NULL", match_entry_get_by_index(&mgr, 1));
    ASSERT_NULL("negative index returns NULL", match_entry_get_by_index(&mgr, -1));
    ASSERT_NULL("NULL manager returns NULL", match_entry_get_by_index(NULL, 0));
}

static void test_reassign_entries(void) {
    printf("\n--- match_entry_reassign_live_windows ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w_orig = make_window(100, "Terminal - bash", "gnome-terminal", "gnome-terminal-server", "Normal");
    matching_create_entry(&mgr, &w_orig);
    ASSERT_INT("assigned to 100", 1, mgr.entries[0].assigned);

    // Now window 100 is gone, but window 200 matches the same class/instance/type
    WindowInfo windows[2];
    windows[0] = make_window(200, "Terminal - bash", "gnome-terminal", "gnome-terminal-server", "Normal");
    windows[1] = make_window(300, "Firefox", "Firefox", "Navigator", "Normal");

    int changed = match_entry_reassign_live_windows(&mgr, windows, 2);
    ASSERT_INT("reassignment happened", 1, changed);
    ASSERT_INT("reassigned to 200", 1, (mgr.entries[0].bound_x11_id == 200));
    ASSERT_INT("still assigned", 1, mgr.entries[0].assigned);
    ASSERT_STR("pattern preserved", "Terminal - bash", mgr.entries[0].original_title);

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
    matching_create_entry(&mgr, &w_orig);

    // No matching windows at all
    WindowInfo windows[1];
    windows[0] = make_window(200, "Firefox", "Firefox", "Navigator", "Normal");
    match_entry_reassign_live_windows(&mgr, windows, 1);

    ASSERT_INT("orphaned (unassigned)", 0, mgr.entries[0].assigned);
    ASSERT_STR("pattern still there", "Terminal", mgr.entries[0].original_title);
}

static void test_reassign_skip_already_named(void) {
    printf("\n--- match_entry_reassign_live_windows (skip already named) ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    // Two windows with same class
    WindowInfo w1 = make_window(100, "Terminal - tab1", "gnome-terminal", "gnome-terminal-server", "Normal");
    WindowInfo w2 = make_window(200, "Terminal - tab2", "gnome-terminal", "gnome-terminal-server", "Normal");
    matching_create_entry(&mgr, &w1);
    matching_create_entry(&mgr, &w2);

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
    matching_create_entry(&mgr, &w);

    ASSERT_STR("asterisk escaped to dot on capture", "test.file", mgr.entries[0].original_title);
}

static void test_reassign_wildcard_characterization(void) {
    printf("\n--- match_entry_reassign_live_windows wildcard characterization ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w = make_window(100, "term-*", "ClassA", "instA", "Normal");
    matching_create_entry(&mgr, &w);
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
    matching_create_entry(&mgr, &w);
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

static void test_matching_create_pattern_entry_always_creates(void) {
    printf("\n--- matching_create_pattern_entry always creates ---\n");

    MatchEntryManager manager;
    match_entry_manager_init(&manager);

    int id1 = matching_create_pattern_entry(&manager, "*shared*");
    int id2 = matching_create_pattern_entry(&manager, "*shared*");

    ASSERT_INT("first pattern entry succeeds", 1, id1 > 0);
    ASSERT_INT("second pattern entry gets distinct id", 1, id2 > id1);
    ASSERT_INT("same pattern creates two entries", 2, manager.count);
    ASSERT_STR("first pattern stored", "*shared*", manager.entries[0].original_title);
    ASSERT_STR("second pattern stored", "*shared*", manager.entries[1].original_title);
    ASSERT_STR("pattern entry has no class anchor", "", manager.entries[0].class_name);
    ASSERT_INT("pattern entry is unassigned", 0, manager.entries[0].assigned);
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
    matching_create_entry(&mgr, &w1);
    matching_create_entry(&mgr, &w2);
    int first_id = mgr.entries[0].match_id;
    int second_id = mgr.entries[1].match_id;

    match_entry_delete(&mgr, 0);
    WindowInfo w3 = make_window(30, "C", "ClassC", "instC", "Normal");
    matching_create_entry(&mgr, &w3);
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
    matching_create_entry(&mgr, &w);
    safe_string_copy(mgr.entries[0].original_title, "Orig [brackets] / path", MAX_TITLE_LEN);
    safe_string_copy(mgr.entries[0].class_name, "", sizeof(mgr.entries[0].class_name));
    safe_string_copy(mgr.entries[0].instance, "inst-special", sizeof(mgr.entries[0].instance));
    safe_string_copy(mgr.entries[0].type, "Normal", sizeof(mgr.entries[0].type));
    mgr.entries[0].assigned = 1;

    save_match_entries(&mgr);

    MatchEntryManager loaded;
    load_match_entries(&loaded);
    ASSERT_INT("roundtrip loads one entry", 1, loaded.count);
    ASSERT_STR("original_title roundtrip", "Orig [brackets] / path", loaded.entries[0].original_title);
    ASSERT_STR("class_name roundtrip empty", "", loaded.entries[0].class_name);
    ASSERT_STR("instance roundtrip populated", "inst-special", loaded.entries[0].instance);
    ASSERT_STR("type roundtrip populated", "Normal", loaded.entries[0].type);
}

static void test_bound_x11_id_validation_and_rebind(void) {
    printf("\n--- bound_x11_id: id wins at runtime; pattern fallback after destroy ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);
    WindowInfo captured = make_window(200, "Title-A", "ClassA", "instA", "Normal");
    matching_create_entry(&mgr, &captured);

    // Drifted title keeps binding — id wins.
    WindowInfo drifted = make_window(200, "Title-B", "ClassA", "instA", "Normal");
    int changed = match_entry_reassign_live_windows(&mgr, &drifted, 1);
    ASSERT_INT("drifted title keeps binding (no churn)", 0, changed);
    ASSERT_INT("bound_x11_id unchanged after title drift", 200, (int)mgr.entries[0].bound_x11_id);
    ASSERT_INT("still assigned after title drift", 1, mgr.entries[0].assigned);

    // After window destruction the binding is cleared, then pattern-based re-resolution runs.
    changed = match_entry_reassign_live_windows(&mgr, &drifted, 0);
    ASSERT_INT("destroy clears binding", 1, changed);
    ASSERT_INT("bound_x11_id zero after destroy", 0, (int)mgr.entries[0].bound_x11_id);
    ASSERT_INT("unassigned after destroy", 0, mgr.entries[0].assigned);

    WindowInfo successor = make_window(300, "Title-A", "ClassA", "instA", "Normal");
    changed = match_entry_reassign_live_windows(&mgr, &successor, 1);
    ASSERT_INT("pattern fallback rebinds to matching successor", 1, changed);
    ASSERT_INT("rebound to successor id", 300, (int)mgr.entries[0].bound_x11_id);
    ASSERT_INT("assigned after pattern rebind", 1, mgr.entries[0].assigned);
}

static void test_startup_load_then_reassign_path(void) {
    printf("\n--- startup load->reassign path ---\n");

    set_test_home("startup");
    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);
    WindowInfo w = make_window(100, "Wanted", "ClassA", "instA", "Normal");
    matching_create_entry(&mgr, &w);
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
    matching_create_entry(&mgr, &w);
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

static void test_layout_save_reuses_existing_entry_without_layout_record(void) {
    printf("\n--- layout save reuses existing entry without prior layout ---\n");

    AppData app;
    memset(&app, 0, sizeof(app));
    match_entry_manager_init(&app.matching);
    layout_store_init(&app.layouts);
    app.display = (Display *)0x1;

    app.windows[0] = make_window(0x501, "Geom Existing", "ClassGeom", "instGeom", "Normal");
    app.window_count = 1;

    int existing_match_id = matching_create_entry(&app.matching, &app.windows[0]);
    ASSERT_INT("seed entry created", 1, app.matching.count);
    ASSERT_INT("seed layout count empty", 0, app.layouts.count);
    ASSERT_INT("seed match id positive", 1, existing_match_id > 0);

    ASSERT_INT("layout save succeeds", TRUE,
               save_window_geometry_for_window(&app, &app.windows[0]));
    ASSERT_INT("layout save does not duplicate entry", 1, app.matching.count);
    ASSERT_INT("layout save creates one layout record", 1, app.layouts.count);
    ASSERT_INT("layout uses existing match id", existing_match_id, app.layouts.records[0].match_id);
}

static void test_layout_restore_resolve_requires_live_binding_and_saved_layout(void) {
    printf("\n--- layout restore resolve ---\n");

    MatchEntryManager mgr;
    LayoutStore store;
    match_entry_manager_init(&mgr);
    layout_store_init(&store);

    WindowInfo w = make_window(0x444, "Restore Me", "ClassR", "instR", "Normal");
    matching_create_entry(&mgr, &w);
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
    matching_create_entry(&mgr, &labeled);
    int referenced_id = matching_create_entry(&mgr, &referenced);

    ASSERT_INT("three entries captured before gc", 3, mgr.count);
    ASSERT_INT("gc removes every unreferenced entry", 3,
               match_entry_gc(&mgr, NULL, 0));
    ASSERT_INT("count after empty-reference gc", 0, mgr.count);
    ASSERT_INT("unlabeled entry removed", -1, match_entry_find_index_by_match_id(&mgr, unlabeled_id));
    ASSERT_INT("second entry removed", -1, match_entry_find_index_by_match_id(&mgr, 2));
    ASSERT_INT("unreferenced captured entry removed", -1, match_entry_find_index_by_match_id(&mgr, referenced_id));
}

static void test_binding_kept_on_title_change(void) {
    printf("\n--- match_entry_reassign_live_windows: title change keeps binding ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w = make_window(0xAAA, "foo", "ClassX", "instX", "Normal");
    matching_create_entry(&mgr, &w);
    ASSERT_INT("entry bound to 0xAAA", 1, (int)(mgr.entries[0].bound_x11_id == 0xAAA));
    ASSERT_STR("original title captured", "foo", mgr.entries[0].original_title);

    safe_string_copy(w.title, "bar", MAX_TITLE_LEN);

    match_entry_reassign_live_windows(&mgr, &w, 1);
    ASSERT_INT("binding kept on title change (window alive)", 1,
               (int)(mgr.entries[0].bound_x11_id == 0xAAA));
    ASSERT_INT("still assigned after title change", 1, mgr.entries[0].assigned);
}

static void test_binding_cleared_when_window_gone(void) {
    printf("\n--- match_entry_reassign_live_windows: window gone clears binding ---\n");

    MatchEntryManager mgr;
    match_entry_manager_init(&mgr);

    WindowInfo w = make_window(0xAAA, "foo", "ClassX", "instX", "Normal");
    matching_create_entry(&mgr, &w);
    ASSERT_INT("entry bound to 0xAAA", 1, (int)(mgr.entries[0].bound_x11_id == 0xAAA));

    match_entry_reassign_live_windows(&mgr, &w, 0);
    ASSERT_INT("bound_x11_id cleared when window gone", 0, (int)mgr.entries[0].bound_x11_id);
    ASSERT_INT("entry unassigned when window gone", 0, mgr.entries[0].assigned);
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
    matching_create_entry(&mgr, &labeled);
    int referenced_id = matching_create_entry(&mgr, &referenced);
    int referenced_ids[] = {mgr.entries[1].match_id, referenced_id};

    ASSERT_INT("three entries captured before gc", 3, mgr.count);
    ASSERT_INT("gc removes only unlabeled unreferenced entry", 1,
               match_entry_gc(&mgr, referenced_ids, 2));
    ASSERT_INT("count after gc", 2, mgr.count);
    ASSERT_INT("unlabeled entry removed", -1, match_entry_find_index_by_match_id(&mgr, unlabeled_id));
    ASSERT_INT("referenced second entry kept", 1, match_entry_find_index_by_match_id(&mgr, referenced_ids[0]) >= 0);
    ASSERT_INT("referenced entry kept", 1, match_entry_find_index_by_match_id(&mgr, referenced_id) >= 0);
}

int main(void) {
    printf("Named Window Manager Tests\n");
    printf("==========================\n");

    test_init();
    test_create_entry_captures_identity();
    test_is_window_already_bound();
    test_find_by_index();
    test_delete();
    test_delete_by_match_id();
    test_get_by_index();
    test_reassign_entries();
    test_reassign_no_match();
    test_reassign_skip_already_named();
    test_wildcard_in_title();
    test_reassign_wildcard_characterization();
    test_match_if_set_class_instance_type();
    test_same_title_different_class_create_distinct_entries();
    test_matching_create_entry_always_creates();
    test_matching_create_pattern_entry_always_creates();
    test_matching_create_entry_round_trips_to_source_window();
    test_match_id_persist_and_non_reuse();
    test_load_repairs_malformed_or_duplicate_match_ids();
    test_load_missing_required_fields_and_missing_match_id();
    test_load_realistic_legacy_matching_json_shape();
    test_save_load_roundtrip_special_chars();
    test_bound_x11_id_validation_and_rebind();
    test_startup_load_then_reassign_path();
    test_escaped_star_pattern_persists_as_single_char_wildcard();
    test_layout_save_persists_on_deduped_match_id();
    test_layout_save_reuses_existing_entry_without_layout_record();
    test_layout_restore_resolve_requires_live_binding_and_saved_layout();
    test_match_entry_gc_is_pure_reference_check();
    test_match_entry_gc_removes_only_unreferenced_ids();
    test_binding_kept_on_title_change();
    test_binding_cleared_when_window_gone();

    printf("\n=====================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
