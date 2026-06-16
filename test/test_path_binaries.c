#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>

#include "core/app/app_data.h"
#include "path/path_binaries.h"
#include "providers/cofi_tab_provider.h"

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

const CofiTabProvider *cofi_get_provider_for_tab(int tab_mode) { (void)tab_mode; return NULL; }
const CofiTabProvider *cofi_get_provider_for_prefix(char prefix) { (void)prefix; return NULL; }
const CofiTabProvider *cofi_get_provider_for_tab_prefix(char prefix) { (void)prefix; return NULL; }
int cofi_get_provider_id_for_tab(int tab_mode) { (void)tab_mode; return -1; }
int cofi_next_generation(int provider_id) { (void)provider_id; return -1; }
TabMode path_tab_mode(void) { return (TabMode)(TAB_COUNT + 2); }
void update_display(AppData *app) { (void)app; }

static PathEntry make_path_entry(const char *name, const char *exec_path) {
    PathEntry entry;
    memset(&entry, 0, sizeof(entry));
    g_strlcpy(entry.name, name, sizeof(entry.name));
    g_strlcpy(entry.exec_path, exec_path, sizeof(entry.exec_path));
    return entry;
}

static void test_dedupe_first_in_path_wins(void) {
    path_binaries_reset_for_tests();

    PathEntry chunk1[] = {
        make_path_entry("foo", "/dir1/foo"),
        make_path_entry("bar", "/dir1/bar"),
    };
    PathEntry chunk2[] = {
        make_path_entry("foo", "/dir2/foo"),
        make_path_entry("baz", "/dir2/baz"),
    };

    path_binaries_merge_entries_test_hook(NULL, chunk1, 2, FALSE);
    path_binaries_merge_entries_test_hook(NULL, chunk2, 2, TRUE);

    PathEntry out[MAX_PATH_BINS];
    int out_count = 0;
    path_binaries_filter("", out, &out_count);

    ASSERT_EQ_INT("dedupe count", 3, out_count);
    ASSERT_STR_EQ("foo first path wins", "/dir1/foo", out[2].exec_path);
}

static void test_filter_by_query(void) {
    path_binaries_reset_for_tests();

    PathEntry chunk[] = {
        make_path_entry("git", "/bin/git"),
        make_path_entry("gitk", "/bin/gitk"),
        make_path_entry("grep", "/bin/grep"),
        make_path_entry("awk", "/bin/awk"),
    };

    path_binaries_merge_entries_test_hook(NULL, chunk, 4, TRUE);

    PathEntry out[MAX_PATH_BINS];
    int out_count = 0;
    path_binaries_filter("gi", out, &out_count);

    ASSERT_EQ_INT("query gi count", 2, out_count);
    ASSERT_STR_EQ("query gi first", "git", out[0].name);
    ASSERT_STR_EQ("query gi second", "gitk", out[1].name);
}

static void test_empty_query_returns_all(void) {
    path_binaries_reset_for_tests();

    PathEntry chunk[] = {
        make_path_entry("git", "/bin/git"),
        make_path_entry("gitk", "/bin/gitk"),
        make_path_entry("grep", "/bin/grep"),
    };

    path_binaries_merge_entries_test_hook(NULL, chunk, 3, TRUE);

    PathEntry out[MAX_PATH_BINS];
    int out_count = 0;
    path_binaries_filter("", out, &out_count);

    ASSERT_EQ_INT("empty query returns all", 3, out_count);
}

static void test_empty_query_is_alphabetical(void) {
    path_binaries_reset_for_tests();

    PathEntry chunk[] = {
        make_path_entry("zstd", "/bin/zstd"),
        make_path_entry("awk", "/bin/awk"),
        make_path_entry("git", "/bin/git"),
    };

    path_binaries_merge_entries_test_hook(NULL, chunk, 3, TRUE);

    PathEntry out[MAX_PATH_BINS];
    int out_count = 0;
    path_binaries_filter("", out, &out_count);

    ASSERT_EQ_INT("alphabetical empty query count", 3, out_count);
    ASSERT_STR_EQ("alphabetical empty query first", "awk", out[0].name);
    ASSERT_STR_EQ("alphabetical empty query second", "git", out[1].name);
    ASSERT_STR_EQ("alphabetical empty query third", "zstd", out[2].name);
}

