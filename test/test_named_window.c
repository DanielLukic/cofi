#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../src/named_window.h"
#include "../src/named_window_config.h"
#include "../src/window_matcher.h"
#include "../src/utils.h"
#include "../src/app_data.h"

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
    printf("\n--- init_named_window_manager ---\n");

    NamedWindowManager mgr;
    init_named_window_manager(&mgr);
    ASSERT_INT("count starts at 0", 0, mgr.count);

    // NULL is safe
    init_named_window_manager(NULL);
    printf("PASS: NULL init does not crash\n");
    tests_passed++;
}

static void test_assign_and_get(void) {
    printf("\n--- assign_custom_name / get_window_custom_name ---\n");

    NamedWindowManager mgr;
    init_named_window_manager(&mgr);

    WindowInfo w = make_window(100, "Firefox - Home", "Firefox", "Navigator", "Normal");
    assign_custom_name(&mgr, &w, "browser");

    ASSERT_INT("count after assign", 1, mgr.count);
    ASSERT_NOT_NULL("custom name not NULL", get_window_custom_name(&mgr, 100));
    ASSERT_STR("custom name value", "browser", get_window_custom_name(&mgr, 100));

    // Update existing
    assign_custom_name(&mgr, &w, "web");
    ASSERT_INT("count unchanged after update", 1, mgr.count);
    ASSERT_STR("updated name", "web", get_window_custom_name(&mgr, 100));

    // Non-existent window
    ASSERT_NULL("unassigned window returns NULL", get_window_custom_name(&mgr, 999));

    // NULL/empty edge cases
    assign_custom_name(&mgr, &w, "");
    ASSERT_INT("empty name ignored", 1, mgr.count);

    assign_custom_name(&mgr, NULL, "test");
    ASSERT_INT("NULL window ignored", 1, mgr.count);

    assign_custom_name(NULL, &w, "test");
    // Should not crash
    printf("PASS: NULL manager does not crash\n");
    tests_passed++;

    // Zero window id
    ASSERT_NULL("id 0 returns NULL", get_window_custom_name(&mgr, 0));
    ASSERT_NULL("NULL manager returns NULL", get_window_custom_name(NULL, 100));
}

static void test_is_window_already_named(void) {
    printf("\n--- is_window_already_named ---\n");

    NamedWindowManager mgr;
    init_named_window_manager(&mgr);

    WindowInfo w = make_window(200, "Terminal", "gnome-terminal", "gnome-terminal", "Normal");
    ASSERT_INT("not named initially", 0, is_window_already_named(&mgr, 200));

    assign_custom_name(&mgr, &w, "term");
    ASSERT_INT("named after assign", 1, is_window_already_named(&mgr, 200));
    ASSERT_INT("different window not named", 0, is_window_already_named(&mgr, 201));
}

static void test_find_by_index(void) {
    printf("\n--- find_named_window_index ---\n");

    NamedWindowManager mgr;
    init_named_window_manager(&mgr);

    WindowInfo w1 = make_window(10, "A", "ClassA", "instA", "Normal");
    WindowInfo w2 = make_window(20, "B", "ClassB", "instB", "Normal");
    assign_custom_name(&mgr, &w1, "first");
    assign_custom_name(&mgr, &w2, "second");

    ASSERT_INT("find first", 0, find_named_window_index(&mgr, 10));
    ASSERT_INT("find second", 1, find_named_window_index(&mgr, 20));
    ASSERT_INT("find missing", -1, find_named_window_index(&mgr, 30));
    ASSERT_INT("find id 0", -1, find_named_window_index(&mgr, 0));
    ASSERT_INT("find NULL mgr", -1, find_named_window_index(NULL, 10));
}

static void test_find_by_name(void) {
    printf("\n--- find_named_window_by_name ---\n");

    NamedWindowManager mgr;
    init_named_window_manager(&mgr);

    WindowInfo w1 = make_window(10, "A", "ClassA", "instA", "Normal");
    WindowInfo w2 = make_window(20, "B", "ClassB", "instB", "Normal");
    assign_custom_name(&mgr, &w1, "alpha");
    assign_custom_name(&mgr, &w2, "beta");

    ASSERT_INT("find alpha", 0, find_named_window_by_name(&mgr, "alpha"));
    ASSERT_INT("find beta", 1, find_named_window_by_name(&mgr, "beta"));
    ASSERT_INT("find missing", -1, find_named_window_by_name(&mgr, "gamma"));
    ASSERT_INT("find NULL name", -1, find_named_window_by_name(&mgr, NULL));
    ASSERT_INT("find NULL mgr", -1, find_named_window_by_name(NULL, "alpha"));
}

