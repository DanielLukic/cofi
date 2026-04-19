#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>

#include "../src/app_data.h"
#include "../src/path_binaries.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(msg, cond) \
    do { \
        tests_run++; \
        if (cond) { \
            tests_passed++; \
            printf("PASS: %s\n", msg); \
        } else { \
            printf("FAIL: %s (line %d)\n", msg, __LINE__); \
        } \
    } while (0)

#define ASSERT_EQ_INT(msg, expected, actual) ASSERT_TRUE(msg, (expected) == (actual))
#define ASSERT_STR_EQ(msg, expected, actual) ASSERT_TRUE(msg, strcmp((expected), (actual)) == 0)

/* ---- stubs required by tab_switching.c (routing tests) ---- */
void filter_windows(AppData *app, const char *filter) { (void)app; (void)filter; }
void filter_names(AppData *app, const char *filter) { (void)app; (void)filter; }
void reset_selection(AppData *app) { (void)app; }
void update_display(AppData *app) { (void)app; }
void apps_load(void) {}

gboolean has_match(const char *query, const char *text) {
    return (query && text && strstr(text, query) != NULL) ? TRUE : FALSE;
}

void build_config_entries(const CofiConfig *config, ConfigEntry entries[], int *count) {
    (void)config;
    (void)entries;
    if (count) {
        *count = 0;
    }
}

static void stub_apps_filter(const char *query, AppEntry *out, int *out_count) {
    (void)query;

    memset(out, 0, sizeof(AppEntry) * 2);
    g_strlcpy(out[0].name, "GitKraken", sizeof(out[0].name));
    out[0].source_kind = APP_SOURCE_DESKTOP;

    g_strlcpy(out[1].name, "Lock", sizeof(out[1].name));
    out[1].source_kind = APP_SOURCE_SYSTEM;

    *out_count = 2;
}

void apps_filter(const char *query, AppEntry *out, int *out_count) {
    stub_apps_filter(query, out, out_count);
}

#include "../src/tab_switching.c"

/* ---- helpers ---- */
static AppEntry make_path_entry(const char *name, const char *exec_path) {
    AppEntry entry;
    memset(&entry, 0, sizeof(entry));
    g_strlcpy(entry.name, name, sizeof(entry.name));
    g_strlcpy(entry.exec_path, exec_path, sizeof(entry.exec_path));
    entry.source_kind = APP_SOURCE_PATH;
    entry.action_id = SYSTEM_ACTION_NONE;
    return entry;
}

static void assert_all_path_entries(const AppEntry *entries, int count, const char *msg) {
    for (int i = 0; i < count; i++) {
        if (entries[i].source_kind != APP_SOURCE_PATH) {
            ASSERT_TRUE(msg, FALSE);
            return;
        }
    }
    ASSERT_TRUE(msg, TRUE);
}

/* ---- tests ---- */
static void test_dedupe_first_in_path_wins(void) {
    path_binaries_reset_for_tests();

    AppEntry chunk1[] = {
        make_path_entry("foo", "/dir1/foo"),
        make_path_entry("bar", "/dir1/bar"),
    };
    AppEntry chunk2[] = {
        make_path_entry("foo", "/dir2/foo"),
        make_path_entry("baz", "/dir2/baz"),
    };

    path_binaries_merge_entries_test_hook(NULL, chunk1, 2, FALSE);
    path_binaries_merge_entries_test_hook(NULL, chunk2, 2, TRUE);

    AppEntry out[MAX_PATH_BINS];
    int out_count = 0;
    path_binaries_filter("", out, &out_count);

    ASSERT_EQ_INT("dedupe count", 3, out_count);
    ASSERT_STR_EQ("foo first path wins", "/dir1/foo", out[2].exec_path);
}