static void test_chunk_merge_atomicity_unique_count(void) {
    path_binaries_reset_for_tests();

    PathEntry chunk1[] = {
        make_path_entry("ls", "/bin/ls"),
        make_path_entry("cp", "/bin/cp"),
    };
    PathEntry chunk2[] = {
        make_path_entry("ls", "/usr/bin/ls"),
        make_path_entry("mv", "/bin/mv"),
    };

    path_binaries_merge_entries_test_hook(NULL, chunk1, 2, FALSE);
    path_binaries_merge_entries_test_hook(NULL, chunk2, 2, TRUE);

    PathEntry out[MAX_PATH_BINS];
    int out_count = 0;
    path_binaries_filter("", out, &out_count);

    ASSERT_EQ_INT("chunk merge unique count", 3, out_count);
}

static void test_monitor_delete_updates_cache(void) {
    path_binaries_reset_for_tests();

    PathEntry chunk[] = {
        make_path_entry("foo", "/tmp/pathbin-delete/foo"),
    };
    path_binaries_merge_entries_test_hook(NULL, chunk, 1, TRUE);

    GFile *file = g_file_new_for_path("/tmp/pathbin-delete/foo");
    path_binaries_on_monitor_event_test_hook(file, NULL, G_FILE_MONITOR_EVENT_DELETED);
    g_object_unref(file);

    PathEntry out[MAX_PATH_BINS];
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

    PathEntry out[MAX_PATH_BINS];
    int out_count = 0;
    path_binaries_filter("", out, &out_count);

    ASSERT_EQ_INT("monitor create adds entry", 1, out_count);
    ASSERT_STR_EQ("monitor create entry name", "newbin", out[0].name);

    g_remove(file_path);
    g_rmdir(tmp_dir);
    g_free(file_path);
    g_free(tmp_dir);
}

static void test_monitor_rename_updates_cache(void) {
    path_binaries_reset_for_tests();

    gchar *tmp_dir = g_dir_make_tmp("cofi-path-bins-rename-XXXXXX", NULL);
    ASSERT_TRUE("rename tmp dir created", tmp_dir != NULL);
    if (!tmp_dir) {
        return;
    }

    gchar *old_path = g_build_filename(tmp_dir, "oldbin", NULL);
    gchar *new_path = g_build_filename(tmp_dir, "newbin", NULL);
    const char script[] = "#!/bin/sh\nexit 0\n";
    g_file_set_contents(new_path, script, -1, NULL);
    g_chmod(new_path, 0755);

    PathEntry chunk[] = {
        make_path_entry("oldbin", old_path),
    };
    path_binaries_merge_entries_test_hook(NULL, chunk, 1, TRUE);

    GFile *old_file = g_file_new_for_path(old_path);
    GFile *new_file = g_file_new_for_path(new_path);
    path_binaries_on_monitor_event_test_hook(old_file, new_file, G_FILE_MONITOR_EVENT_RENAMED);
    g_object_unref(old_file);
    g_object_unref(new_file);

    PathEntry out[MAX_PATH_BINS];
    int out_count = 0;
    path_binaries_filter("", out, &out_count);

    ASSERT_EQ_INT("monitor rename keeps one entry", 1, out_count);
    ASSERT_STR_EQ("monitor rename updates basename", "newbin", out[0].name);
    ASSERT_STR_EQ("monitor rename updates exec path", new_path, out[0].exec_path);

    g_remove(old_path);
    g_remove(new_path);
    g_rmdir(tmp_dir);
    g_free(old_path);
    g_free(new_path);
    g_free(tmp_dir);
}