static void test_delete(void) {
    printf("\n--- delete_custom_name ---\n");

    NamedWindowManager mgr;
    init_named_window_manager(&mgr);

    WindowInfo w1 = make_window(10, "A", "ClassA", "instA", "Normal");
    WindowInfo w2 = make_window(20, "B", "ClassB", "instB", "Normal");
    WindowInfo w3 = make_window(30, "C", "ClassC", "instC", "Normal");
    assign_custom_name(&mgr, &w1, "first");
    assign_custom_name(&mgr, &w2, "second");
    assign_custom_name(&mgr, &w3, "third");

    ASSERT_INT("count is 3", 3, mgr.count);

    // Delete middle
    delete_custom_name(&mgr, 1);
    ASSERT_INT("count after delete middle", 2, mgr.count);
    ASSERT_STR("first still there", "first", mgr.entries[0].custom_name);
    ASSERT_STR("third shifted to index 1", "third", mgr.entries[1].custom_name);

    // Delete first
    delete_custom_name(&mgr, 0);
    ASSERT_INT("count after delete first", 1, mgr.count);
    ASSERT_STR("third now at index 0", "third", mgr.entries[0].custom_name);

    // Delete last
    delete_custom_name(&mgr, 0);
    ASSERT_INT("count after delete last", 0, mgr.count);

    // Edge cases: invalid indices
    delete_custom_name(&mgr, -1);
    delete_custom_name(&mgr, 0);
    delete_custom_name(&mgr, 100);
    delete_custom_name(NULL, 0);
    printf("PASS: invalid delete indices do not crash\n");
    tests_passed++;
}

static void test_update_name(void) {
    printf("\n--- update_custom_name ---\n");

    NamedWindowManager mgr;
    init_named_window_manager(&mgr);

    WindowInfo w = make_window(10, "A", "ClassA", "instA", "Normal");
    assign_custom_name(&mgr, &w, "original");

    update_custom_name(&mgr, 0, "updated");
    ASSERT_STR("name updated", "updated", mgr.entries[0].custom_name);
    ASSERT_INT("count unchanged", 1, mgr.count);

    // Edge cases
    update_custom_name(&mgr, -1, "bad");
    update_custom_name(&mgr, 99, "bad");
    update_custom_name(&mgr, 0, NULL);
    update_custom_name(NULL, 0, "bad");
    printf("PASS: invalid update args do not crash\n");
    tests_passed++;
}

static void test_get_by_index(void) {
    printf("\n--- get_named_window_by_index ---\n");

    NamedWindowManager mgr;
    init_named_window_manager(&mgr);

    WindowInfo w = make_window(10, "A", "ClassA", "instA", "Normal");
    assign_custom_name(&mgr, &w, "test");

    ASSERT_NOT_NULL("valid index returns entry", get_named_window_by_index(&mgr, 0));
    ASSERT_NULL("out of bounds returns NULL", get_named_window_by_index(&mgr, 1));
    ASSERT_NULL("negative index returns NULL", get_named_window_by_index(&mgr, -1));
    ASSERT_NULL("NULL manager returns NULL", get_named_window_by_index(NULL, 0));
}

static void test_reassign_names(void) {
    printf("\n--- check_and_reassign_names ---\n");

    NamedWindowManager mgr;
    init_named_window_manager(&mgr);

    // Assign a name to window 100
    WindowInfo w_orig = make_window(100, "Terminal - bash", "gnome-terminal", "gnome-terminal-server", "Normal");
    assign_custom_name(&mgr, &w_orig, "term");
    ASSERT_INT("assigned to 100", 1, mgr.entries[0].assigned);

    // Now window 100 is gone, but window 200 matches the same class/instance/type
    WindowInfo windows[2];
    windows[0] = make_window(200, "Terminal - bash", "gnome-terminal", "gnome-terminal-server", "Normal");
    windows[1] = make_window(300, "Firefox", "Firefox", "Navigator", "Normal");

    int changed = check_and_reassign_names(&mgr, windows, 2);
    ASSERT_INT("reassignment happened", 1, changed);
    ASSERT_INT("reassigned to 200", 1, (mgr.entries[0].bound_x11_id == 200));
    ASSERT_INT("still assigned", 1, mgr.entries[0].assigned);
    ASSERT_STR("name preserved", "term", mgr.entries[0].custom_name);

    // If window still exists, no reassignment needed
    changed = check_and_reassign_names(&mgr, windows, 2);
    ASSERT_INT("no change when window exists", 0, changed);

    // NULL safety
    ASSERT_INT("NULL manager", 0, (int)check_and_reassign_names(NULL, windows, 2));
    ASSERT_INT("NULL windows", 0, (int)check_and_reassign_names(&mgr, NULL, 2));
}