static void test_filter_by_query(void) {
    path_binaries_reset_for_tests();

    AppEntry chunk[] = {
        make_path_entry("git", "/bin/git"),
        make_path_entry("gitk", "/bin/gitk"),
        make_path_entry("grep", "/bin/grep"),
        make_path_entry("awk", "/bin/awk"),
    };

    path_binaries_merge_entries_test_hook(NULL, chunk, 4, TRUE);

    AppEntry out[MAX_PATH_BINS];
    int out_count = 0;
    path_binaries_filter("gi", out, &out_count);

    ASSERT_EQ_INT("query gi count", 2, out_count);
    ASSERT_STR_EQ("query gi first", "git", out[0].name);
    ASSERT_STR_EQ("query gi second", "gitk", out[1].name);
}

static void test_empty_query_returns_all(void) {
    path_binaries_reset_for_tests();

    AppEntry chunk[] = {
        make_path_entry("git", "/bin/git"),
        make_path_entry("gitk", "/bin/gitk"),
        make_path_entry("grep", "/bin/grep"),
    };

    path_binaries_merge_entries_test_hook(NULL, chunk, 3, TRUE);

    AppEntry out[MAX_PATH_BINS];
    int out_count = 0;
    path_binaries_filter("", out, &out_count);

    ASSERT_EQ_INT("empty query returns all", 3, out_count);
}

static void test_dollar_routing_uses_path_only(void) {
    path_binaries_reset_for_tests();

    AppEntry chunk[] = {
        make_path_entry("git", "/usr/bin/git"),
        make_path_entry("gitk", "/usr/bin/gitk"),
    };
    path_binaries_merge_entries_test_hook(NULL, chunk, 2, TRUE);

    AppData app;
    memset(&app, 0, sizeof(app));

    filter_apps(&app, "$git");

    ASSERT_EQ_INT("$ routing count", 2, app.filtered_apps_count);
    assert_all_path_entries(app.filtered_apps, app.filtered_apps_count,
                            "$ routing excludes desktop/system entries");
}

static void test_plain_query_uses_desktop_system_only(void) {
    path_binaries_reset_for_tests();

    AppData app;
    memset(&app, 0, sizeof(app));

    filter_apps(&app, "git");

    ASSERT_EQ_INT("plain routing count", 2, app.filtered_apps_count);
    ASSERT_TRUE("plain routing first is desktop",
                app.filtered_apps[0].source_kind == APP_SOURCE_DESKTOP);
    ASSERT_TRUE("plain routing second is system",
                app.filtered_apps[1].source_kind == APP_SOURCE_SYSTEM);
}

static void test_chunk_merge_atomicity_unique_count(void) {
    path_binaries_reset_for_tests();

    AppEntry chunk1[] = {
        make_path_entry("ls", "/bin/ls"),
        make_path_entry("cp", "/bin/cp"),
    };
    AppEntry chunk2[] = {
        make_path_entry("ls", "/usr/bin/ls"),
        make_path_entry("mv", "/bin/mv"),
    };

    path_binaries_merge_entries_test_hook(NULL, chunk1, 2, FALSE);
    path_binaries_merge_entries_test_hook(NULL, chunk2, 2, TRUE);

    AppEntry out[MAX_PATH_BINS];
    int out_count = 0;
    path_binaries_filter("", out, &out_count);

    ASSERT_EQ_INT("chunk merge unique count", 3, out_count);
}

static void test_monitor_delete_updates_cache(void) {
    path_binaries_reset_for_tests();

    AppEntry chunk[] = {
        make_path_entry("foo", "/tmp/pathbin-delete/foo"),
    };
    path_binaries_merge_entries_test_hook(NULL, chunk, 1, TRUE);

    GFile *file = g_file_new_for_path("/tmp/pathbin-delete/foo");
    path_binaries_on_monitor_event_test_hook(file, NULL, G_FILE_MONITOR_EVENT_DELETED);
    g_object_unref(file);

    AppEntry out[MAX_PATH_BINS];
    int out_count = 0;
    path_binaries_filter("", out, &out_count);
    ASSERT_EQ_INT("monitor delete removes entry", 0, out_count);
}

