#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../src/projects_remote_store.h"

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s (line %d)\n", name, __LINE__); fail++; } \
} while (0)

static void reset_store_with_path(const char *path) {
    projects_remote_store_reset_for_test();
    projects_remote_store_set_path_for_test(path);
}

static void test_projects_remote_store_roundtrip(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/cofi-projects-remote-%ld.json", (long)getpid());
    unlink(path);
    reset_store_with_path(path);

    ASSERT_TRUE("add tmux remote entry",
                projects_remote_store_add_for_test("tsunami",
                                                   PROJECT_BACKEND_TMUX,
                                                   "work_api",
                                                   "/home/dl/Projects/cofi"));
    ASSERT_TRUE("add zellij remote entry",
                projects_remote_store_add_for_test("atlas",
                                                   PROJECT_BACKEND_ZELLIJ,
                                                   "ops",
                                                   ""));
    ASSERT_TRUE("save remote store", projects_remote_store_save_for_test());

    projects_remote_store_reset_for_test();
    projects_remote_store_set_path_for_test(path);
    ASSERT_TRUE("reload remote store", projects_remote_store_reload());
    ASSERT_TRUE("reload count is 2", projects_remote_store_count() == 2);

    const ProjectRemoteEntry *first = projects_remote_store_entry_at(0);
    const ProjectRemoteEntry *second = projects_remote_store_entry_at(1);
    ASSERT_TRUE("first entry preserved host/tool/name/cwd",
                first &&
                strcmp(first->host, "tsunami") == 0 &&
                first->backend == PROJECT_BACKEND_TMUX &&
                strcmp(first->name, "work_api") == 0 &&
                strcmp(first->cwd, "/home/dl/Projects/cofi") == 0);
    ASSERT_TRUE("second entry preserved host/tool/name",
                second &&
                strcmp(second->host, "atlas") == 0 &&
                second->backend == PROJECT_BACKEND_ZELLIJ &&
                strcmp(second->name, "ops") == 0);

    unlink(path);
    projects_remote_store_reset_for_test();
}

static void test_projects_remote_store_append_sessions_merges_remote_entries(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/cofi-projects-remote-merge-%ld.json", (long)getpid());
    unlink(path);
    reset_store_with_path(path);

    ASSERT_TRUE("add merge entry",
                projects_remote_store_add_for_test("edge",
                                                   PROJECT_BACKEND_TMUX,
                                                   "dev",
                                                   "/srv/dev"));
    ASSERT_TRUE("save merge entry", projects_remote_store_save_for_test());
    ASSERT_TRUE("reload merge entry", projects_remote_store_reload());

    ProjectSessionEntry sessions[4];
    memset(sessions, 0, sizeof(sessions));
    int count = projects_remote_store_append_sessions(sessions, 0, 4);
    ASSERT_TRUE("append count includes saved remote", count == 1);
    ASSERT_TRUE("merged entry marked as saved remote",
                sessions[0].is_saved_remote == TRUE);
    ASSERT_TRUE("merged entry carries host/name/cwd/backend",
                strcmp(sessions[0].remote_host, "edge") == 0 &&
                strcmp(sessions[0].name, "dev") == 0 &&
                strcmp(sessions[0].remote_cwd, "/srv/dev") == 0 &&
                sessions[0].backend == PROJECT_BACKEND_TMUX);

    unlink(path);
    projects_remote_store_reset_for_test();
}

static void test_projects_remote_store_forget_removes_and_persists(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/cofi-projects-remote-forget-%ld.json", (long)getpid());
    unlink(path);
    reset_store_with_path(path);

    ASSERT_TRUE("add keep entry",
                projects_remote_store_add_for_test("keep-host",
                                                   PROJECT_BACKEND_TMUX,
                                                   "keep",
                                                   ""));
    ASSERT_TRUE("add delete entry",
                projects_remote_store_add_for_test("drop-host",
                                                   PROJECT_BACKEND_ZELLIJ,
                                                   "drop",
                                                   "/tmp/drop"));
    ASSERT_TRUE("save before forget", projects_remote_store_save_for_test());
    ASSERT_TRUE("forget saved entry",
                projects_remote_store_forget("drop-host",
                                             PROJECT_BACKEND_ZELLIJ,
                                             "drop",
                                             "/tmp/drop"));

    projects_remote_store_reset_for_test();
    projects_remote_store_set_path_for_test(path);
    ASSERT_TRUE("reload after forget", projects_remote_store_reload());
    ASSERT_TRUE("count is 1 after forget", projects_remote_store_count() == 1);
    const ProjectRemoteEntry *remaining = projects_remote_store_entry_at(0);
    ASSERT_TRUE("remaining entry is keep-host/keep",
                remaining &&
                strcmp(remaining->host, "keep-host") == 0 &&
                strcmp(remaining->name, "keep") == 0);

    unlink(path);
    projects_remote_store_reset_for_test();
}

static void test_projects_remote_store_save_intent_upserts_entry(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/cofi-projects-remote-intent-%ld.json", (long)getpid());
    unlink(path);
    reset_store_with_path(path);

    ASSERT_TRUE("save new remote intent",
                projects_remote_store_save_intent("tsunami",
                                                  PROJECT_BACKEND_TMUX,
                                                  "work",
                                                  "/srv/work"));
    ASSERT_TRUE("save existing remote intent updates cwd",
                projects_remote_store_save_intent("tsunami",
                                                  PROJECT_BACKEND_TMUX,
                                                  "work",
                                                  "/srv/work2"));

    projects_remote_store_reset_for_test();
    projects_remote_store_set_path_for_test(path);
    ASSERT_TRUE("reload after save_intent", projects_remote_store_reload());
    ASSERT_TRUE("intent upsert keeps one entry", projects_remote_store_count() == 1);
    const ProjectRemoteEntry *entry = projects_remote_store_entry_at(0);
    ASSERT_TRUE("intent upsert persisted updated cwd",
                entry &&
                strcmp(entry->host, "tsunami") == 0 &&
                strcmp(entry->name, "work") == 0 &&
                strcmp(entry->cwd, "/srv/work2") == 0);

    unlink(path);
    projects_remote_store_reset_for_test();
}

int main(void) {
    printf("projects remote store tests\n");
    printf("===========================\n\n");

    test_projects_remote_store_roundtrip();
    test_projects_remote_store_append_sessions_merges_remote_entries();
    test_projects_remote_store_forget_removes_and_persists();
    test_projects_remote_store_save_intent_upserts_entry();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