static void test_reassign_no_match(void) {
    printf("\n--- check_and_reassign_names (orphaned) ---\n");

    NamedWindowManager mgr;
    init_named_window_manager(&mgr);

    WindowInfo w_orig = make_window(100, "Terminal", "gnome-terminal", "gnome-terminal-server", "Normal");
    assign_custom_name(&mgr, &w_orig, "term");

    // No matching windows at all
    WindowInfo windows[1];
    windows[0] = make_window(200, "Firefox", "Firefox", "Navigator", "Normal");
    check_and_reassign_names(&mgr, windows, 1);

    ASSERT_INT("orphaned (unassigned)", 0, mgr.entries[0].assigned);
    ASSERT_STR("name still there", "term", mgr.entries[0].custom_name);
}

static void test_reassign_skip_already_named(void) {
    printf("\n--- check_and_reassign_names (skip already named) ---\n");

    NamedWindowManager mgr;
    init_named_window_manager(&mgr);

    // Two windows with same class
    WindowInfo w1 = make_window(100, "Terminal - tab1", "gnome-terminal", "gnome-terminal-server", "Normal");
    WindowInfo w2 = make_window(200, "Terminal - tab2", "gnome-terminal", "gnome-terminal-server", "Normal");
    assign_custom_name(&mgr, &w1, "tab1");
    assign_custom_name(&mgr, &w2, "tab2");

    // Now only window 200 remains - window 100 is gone
    WindowInfo remaining[1];
    remaining[0] = make_window(200, "Terminal - tab2", "gnome-terminal", "gnome-terminal-server", "Normal");

    check_and_reassign_names(&mgr, remaining, 1);

    // tab2 keeps window 200, tab1 becomes orphaned (not stolen from tab2)
    ASSERT_INT("tab2 still assigned", 1, mgr.entries[1].assigned);
    ASSERT_INT("tab2 still on 200", 1, (mgr.entries[1].bound_x11_id == 200));
    ASSERT_INT("tab1 orphaned", 0, mgr.entries[0].assigned);
}

static void test_wildcard_in_title(void) {
    printf("\n--- wildcard matching in title storage ---\n");

    NamedWindowManager mgr;
    init_named_window_manager(&mgr);

    // Window with asterisk in title is stored literally (no conversion in PR2)
    WindowInfo w = make_window(100, "test*file", "Class", "inst", "Normal");
    assign_custom_name(&mgr, &w, "myfile");

    ASSERT_STR("asterisk preserved literally", "test*file", mgr.entries[0].original_title);
}

static void test_reassign_title_mode_characterization(void) {
    printf("\n--- check_and_reassign_names title mode characterization ---\n");

    NamedWindowManager mgr;
    init_named_window_manager(&mgr);

    WindowInfo w = make_window(100, "term-*", "ClassA", "instA", "Normal");
    assign_custom_name(&mgr, &w, "term");
    ASSERT_STR("capture stores literal title", "term-*", mgr.entries[0].original_title);
    ASSERT_INT("capture defaults to EXACT mode", TITLE_MATCH_MODE_EXACT, mgr.entries[0].match_mode);

    mgr.entries[0].bound_x11_id = 999;
    mgr.entries[0].assigned = 1;
    WindowInfo exact_candidate = make_window(200, "term-1", "ClassA", "instA", "Normal");
    int exact_changed = check_and_reassign_names(&mgr, &exact_candidate, 1);
    ASSERT_INT("EXACT mode reports changed when stale binding is cleared", 1, exact_changed);
    ASSERT_INT("EXACT mode leaves entry orphaned on title mismatch", 0, mgr.entries[0].assigned);

    mgr.entries[0].bound_x11_id = 998;
    mgr.entries[0].assigned = 1;
    mgr.entries[0].match_mode = TITLE_MATCH_MODE_GLOB;
    safe_string_copy(mgr.entries[0].original_title, "term-*", MAX_TITLE_LEN);
    WindowInfo glob_candidate = make_window(300, "term-xyz", "ClassA", "instA", "Normal");
    ASSERT_INT("GLOB mode matches wildcard title", 1, (int)check_and_reassign_names(&mgr, &glob_candidate, 1));
    ASSERT_INT("GLOB mode rebinds matched window", 1, (mgr.entries[0].bound_x11_id == 300));
}