static void test_monitor_create_updates_cache(void) {
    path_binaries_reset_for_tests();

    gchar *tmp_dir = g_dir_make_tmp("cofi-path-bins-XXXXXX", NULL);
    ASSERT_TRUE("tmp dir created", tmp_dir != NULL);
    if (!tmp_dir) {
        return;
    }

    gchar *file_path = g_build_filename(tmp_dir, "newbin", NULL);
    const char script[] = "#!/bin/sh\nexit 0\n";
    g_file_set_contents(file_path, script, -1, NULL);
    g_chmod(file_path, 0755);

    GFile *file = g_file_new_for_path(file_path);
    path_binaries_on_monitor_event_test_hook(file, NULL, G_FILE_MONITOR_EVENT_CREATED);
    g_object_unref(file);

    AppEntry out[MAX_PATH_BINS];
    int out_count = 0;
    path_binaries_filter("", out, &out_count);

    ASSERT_EQ_INT("monitor create adds entry", 1, out_count);
    ASSERT_STR_EQ("monitor create entry name", "newbin", out[0].name);

    g_remove(file_path);
    g_rmdir(tmp_dir);
    g_free(file_path);
    g_free(tmp_dir);
}

static void test_global_cap_overflow_sets_warned(void) {
    path_binaries_reset_for_tests();

    int total = MAX_PATH_BINS + 10;
    AppEntry *entries = calloc((size_t)total, sizeof(AppEntry));
    ASSERT_TRUE("cap test alloc entries", entries != NULL);
    if (!entries) {
        return;
    }

    for (int i = 0; i < total; i++) {
        char name[64];
        char exec_path[128];
        snprintf(name, sizeof(name), "capcmd_%d", i);
        snprintf(exec_path, sizeof(exec_path), "/tmp/capcmd_%d", i);
        entries[i] = make_path_entry(name, exec_path);
    }

    path_binaries_merge_entries_test_hook(NULL, entries, total, TRUE);

    AppEntry out[MAX_PATH_BINS + 16];
    int out_count = 0;
    path_binaries_filter("", out, &out_count);

    ASSERT_EQ_INT("cap overflow clamps count to MAX_APPS", MAX_APPS, out_count);
    ASSERT_TRUE("cap overflow sets warned", path_binaries_cap_warned_for_tests());

    free(entries);
}

static void test_cap_warning_emits_once(void) {
    path_binaries_reset_for_tests();

    int total = MAX_PATH_BINS + 10;
    AppEntry *entries = calloc((size_t)total, sizeof(AppEntry));
    ASSERT_TRUE("cap-once alloc entries", entries != NULL);
    if (!entries) {
        return;
    }

    for (int i = 0; i < total; i++) {
        char name[64];
        char exec_path[128];
        snprintf(name, sizeof(name), "oncecmd_%d", i);
        snprintf(exec_path, sizeof(exec_path), "/tmp/oncecmd_%d", i);
        entries[i] = make_path_entry(name, exec_path);
    }

    path_binaries_merge_entries_test_hook(NULL, entries, total, FALSE);
    path_binaries_merge_entries_test_hook(NULL, entries, total, FALSE);
    path_binaries_merge_entries_test_hook(NULL, entries, total, TRUE);

    ASSERT_EQ_INT("cap warning emitted once", 1, path_binaries_cap_warn_count_for_tests());

    free(entries);
}

int main(void) {
    test_dedupe_first_in_path_wins();
    test_filter_by_query();
    test_empty_query_returns_all();
    test_dollar_routing_uses_path_only();
    test_plain_query_uses_desktop_system_only();
    test_chunk_merge_atomicity_unique_count();
    test_monitor_delete_updates_cache();
    test_monitor_create_updates_cache();
    test_global_cap_overflow_sets_warned();
    test_cap_warning_emits_once();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