static void test_global_cap_overflow_sets_warned(void) {
    path_binaries_reset_for_tests();

    int total = MAX_PATH_BINS + 10;
    PathEntry *entries = calloc((size_t)total, sizeof(PathEntry));
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

    PathEntry out[MAX_PATH_BINS + 16];
    int out_count = 0;
    path_binaries_filter("", out, &out_count);

    ASSERT_EQ_INT("cache fills to MAX_PATH_BINS", MAX_PATH_BINS, path_binaries_count_for_tests());
    ASSERT_EQ_INT("cap overflow clamps count to MAX_APPS", MAX_APPS, out_count);
    ASSERT_TRUE("cap overflow sets warned", path_binaries_cap_warned_for_tests());

    free(entries);
}

static void test_tig_ranked_above_loose_matches(void) {
    path_binaries_reset_for_tests();

    PathEntry chunk[] = {
        make_path_entry("activate-global-python-argcomplete",
                        "/usr/bin/activate-global-python-argcomplete"),
        make_path_entry("apt-config", "/usr/bin/apt-config"),
        make_path_entry("aptitude-changelog-parser",
                        "/usr/bin/aptitude-changelog-parser"),
        make_path_entry("ayatana-settings", "/usr/bin/ayatana-settings"),
        make_path_entry("btrfs-image", "/usr/bin/btrfs-image"),
        make_path_entry("caja-actions-config-tool",
                        "/usr/bin/caja-actions-config-tool"),
        make_path_entry("create-ocs-tmp-img", "/usr/bin/create-ocs-tmp-img"),
        make_path_entry("dh_auto_configure", "/usr/bin/dh_auto_configure"),
        make_path_entry("tig", "/usr/bin/tig"),
        make_path_entry("tigris", "/usr/bin/tigris"),
    };
    int count = (int)(sizeof(chunk) / sizeof(chunk[0]));

    path_binaries_merge_entries_test_hook(NULL, chunk, count, TRUE);

    PathEntry out[MAX_PATH_BINS];
    int out_count = 0;
    path_binaries_filter("tig", out, &out_count);

    ASSERT_TRUE("tig query returns at least one result", out_count >= 1);
    if (out_count < 1) return;

    ASSERT_STR_EQ("tig is first result", "tig", out[0].name);
}

static void test_cap_warning_emits_once(void) {
    path_binaries_reset_for_tests();

    int total = MAX_PATH_BINS + 10;
    PathEntry *entries = calloc((size_t)total, sizeof(PathEntry));
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

static void test_large_match_set_prefers_high_score(void) {
    path_binaries_reset_for_tests();

    int total = 600;
    PathEntry *entries = calloc((size_t)total, sizeof(PathEntry));
    ASSERT_TRUE("large match set alloc", entries != NULL);
    if (!entries) return;

    for (int i = 0; i < total - 1; i++) {
        char name[64];
        char exec_path[128];
        snprintf(name, sizeof(name), "a_match_%04d_e", i);
        snprintf(exec_path, sizeof(exec_path), "/usr/bin/a_match_%04d_e", i);
        entries[i] = make_path_entry(name, exec_path);
    }
    entries[total - 1] = make_path_entry("e", "/usr/bin/e");

    path_binaries_merge_entries_test_hook(NULL, entries, total, TRUE);
    free(entries);

    PathEntry out[MAX_PATH_BINS + 16];
    int out_count = 0;
    path_binaries_filter("e", out, &out_count);

    ASSERT_EQ_INT("large match set returns MAX_APPS", MAX_APPS, out_count);
    ASSERT_STR_EQ("exact match 'e' tops the sorted list", "e", out[0].name);
}

int main(void) {
    test_dedupe_first_in_path_wins();
    test_filter_by_query();
    test_empty_query_returns_all();
    test_empty_query_is_alphabetical();
    test_chunk_merge_atomicity_unique_count();
    test_monitor_delete_updates_cache();
    test_monitor_create_updates_cache();
    test_monitor_rename_updates_cache();
    test_global_cap_overflow_sets_warned();
    test_cap_warning_emits_once();
    test_tig_ranked_above_loose_matches();
    test_large_match_set_prefers_high_score();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