static void test_reassign_requires_exact_class_instance_type(void) {
    printf("\n--- check_and_reassign_names requires exact class/instance/type ---\n");

    NamedWindowManager mgr;
    init_named_window_manager(&mgr);

    WindowInfo w = make_window(100, "term-1", "ClassA", "instA", "Normal");
    assign_custom_name(&mgr, &w, "term");
    mgr.entries[0].bound_x11_id = 999;
    mgr.entries[0].assigned = 1;
    safe_string_copy(mgr.entries[0].original_title, "term-1", MAX_TITLE_LEN);

    WindowInfo wrong_class = make_window(200, "term-1", "ClassB", "instA", "Normal");
    int wrong_class_changed = check_and_reassign_names(&mgr, &wrong_class, 1);
    ASSERT_INT("wrong class clears stale binding", 1, wrong_class_changed);
    ASSERT_INT("wrong class leaves entry orphaned", 0, mgr.entries[0].assigned);

    mgr.entries[0].bound_x11_id = 998;
    mgr.entries[0].assigned = 1;
    WindowInfo wrong_instance = make_window(201, "term-1", "ClassA", "instB", "Normal");
    int wrong_instance_changed = check_and_reassign_names(&mgr, &wrong_instance, 1);
    ASSERT_INT("wrong instance clears stale binding", 1, wrong_instance_changed);
    ASSERT_INT("wrong instance leaves entry orphaned", 0, mgr.entries[0].assigned);

    mgr.entries[0].bound_x11_id = 997;
    mgr.entries[0].assigned = 1;
    WindowInfo wrong_type = make_window(202, "term-1", "ClassA", "instA", "Special");
    int wrong_type_changed = check_and_reassign_names(&mgr, &wrong_type, 1);
    ASSERT_INT("wrong type clears stale binding", 1, wrong_type_changed);
    ASSERT_INT("wrong type leaves entry orphaned", 0, mgr.entries[0].assigned);
}

static void set_test_home(const char *suffix) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/cofi-named-window-%s-%ld", suffix, (long)getpid());
    mkdir(path, 0755);
    setenv("HOME", path, 1);
}

static void test_matching_capture_or_get_dedups_same_window(void) {
    printf("\n--- matching_capture_or_get dedup ---\n");

    AppData app = {0};
    init_named_window_manager(&app.names);
    WindowInfo w = make_window(111, "Editor", "Code", "code", "Normal");
    app.windows[0] = w;
    app.window_count = 1;

    int id1 = matching_capture_or_get(&app, &w);
    int id2 = matching_capture_or_get(&app, &w);

    ASSERT_INT("first capture succeeds", 1, id1 > 0);
    ASSERT_INT("second capture returns same match id", id1, id2);
    ASSERT_INT("capture dedup keeps one entry", 1, app.names.count);
}

