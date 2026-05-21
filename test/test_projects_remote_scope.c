#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/projects_parse.h"
#include "../src/projects_remote_scope.h"

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

void projects_refresh(AppData *app) { (void)app; }
void reset_selection(AppData *app) { (void)app; }
void update_scroll_position(AppData *app) { (void)app; }
void update_display(AppData *app) { (void)app; }

static gboolean failing_exec_stdout_reason(const char *host,
                                           const char *const *remote_argv,
                                           gchar **stdout_out,
                                           gchar **stderr_out) {
    (void)host;
    (void)remote_argv;
    if (stdout_out) *stdout_out = g_strdup("rejected: only git push/fetch allowed\n");
    if (stderr_out) *stderr_out = g_strdup("");
    return FALSE;
}

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s (line %d)\n", name, __LINE__); fail++; } \
} while (0)

static void test_remote_output_parsing_builds_scoped_rows(void) {
    projects_remote_scope_reset_for_test();

    char err[256];
    gboolean ok = projects_remote_scope_load_from_outputs_for_test(
        "tsunami",
        "local\t2\t1\n",
        "remote-zj\n",
        "/srv/work\n",
        err,
        sizeof(err));
    ASSERT_TRUE("parse remote outputs succeeds", ok == TRUE);

    ProjectsMode mode;
    init_projects_mode(&mode);
    ASSERT_TRUE("remote scope applies to mode", projects_remote_scope_apply(&mode) == TRUE);
    ASSERT_TRUE("remote mode contains parsed sessions",
                mode.session_count == 2 &&
                mode.projects[0].is_saved_remote &&
                strcmp(mode.projects[0].remote_host, "tsunami") == 0 &&
                strcmp(mode.projects[0].name, "local") == 0);
    ASSERT_TRUE("remote mode contains parsed folders",
                mode.folder_count == 1 &&
                mode.folders[0].is_remote &&
                strcmp(mode.folders[0].remote_host, "tsunami") == 0);
    projects_clear_folders(mode.folders, mode.folder_count);
}

static void test_scope_toggle_local_remote_local(void) {
    projects_remote_scope_reset_for_test();
    projects_remote_scope_set_loading_for_test("edge", FALSE);
    projects_remote_scope_set_active_for_test(FALSE);

    ProjectsMode mode;
    init_projects_mode(&mode);
    ASSERT_TRUE("apply returns false in local mode",
                projects_remote_scope_apply(&mode) == FALSE);

    char err[256];
    ASSERT_TRUE("load test scope data",
                projects_remote_scope_load_from_outputs_for_test(
                    "edge", "a\t1\t0\n", "", "", err, sizeof(err)) == TRUE);
    ASSERT_TRUE("active host set", strcmp(projects_remote_scope_current_host(), "edge") == 0);
    ASSERT_TRUE("active mode applies", projects_remote_scope_apply(&mode) == TRUE);
    projects_clear_folders(mode.folders, mode.folder_count);

    projects_remote_scope_clear();
    ASSERT_TRUE("after clear scope inactive", projects_remote_scope_is_active() == FALSE);
    ASSERT_TRUE("after clear apply returns false",
                projects_remote_scope_apply(&mode) == FALSE);
}

static void test_failure_and_loading_state_return_local_or_loading_message(void) {
    projects_remote_scope_reset_for_test();

    char err[256];
    gboolean ok = projects_remote_scope_load_from_outputs_for_test(
        "voidhost", "", "", "", err, sizeof(err));
    ASSERT_TRUE("empty remote fetch result fails", ok == FALSE);
    ASSERT_TRUE("failure does not activate scope", projects_remote_scope_is_active() == FALSE);

    ProjectsMode mode;
    init_projects_mode(&mode);
    projects_remote_scope_set_loading_for_test("voidhost", TRUE);
    ASSERT_TRUE("loading state applies", projects_remote_scope_apply(&mode) == TRUE);
    ASSERT_TRUE("loading state shows message",
                strstr(mode.last_error, "Loading remote projects for voidhost") != NULL);
    projects_remote_scope_set_loading_for_test("voidhost", FALSE);
}

static void test_failure_reason_uses_stdout_and_sets_status_line(void) {
    projects_remote_scope_reset_for_test();
    projects_remote_scope_set_exec_for_test(failing_exec_stdout_reason);

    char err[256];
    gboolean ok = projects_remote_scope_fetch_sync_for_test("tsunami", err, sizeof(err));
    ASSERT_TRUE("sync fetch fails when ssh commands fail", ok == FALSE);
    ASSERT_TRUE("failure reason includes stdout rejection text",
                strstr(err, "rejected: only git push/fetch allowed") != NULL);
    ASSERT_TRUE("status line is set from failure reason",
                strstr(projects_remote_scope_status_message(),
                       "Remote fetch failed for 'tsunami': rejected: only git push/fetch allowed") != NULL);

    projects_remote_scope_clear_status_message();
    ASSERT_TRUE("status line clears explicitly",
                projects_remote_scope_status_message()[0] == '\0');
}

static void test_ssh_argv_builder_keeps_host_and_appends_remote_command(void) {
    const char *remote_argv[] = {
        "tmux", "list-sessions", "-F", "#{session_name}\t#{session_windows}\t#{session_attached}", NULL
    };
    gchar *argv[16] = {0};
    int argc = projects_remote_scope_build_ssh_argv_for_test("tsunami", remote_argv, argv, 16);

    ASSERT_TRUE("ssh argv has expected argc", argc == 10);
    ASSERT_TRUE("ssh argv starts with ssh", strcmp(argv[0], "ssh") == 0);
    ASSERT_TRUE("ssh argv keeps host at slot 5", strcmp(argv[5], "tsunami") == 0);
    ASSERT_TRUE("ssh argv command starts after host", strcmp(argv[6], "tmux") == 0);
    ASSERT_TRUE("ssh argv includes subsequent command words", strcmp(argv[7], "list-sessions") == 0);
    ASSERT_TRUE("ssh argv is null terminated", argv[argc] == NULL);
}

int main(void) {
    printf("projects remote scope tests\n");
    printf("===========================\n\n");

    test_remote_output_parsing_builds_scoped_rows();
    test_scope_toggle_local_remote_local();
    test_failure_and_loading_state_return_local_or_loading_message();
    test_failure_reason_uses_stdout_and_sets_status_line();
    test_ssh_argv_builder_keeps_host_and_appends_remote_command();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
