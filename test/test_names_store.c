#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/app/app_data.h"
#include "names/names_store.h"
#include "core/utils/utils.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(msg, cond) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("PASS: %s\n", msg); } \
    else { printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

static void tmp_path(char *out, size_t out_size, const char *name) {
    snprintf(out, out_size, "/tmp/cofi-names-store-%ld-%s.json", (long)getpid(), name);
    unlink(out);
}

static void set_test_home(const char *name) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/cofi-names-home-%ld-%s", (long)getpid(), name);
    mkdir(path, 0755);
    setenv("HOME", path, 1);
}

static WindowInfo make_window(Window id, const char *title, const char *class_name) {
    WindowInfo w = {0};
    w.id = id;
    safe_string_copy(w.title, title, sizeof(w.title));
    safe_string_copy(w.class_name, class_name, sizeof(w.class_name));
    safe_string_copy(w.instance, "inst", sizeof(w.instance));
    safe_string_copy(w.type, "Normal", sizeof(w.type));
    return w;
}

static void test_upsert_remove_collect(void) {
    NamesStore store;
    names_store_init_with_path(&store, "/tmp/unused-names.json");

    ASSERT_TRUE("rejects bad match id", !names_store_set(&store, 0, "bad"));
    ASSERT_TRUE("rejects empty name", !names_store_set(&store, 1, ""));
    ASSERT_TRUE("insert succeeds", names_store_set(&store, 10, "alpha"));
    ASSERT_TRUE("upsert succeeds", names_store_set(&store, 10, "beta"));
    ASSERT_TRUE("upsert keeps one record", store.count == 1);
    ASSERT_TRUE("lookup returns updated name",
                strcmp(names_store_get_by_match_id(&store, 10), "beta") == 0);
    ASSERT_TRUE("find by custom name", names_store_find_by_custom_name(&store, "beta") == &store.records[0]);

    ASSERT_TRUE("insert second succeeds", names_store_set(&store, 20, "gamma"));
    int ids[4] = {0};
    ASSERT_TRUE("collect ids count", names_store_collect_ids(&store, ids, 4) == 2);
    ASSERT_TRUE("collect ids order", ids[0] == 10 && ids[1] == 20);
    ASSERT_TRUE("remove existing", names_store_remove_by_match_id(&store, 10));
    ASSERT_TRUE("remove compacts", store.count == 1 && store.records[0].match_id == 20);
    ASSERT_TRUE("remove missing false", !names_store_remove_by_match_id(&store, 999));
}

static void test_save_load_and_malformed_tolerance(void) {
    char path[256];
    tmp_path(path, sizeof(path), "roundtrip");

    NamesStore store;
    names_store_init_with_path(&store, path);
    ASSERT_TRUE("set alpha", names_store_set(&store, 7, "alpha"));
    ASSERT_TRUE("set beta", names_store_set(&store, 9, "beta"));
    ASSERT_TRUE("save succeeds", names_store_save(&store));

    NamesStore loaded;
    names_store_init_with_path(&loaded, path);
    ASSERT_TRUE("load succeeds", names_store_load(&loaded));
    ASSERT_TRUE("load count", loaded.count == 2);
    ASSERT_TRUE("load alpha", strcmp(names_store_get_by_match_id(&loaded, 7), "alpha") == 0);
    ASSERT_TRUE("load beta", strcmp(names_store_get_by_match_id(&loaded, 9), "beta") == 0);

    FILE *f = fopen(path, "w");
    fprintf(f, "{ \"names\": ["
               "{ \"match_id\": 11, \"custom_name\": \"ok\", \"ignored\": true },"
               "{ \"match_id\": 0, \"custom_name\": \"bad\" },"
               "{ \"match_id\": 12 },"
               "42 ] }");
    fclose(f);

    ASSERT_TRUE("malformed load succeeds", names_store_load(&loaded));
    ASSERT_TRUE("malformed load keeps valid only", loaded.count == 1);
    ASSERT_TRUE("unknown fields ignored", strcmp(names_store_get_by_match_id(&loaded, 11), "ok") == 0);

    f = fopen(path, "w");
    fprintf(f, "{ broken");
    fclose(f);
    ASSERT_TRUE("corrupt load returns empty success", names_store_load(&loaded));
    ASSERT_TRUE("corrupt load empties store", loaded.count == 0);
}

static void test_get_for_window_matches_current_identity_not_stale_binding(void) {
    MatchEntryManager manager;
    NamesStore store;
    match_entry_manager_init(&manager);
    names_store_init_with_path(&store, "/tmp/unused-names.json");

    WindowInfo terminal = make_window(0x100, "Terminal", "Alacritty");
    int terminal_id = matching_create_entry(&manager, &terminal);
    ASSERT_TRUE("terminal entry created", terminal_id > 0);
    ASSERT_TRUE("terminal name set", names_store_set(&store, terminal_id, "term"));

    WindowInfo stream = make_window(0x100, "Stream", "Alacritty");
    int stream_id = matching_create_entry(&manager, &stream);
    ASSERT_TRUE("stream entry created", stream_id > 0);
    ASSERT_TRUE("stream name set", names_store_set(&store, stream_id, "video"));

    manager.entries[0].bound_x11_id = stream.id;
    manager.entries[0].assigned = 1;
    manager.entries[1].bound_x11_id = stream.id;
    manager.entries[1].assigned = 1;

    const char *name = names_get_for_window(&store, &manager, &stream);
    ASSERT_TRUE("current matching identity wins over stale binding",
                name && strcmp(name, "video") == 0);
}

static void test_assign_updates_existing_name_record(void) {
    set_test_home("assign");

    AppData app;
    memset(&app, 0, sizeof(app));
    match_entry_manager_init(&app.matching);
    names_store_init_with_path(&app.names, "/tmp/cofi-names-assign.json");
    unlink(app.names.path);

    WindowInfo window = make_window(0x200, "Invoice", "Alacritty");
    ASSERT_TRUE("first assign succeeds", names_assign_window(&app, &window, "billing"));
    ASSERT_TRUE("one entry after first assign", app.matching.count == 1);
    ASSERT_TRUE("one name after first assign", app.names.count == 1);
    int match_id = app.names.records[0].match_id;

    ASSERT_TRUE("rename succeeds", names_assign_window(&app, &window, "accounts"));
    ASSERT_TRUE("rename keeps one entry", app.matching.count == 1);
    ASSERT_TRUE("rename keeps one name", app.names.count == 1);
    ASSERT_TRUE("rename preserves match id", app.names.records[0].match_id == match_id);
    ASSERT_TRUE("rename updates name", strcmp(app.names.records[0].custom_name, "accounts") == 0);
}

int main(void) {
    test_upsert_remove_collect();
    test_save_load_and_malformed_tolerance();
    test_get_for_window_matches_current_identity_not_stale_binding();
    test_assign_updates_existing_name_record();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