static void test_match_id_persist_and_non_reuse(void) {
    printf("\n--- match_id persistence and non-reuse ---\n");

    set_test_home("ids");
    NamedWindowManager mgr;
    init_named_window_manager(&mgr);

    WindowInfo w1 = make_window(10, "A", "ClassA", "instA", "Normal");
    WindowInfo w2 = make_window(20, "B", "ClassB", "instB", "Normal");
    assign_custom_name(&mgr, &w1, "first");
    assign_custom_name(&mgr, &w2, "second");
    int first_id = mgr.entries[0].match_id;
    int second_id = mgr.entries[1].match_id;

    delete_custom_name(&mgr, 0);
    WindowInfo w3 = make_window(30, "C", "ClassC", "instC", "Normal");
    assign_custom_name(&mgr, &w3, "third");
    int third_id = mgr.entries[1].match_id;

    ASSERT_INT("second id remains stable", second_id, mgr.entries[0].match_id);
    ASSERT_INT("new id is not reused", 1, third_id > second_id && third_id != first_id);

    save_named_windows(&mgr);
    NamedWindowManager loaded;
    load_named_windows(&loaded);
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
    snprintf(config_path, sizeof(config_path), "%s/.config/cofi/names.json", home);
    mkdir(config_root, 0755);
    mkdir(config_dir, 0755);

    FILE *f = fopen(config_path, "w");
    ASSERT_NOT_NULL("test file opened", f);
    if (!f) return;
    fprintf(f,
            "{\n"
            "  \"next_match_id\": 1,\n"
            "  \"named_windows\": [\n"
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

    NamedWindowManager loaded;
    load_named_windows(&loaded);
    ASSERT_INT("loaded malformed/duplicate entries", 3, loaded.count);
    ASSERT_INT("id[0] positive", 1, loaded.entries[0].match_id > 0);
    ASSERT_INT("id[1] positive", 1, loaded.entries[1].match_id > 0);
    ASSERT_INT("id[2] positive", 1, loaded.entries[2].match_id > 0);
    ASSERT_INT("id[0] != id[1]", 1, loaded.entries[0].match_id != loaded.entries[1].match_id);
    ASSERT_INT("id[1] != id[2]", 1, loaded.entries[1].match_id != loaded.entries[2].match_id);
    ASSERT_INT("next_match_id monotonic", 1, loaded.next_match_id > loaded.entries[2].match_id);
}

static void test_bound_x11_id_validation_and_rebind(void) {
    printf("\n--- bound_x11_id validation and rebind ---\n");

    NamedWindowManager mgr;
    init_named_window_manager(&mgr);
    WindowInfo captured = make_window(200, "Title-A", "ClassA", "instA", "Normal");
    assign_custom_name(&mgr, &captured, "name");

    // Same id + drifted title is still trusted (class/instance/type validation only).
    WindowInfo drifted = make_window(200, "Title-B", "ClassA", "instA", "Normal");
    int changed = check_and_reassign_names(&mgr, &drifted, 1);
    ASSERT_INT("drifted title keeps trusted binding", 0, changed);
    ASSERT_INT("binding preserved on drifted title", 200, (int)mgr.entries[0].bound_x11_id);
    ASSERT_INT("entry remains assigned", 1, mgr.entries[0].assigned);

    // Reused id with wrong class should be dropped, then rebound by criteria.
    safe_string_copy(mgr.entries[0].original_title, "Wanted", MAX_TITLE_LEN);
    WindowInfo windows[2];
    windows[0] = make_window(200, "Wanted", "OtherClass", "instA", "Normal");
    windows[1] = make_window(300, "Wanted", "ClassA", "instA", "Normal");
    changed = check_and_reassign_names(&mgr, windows, 2);
    ASSERT_INT("class mismatch triggers rebind path", 1, changed);
    ASSERT_INT("rebound to matching criteria window", 300, (int)mgr.entries[0].bound_x11_id);
}

static void test_startup_load_then_reassign_path(void) {
    printf("\n--- startup load->reassign path ---\n");

    set_test_home("startup");
    NamedWindowManager mgr;
    init_named_window_manager(&mgr);
    WindowInfo w = make_window(100, "Wanted", "ClassA", "instA", "Normal");
    assign_custom_name(&mgr, &w, "name");
    save_named_windows(&mgr);

    NamedWindowManager loaded;
    load_named_windows(&loaded);
    ASSERT_INT("loaded one entry", 1, loaded.count);
    ASSERT_INT("loaded starts assigned", 1, loaded.entries[0].assigned);

    WindowInfo windows[1];
    windows[0] = make_window(300, "Wanted", "ClassA", "instA", "Normal");
    int changed = check_and_reassign_names(&loaded, windows, 1);
    ASSERT_INT("startup reassign changed binding", 1, changed);
    ASSERT_INT("startup rebound to live window", 300, (int)loaded.entries[0].bound_x11_id);
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
    test_reassign_title_mode_characterization();
    test_reassign_requires_exact_class_instance_type();
    test_matching_capture_or_get_dedups_same_window();
    test_match_id_persist_and_non_reuse();
    test_load_repairs_malformed_or_duplicate_match_ids();
    test_bound_x11_id_validation_and_rebind();
    test_startup_load_then_reassign_path();

    printf("\n=====================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
