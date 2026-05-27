#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "projects/projects.h"
#include "projects/projects_commands.h"
#include "projects/projects_exec.h"
#include "projects/projects_folder_windows.h"
#include "projects/projects_parse.h"
#include "projects/projects_remote_windows.h"
#include "projects/projects_tmux_windows.h"
#include "projects/projects_window_env.h"
#include "projects/projects_zellij_windows.h"
#include "core/log/log.h"

void activate_window(Display *display, Window window_id) {
    (void)display;
    (void)window_id;
}

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

void get_window_list(AppData *app) {
    (void)app;
}

gboolean process_find_window_for_pid_ancestry(AppData *app,
                                              pid_t pid,
                                              int max_depth,
                                              Window *window_out) {
    (void)app;
    (void)pid;
    (void)max_depth;
    if (window_out) *window_out = 0;
    return FALSE;
}

#include "projects/projects_window_env.c"
#include "projects/projects_remote_windows.c"
#include "projects/projects_tmux_windows.c"
#include "projects/projects_zellij_windows.c"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s (line %d)\n", name, __LINE__); fail++; } \
} while (0)

#define ASSERT_EQ_INT(name, expected, actual) \
    ASSERT_TRUE(name, (expected) == (actual))

#define ASSERT_EQ_ULONG(name, expected, actual) \
    ASSERT_TRUE(name, (unsigned long)(expected) == (unsigned long)(actual))

#define ASSERT_STR_EQ(name, expected, actual) \
    ASSERT_TRUE(name, strcmp((expected), (actual)) == 0)

static void test_parse_formatted_projects(void) {
    const char *output =
        "cofi\t2\t1\n"
        "linear work\t1\t0\n"
        "scrcpy:debug\t3\t2\n";
    ProjectSessionEntry projects[MAX_PROJECTS];
    char error[256];

    int count = projects_parse_tmux_list(output, projects, MAX_PROJECTS,
                                         error, sizeof(error));

    ASSERT_EQ_INT("parse count", 3, count);
    ASSERT_STR_EQ("first session name", "cofi", projects[0].name);
    ASSERT_EQ_INT("first windows", 2, projects[0].windows);
    ASSERT_EQ_INT("first attached", 1, projects[0].attached);
    ASSERT_STR_EQ("space in session name preserved", "linear work", projects[1].name);
    ASSERT_STR_EQ("colon in session name preserved", "scrcpy:debug", projects[2].name);
    ASSERT_STR_EQ("no parse error", "", error);
}

static void test_empty_output_reports_no_projects(void) {
    ProjectSessionEntry projects[MAX_PROJECTS];
    char error[256];

    int count = projects_parse_tmux_list("", projects, MAX_PROJECTS,
                                         error, sizeof(error));

    ASSERT_EQ_INT("empty output count", 0, count);
    ASSERT_STR_EQ("empty output message", "No tmux sessions", error);
}

static void test_malformed_lines_are_ignored(void) {
    const char *output =
        "not enough fields\n"
        "bad\tNaN\t0\n"
        "valid\t4\t0\n";
    ProjectSessionEntry projects[MAX_PROJECTS];
    char error[256];

    int count = projects_parse_tmux_list(output, projects, MAX_PROJECTS,
                                         error, sizeof(error));

    ASSERT_EQ_INT("malformed lines skipped", 1, count);
    ASSERT_STR_EQ("valid line parsed", "valid", projects[0].name);
    ASSERT_EQ_INT("valid windows parsed", 4, projects[0].windows);
    ASSERT_STR_EQ("malformed skipped without error when valid rows exist", "", error);
}

static void test_attach_command_uses_exact_shell_quoted_target(void) {
    gchar *cmd = projects_build_tmux_attach_command("tmux", "work:api session");

    ASSERT_STR_EQ("attach command exact target quoted",
                  "'tmux' attach-session -t '=work:api session'", cmd);
    g_free(cmd);
}

static void test_parse_zoxide_folders(void) {
    const char *output =
        "/home/user/Projects/cofi\n"
        "/home/user/Projects/codex-msgnr\n";
    ProjectFolder folders[MAX_PROJECT_FOLDERS];
    char error[256];

    int count = projects_parse_zoxide_list(output, folders, MAX_PROJECT_FOLDERS,
                                           error, sizeof(error));

    ASSERT_EQ_INT("zoxide folder count", 2, count);
    ASSERT_STR_EQ("first zoxide path", "/home/user/Projects/cofi", folders[0].path);
    ASSERT_STR_EQ("first zoxide label", "cofi", folders[0].label);
    ASSERT_STR_EQ("second zoxide label", "codex-msgnr", folders[1].label);
    ASSERT_STR_EQ("zoxide parse no error", "", error);
    projects_clear_folders(folders, count);
}

static void test_folder_session_command_uses_start_directory(void) {
    gchar *session_name = projects_build_folder_session_name("/home/user/Projects/cofi");
    gchar *cmd = projects_build_tmux_new_command("tmux", session_name, "/home/user/Projects/cofi");

    ASSERT_STR_EQ("folder command creates or attaches in directory",
                  "'tmux' new-session -A -s 'cofi' -c '/home/user/Projects/cofi'", cmd);
    g_free(cmd);
    g_free(session_name);
}

static void test_folder_session_command_quotes_path_and_sanitizes_name(void) {
    gchar *session_name = projects_build_folder_session_name("/tmp/work:api session");
    gchar *cmd = projects_build_tmux_new_command("tmux", session_name, "/tmp/work:api session");

    ASSERT_STR_EQ("folder command quotes path and sanitizes session name",
                  "'tmux' new-session -A -s 'work_api_session' -c '/tmp/work:api session'", cmd);
    g_free(cmd);
    g_free(session_name);
}

static void test_folder_session_name_replaces_tmux_separators(void) {
    gchar *session_name = projects_build_folder_session_name("/tmp/my.project");
    gchar *cmd = projects_build_tmux_new_command("tmux", session_name, "/tmp/my.project");

    ASSERT_STR_EQ("folder command replaces dot in session name",
                  "'tmux' new-session -A -s 'my_project' -c '/tmp/my.project'", cmd);
    g_free(cmd);
    g_free(session_name);
}

static void test_kill_command_uses_exact_target(void) {
    gchar *cmd = projects_build_tmux_kill_command("tmux", "work:api session");

    ASSERT_STR_EQ("kill command exact target quoted",
                  "'tmux' kill-session -t '=work:api session'", cmd);
    g_free(cmd);
}

static void test_rename_command_quotes_old_and_new_names(void) {
    gchar *cmd = projects_build_tmux_rename_command("tmux", "work:api session", "renamed session");

    ASSERT_STR_EQ("rename command quotes exact target and new name",
                  "'tmux' rename-session -t '=work:api session' 'renamed session'", cmd);
    g_free(cmd);
}

static void test_new_session_command_uses_home_directory(void) {
    gchar *cmd = projects_build_tmux_new_command("tmux", "scratch", "/home/user");

    ASSERT_STR_EQ("new session command starts in home",
                  "'tmux' new-session -A -s 'scratch' -c '/home/user'", cmd);
    g_free(cmd);
}

static void test_terminal_title_prefix_wraps_command_with_safe_quoted_title(void) {
    gchar *wrapped = projects_with_terminal_title(
        "'tmux' attach-session -t '=work:api session'",
        "dev'session;$(rm -rf /)");

    ASSERT_STR_EQ("terminal title wrapper uses literal printf format and shell-quoted title",
                  "printf '\\033]2;%s\\007' 'dev'\\''session;$(rm -rf /)'; "
                  "'tmux' attach-session -t '=work:api session'",
                  wrapped);
    g_free(wrapped);
}

static void test_remote_attach_command_builds_ssh_t_path(void) {
    gchar *cmd = projects_build_remote_attach_command(
        PROJECT_BACKEND_TMUX, "tsunami", "tmux", "work api");
    ASSERT_STR_EQ("remote tmux attach uses ssh X forwarding and tty",
                  "ssh -X -t 'tsunami' 'tmux' attach-session -t 'work api'", cmd);
    g_free(cmd);
}

static void test_remote_new_command_builds_ssh_t_with_cwd(void) {
    gchar *cmd = projects_build_remote_new_command(
        PROJECT_BACKEND_TMUX, "tsunami", "tmux", "work api", "/srv/work");
    ASSERT_STR_EQ("remote tmux new uses ssh X forwarding and cwd",
                  "ssh -X -t 'tsunami' 'tmux' new -A -s 'work api' -c '/srv/work'", cmd);
    g_free(cmd);
}

static void test_folder_terminal_command_cd_and_exec_shell(void) {
    gchar *cmd = projects_build_folder_terminal_command("/home/user/work dir");
    ASSERT_STR_EQ("folder terminal command cds and execs login shell",
                  "cd '/home/user/work dir' && exec \"${SHELL:-bash}\" -l", cmd);
    g_free(cmd);
}

static void test_remote_folder_terminal_command_ssh_cd_and_exec_shell(void) {
    gchar *cmd = projects_build_remote_folder_terminal_command("root@tsunami",
                                                               "/srv/work dir");
    ASSERT_STR_EQ("remote folder terminal command uses ssh X forwarding and tty",
                  "ssh -X -t 'root@tsunami' 'cd '\\''/srv/work dir'\\'' && exec \"${SHELL:-bash}\" -l'",
                  cmd);
    g_free(cmd);
}

static void test_remote_cmdline_matcher_matches_tmux_attach(void) {
    const char cmdline[] =
        "ssh\0-X\0-t\0root@tsunami\0tmux\0attach-session\0-t\0work api\0";

    ASSERT_TRUE("remote matcher matches tmux attach-session",
                projects_remote_cmdline_matches_attach(cmdline,
                                                       sizeof(cmdline),
                                                       "root@tsunami",
                                                       "tmux",
                                                       "work api"));
}

static void test_remote_cmdline_matcher_matches_zellij_attach_create(void) {
    const char cmdline[] =
        "ssh\0-t\0root@tsunami\0zellij\0attach\0--create\0work api\0";

    ASSERT_TRUE("remote matcher matches zellij attach --create",
                projects_remote_cmdline_matches_attach(cmdline,
                                                       sizeof(cmdline),
                                                       "root@tsunami",
                                                       "zellij",
                                                       "work api"));
}

static void test_remote_cmdline_matcher_rejects_wrong_host_or_session(void) {
    const char cmdline[] =
        "ssh\0-t\0root@tsunami\0tmux\0attach-session\0-t\0work api\0";

    ASSERT_TRUE("remote matcher rejects wrong host",
                !projects_remote_cmdline_matches_attach(cmdline,
                                                        sizeof(cmdline),
                                                        "root@other",
                                                        "tmux",
                                                        "work api"));
    ASSERT_TRUE("remote matcher rejects wrong session",
                !projects_remote_cmdline_matches_attach(cmdline,
                                                        sizeof(cmdline),
                                                        "root@tsunami",
                                                        "tmux",
                                                        "other"));
}

static void test_remote_cmdline_matcher_rejects_non_remote_attach(void) {
    const char local_zellij[] = "zellij\0attach\0--create\0work api\0";
    const char plain_ssh_shell[] = "ssh\0-t\0root@tsunami\0";

    ASSERT_TRUE("remote matcher rejects local zellij client process",
                !projects_remote_cmdline_matches_attach(local_zellij,
                                                        sizeof(local_zellij),
                                                        "root@tsunami",
                                                        "zellij",
                                                        "work api"));
    ASSERT_TRUE("remote matcher rejects plain ssh shell",
                !projects_remote_cmdline_matches_attach(plain_ssh_shell,
                                                        sizeof(plain_ssh_shell),
                                                        "root@tsunami",
                                                        "zellij",
                                                        "work api"));
}

static void test_parse_zellij_projects(void) {
    ProjectSessionEntry projects[4];
    char error[128];
    int count = projects_parse_zellij_list(
        "home\nwork api\ncofi\n", projects, 4, error, sizeof(error));

    ASSERT_EQ_INT("zellij parse count", 3, count);
    ASSERT_STR_EQ("zellij first session name", "home", projects[0].name);
    ASSERT_STR_EQ("zellij preserves spaces", "work api", projects[1].name);
    ASSERT_EQ_INT("zellij backend marker", PROJECT_BACKEND_ZELLIJ, projects[0].backend);
    ASSERT_STR_EQ("zellij parse no error", "", error);
}

static void test_zellij_attach_command_quotes_name(void) {
    gchar *cmd = projects_build_zellij_attach_command("zellij", "work api");

    ASSERT_STR_EQ("zellij attach command creates missing session",
                  "'zellij' attach --create 'work api'", cmd);
    g_free(cmd);
}

static void test_zellij_kill_command_quotes_name(void) {
    gchar *cmd = projects_build_zellij_kill_command("zellij", "work api");

    ASSERT_STR_EQ("zellij kill command quotes name",
                  "'zellij' kill-session 'work api'", cmd);
    g_free(cmd);
}

static void test_zellij_new_command_starts_in_directory(void) {
    gchar *cmd = projects_build_zellij_new_command("zellij", "work api", "/tmp/work api");

    ASSERT_STR_EQ("zellij new command changes directory before attach",
                  "cd '/tmp/work api' && 'zellij' attach --create 'work api'", cmd);
    g_free(cmd);
}

static void test_project_tool_resolver_prefers_configured_path(void) {
    CofiConfig config;
    memset(&config, 0, sizeof(config));
    g_strlcpy(config.projects_tmux_path, "/bin/sh", sizeof(config.projects_tmux_path));

    gchar *resolved = projects_resolve_tool(&config, PROJECT_TOOL_TMUX, NULL, 0);

    ASSERT_STR_EQ("configured tmux path resolved", "/bin/sh", resolved);
    g_free(resolved);
}

static void test_project_tool_resolver_uses_path_when_empty(void) {
    char dir_template[] = "/tmp/cofi-project-tool-XXXXXX";
    char *dir = mkdtemp(dir_template);
    ASSERT_TRUE("resolver temp dir created", dir != NULL);
    if (!dir) return;

    gchar *tool_path = g_build_filename(dir, "tmux", NULL);
    FILE *file = fopen(tool_path, "w");
    ASSERT_TRUE("resolver fake tool created", file != NULL);
    if (file) {
        fputs("#!/bin/sh\nexit 0\n", file);
        fclose(file);
        chmod(tool_path, 0755);
    }

    const char *old_path = g_getenv("PATH");
    gchar *saved_path = old_path ? g_strdup(old_path) : NULL;
    g_setenv("PATH", dir, TRUE);

    CofiConfig config;
    memset(&config, 0, sizeof(config));
    gchar *resolved = projects_resolve_tool(&config, PROJECT_TOOL_TMUX, NULL, 0);

    ASSERT_STR_EQ("empty tmux path uses PATH", tool_path, resolved);

    if (saved_path) g_setenv("PATH", saved_path, TRUE);
    else g_unsetenv("PATH");
    g_free(saved_path);
    g_free(resolved);
    unlink(tool_path);
    g_free(tool_path);
    rmdir(dir);
}

static void test_tmux_client_pid_parser(void) {
    pid_t pids[4];
    int count = projects_parse_tmux_client_pids("123\nbad\n1\n456\n", pids, 4);

    ASSERT_EQ_INT("tmux client pid count", 2, count);
    ASSERT_EQ_INT("tmux first client pid", 123, (int)pids[0]);
    ASSERT_EQ_INT("tmux second client pid", 456, (int)pids[1]);
}

static void test_tmux_client_pid_parser_ignores_empty_output(void) {
    pid_t pids[2] = {99, 88};
    int count = projects_parse_tmux_client_pids("\n\n", pids, 2);

    ASSERT_EQ_INT("tmux empty client pid count", 0, count);
    ASSERT_EQ_INT("tmux empty leaves first pid untouched", 99, (int)pids[0]);
}

static void test_zellij_cmdline_matches_short_attach(void) {
    const char cmdline[] = "/usr/bin/zellij\0a\0coiner-dev\0";

    ASSERT_TRUE("zellij short attach cmdline matches session",
                projects_zellij_cmdline_matches_session(cmdline, sizeof(cmdline),
                                                        "zellij",
                                                        "coiner-dev"));
}

static void test_zellij_cmdline_matches_attach_create(void) {
    const char cmdline[] = "zellij\0attach\0--create\0work api\0";

    ASSERT_TRUE("zellij attach create cmdline matches session",
                projects_zellij_cmdline_matches_session(cmdline, sizeof(cmdline),
                                                        "zellij",
                                                        "work api"));
}

static void test_zellij_cmdline_matches_session_option(void) {
    const char cmdline[] = "zellij\0--session\0coiner-dev\0";

    ASSERT_TRUE("zellij session option cmdline matches session",
                projects_zellij_cmdline_matches_session(cmdline, sizeof(cmdline),
                                                        "zellij",
                                                        "coiner-dev"));
}

static void test_zellij_cmdline_rejects_server_and_other_session(void) {
    const char server[] = "zellij\0--server\0/run/user/1000/zellij/coiner-dev\0";
    const char other[] = "zellij\0attach\0--create\0other\0";

    ASSERT_TRUE("zellij server cmdline rejected",
                !projects_zellij_cmdline_matches_session(server, sizeof(server),
                                                         "zellij",
                                                         "coiner-dev"));
    ASSERT_TRUE("zellij other session cmdline rejected",
                !projects_zellij_cmdline_matches_session(other, sizeof(other),
                                                         "zellij",
                                                         "coiner-dev"));
}

static void test_zellij_cmdline_rejects_action_with_session_option(void) {
    const char cmdline[] = "zellij\0--session\0coiner-dev\0action\0list-clients\0";

    ASSERT_TRUE("zellij action cmdline rejected",
                !projects_zellij_cmdline_matches_session(cmdline, sizeof(cmdline),
                                                         "zellij",
                                                         "coiner-dev"));
}

static void test_zellij_cmdline_matches_configured_wrapper_basename(void) {
    const char cmdline[] = "/opt/bin/zellij-wrapper\0attach\0work api\0";

    ASSERT_TRUE("zellij configured wrapper basename matches",
                projects_zellij_cmdline_matches_session(cmdline, sizeof(cmdline),
                                                        "/opt/bin/zellij-wrapper",
                                                        "work api"));
}

static void test_zellij_windowid_from_environ(void) {
    const char environ_data[] = "TERM=xterm\0WINDOWID=64284440\0";
    Window window = 0;

    ASSERT_TRUE("zellij environ windowid parsed",
                projects_windowid_from_environ(environ_data, sizeof(environ_data),
                                               &window));
    ASSERT_EQ_ULONG("zellij environ windowid value", 64284440UL, window);
}

static void test_zellij_windowid_rejects_invalid_environ(void) {
    const char invalid[] = "WINDOWID=not-a-number\0";
    const char zero[] = "WINDOWID=0\0";
    Window window = 123;

    ASSERT_TRUE("zellij invalid windowid rejected",
                !projects_windowid_from_environ(invalid, sizeof(invalid), &window));
    ASSERT_EQ_ULONG("zellij invalid clears windowid", 0UL, window);

    window = 123;
    ASSERT_TRUE("zellij zero windowid rejected",
                !projects_windowid_from_environ(zero, sizeof(zero), &window));
    ASSERT_EQ_ULONG("zellij zero clears windowid", 0UL, window);
}

static void test_match_text_includes_short_backend_markers(void) {
    ProjectSessionEntry tmux_session = {
        .backend = PROJECT_BACKEND_TMUX,
        .windows = 2,
        .attached = 1,
    };
    g_strlcpy(tmux_session.name, "cofi", sizeof(tmux_session.name));

    ProjectSessionEntry zellij_session = {
        .backend = PROJECT_BACKEND_ZELLIJ,
        .windows = -1,
        .attached = -1,
    };
    g_strlcpy(zellij_session.name, "cofi", sizeof(zellij_session.name));

    ProjectFolder folder = {
        .path = "/home/user/Projects/cofi",
        .label = "cofi",
    };

    char text[256];
    projects_format_session_match_text(&tmux_session, text, sizeof(text));
    ASSERT_STR_EQ("tmux match row has [t] marker",
                  "[t] cofi 2 wins 1 client", text);

    projects_format_session_match_text(&zellij_session, text, sizeof(text));
    ASSERT_STR_EQ("zellij match row has [z] marker", "[z] cofi", text);

    projects_format_folder_match_text(&folder, text, sizeof(text));
    ASSERT_STR_EQ("folder match row has [d] marker",
                  "[d] cofi /home/user/Projects/cofi", text);
}

static void test_slot_payloads_are_typed(void) {
    gchar *tmux = projects_build_session_slot_payload(PROJECT_BACKEND_TMUX, "cofi");
    gchar *zellij = projects_build_session_slot_payload(PROJECT_BACKEND_ZELLIJ, "cofi");
    gchar *folder = projects_build_folder_slot_payload("/home/user/Projects/cofi");

    ASSERT_STR_EQ("tmux slot payload includes backend",
                  "session:tmux:cofi", tmux);
    ASSERT_STR_EQ("zellij slot payload includes backend",
                  "session:zellij:cofi", zellij);
    ASSERT_STR_EQ("folder slot payload stores path",
                  "folder:/home/user/Projects/cofi", folder);

    g_free(tmux);
    g_free(zellij);
    g_free(folder);
}

static void test_parse_slot_payloads(void) {
    ProjectSlotTarget target;

    ASSERT_TRUE("parse tmux slot payload",
                projects_parse_slot_payload("session:tmux:work:api", &target));
    ASSERT_EQ_INT("tmux slot kind", PROJECT_SLOT_SESSION, target.kind);
    ASSERT_EQ_INT("tmux slot backend", PROJECT_BACKEND_TMUX, target.backend);
    ASSERT_STR_EQ("tmux slot value preserves colon", "work:api", target.value);

    ASSERT_TRUE("parse zellij slot payload",
                projects_parse_slot_payload("session:zellij:work", &target));
    ASSERT_EQ_INT("zellij slot backend", PROJECT_BACKEND_ZELLIJ, target.backend);
    ASSERT_STR_EQ("zellij slot value", "work", target.value);

    ASSERT_TRUE("parse folder slot payload",
                projects_parse_slot_payload("folder:/home/user/Projects/cofi", &target));
    ASSERT_EQ_INT("folder slot kind", PROJECT_SLOT_FOLDER, target.kind);
    ASSERT_STR_EQ("folder slot value", "/home/user/Projects/cofi", target.value);

    ASSERT_TRUE("reject empty typed payload",
                !projects_parse_slot_payload("session:tmux:", &target));
    ASSERT_TRUE("reject unknown typed payload",
                !projects_parse_slot_payload("tmux:cofi", &target));
}

static WindowInfo test_window(Window id,
                              const char *title,
                              const char *instance,
                              const char *class_name,
                              const char *type) {
    WindowInfo win;
    memset(&win, 0, sizeof(win));
    win.id = id;
    g_strlcpy(win.title, title, sizeof(win.title));
    g_strlcpy(win.instance, instance, sizeof(win.instance));
    g_strlcpy(win.class_name, class_name, sizeof(win.class_name));
    g_strlcpy(win.type, type, sizeof(win.type));
    return win;
}

static void test_caja_folder_window_matches_exact_basename(void) {
    WindowInfo windows[] = {
        test_window(10, "cofi", "mate-terminal", "Mate-terminal", WINDOW_TYPE_NORMAL),
        test_window(11, "cofi", "caja", "Caja", WINDOW_TYPE_NORMAL),
        test_window(12, "cofi-old", "caja", "Caja", WINDOW_TYPE_NORMAL),
    };
    Window found = 0;

    ASSERT_TRUE("find caja folder by exact basename",
                projects_find_caja_folder_window(windows, 3, NULL, 0,
                                                 "/home/user/Projects/cofi", &found));
    ASSERT_EQ_INT("exact basename window selected", 11, (int)found);
}

static void test_caja_folder_window_ignores_desktop_window(void) {
    WindowInfo windows[] = {
        test_window(20, "user", "desktop_window", "Caja", WINDOW_TYPE_NORMAL),
        test_window(21, "user", "caja", "Caja", WINDOW_TYPE_NORMAL),
    };
    Window found = 0;

    ASSERT_TRUE("desktop window ignored for caja folder",
                projects_find_caja_folder_window(windows, 2, NULL, 0,
                                                 "/home/user", &found));
    ASSERT_EQ_INT("normal caja window selected", 21, (int)found);
}

static void test_caja_folder_window_uses_topmost_duplicate(void) {
    WindowInfo windows[] = {
        test_window(30, "user", "caja", "Caja", WINDOW_TYPE_NORMAL),
        test_window(31, "user", "caja", "Caja", WINDOW_TYPE_NORMAL),
    };
    Window stack[] = {31, 30}; /* bottom-to-top */
    Window found = 0;

    ASSERT_TRUE("duplicate caja folders choose topmost",
                projects_find_caja_folder_window(windows, 2, stack, 2,
                                                 "/home/user", &found));
    ASSERT_EQ_INT("topmost duplicate selected", 30, (int)found);
}

static void test_caja_folder_window_rejects_substring_match(void) {
    WindowInfo windows[] = {
        test_window(40, "dl-work", "caja", "Caja", WINDOW_TYPE_NORMAL),
    };
    Window found = 0;

    ASSERT_TRUE("no substring match for caja folder",
                !projects_find_caja_folder_window(windows, 1, NULL, 0,
                                                  "/home/user", &found));
    ASSERT_EQ_INT("no substring window", 0, (int)found);
}

int main(void) {
    printf("projects parser tests\n");
    printf("=====================\n\n");

    test_parse_formatted_projects();
    test_empty_output_reports_no_projects();
    test_malformed_lines_are_ignored();
    test_attach_command_uses_exact_shell_quoted_target();
    test_parse_zoxide_folders();
    test_folder_session_command_uses_start_directory();
    test_folder_session_command_quotes_path_and_sanitizes_name();
    test_folder_session_name_replaces_tmux_separators();
    test_kill_command_uses_exact_target();
    test_rename_command_quotes_old_and_new_names();
    test_new_session_command_uses_home_directory();
    test_terminal_title_prefix_wraps_command_with_safe_quoted_title();
    test_remote_attach_command_builds_ssh_t_path();
    test_remote_new_command_builds_ssh_t_with_cwd();
    test_folder_terminal_command_cd_and_exec_shell();
    test_remote_folder_terminal_command_ssh_cd_and_exec_shell();
    test_remote_cmdline_matcher_matches_tmux_attach();
    test_remote_cmdline_matcher_matches_zellij_attach_create();
    test_remote_cmdline_matcher_rejects_wrong_host_or_session();
    test_remote_cmdline_matcher_rejects_non_remote_attach();
    test_parse_zellij_projects();
    test_zellij_attach_command_quotes_name();
    test_zellij_kill_command_quotes_name();
    test_zellij_new_command_starts_in_directory();
    test_project_tool_resolver_prefers_configured_path();
    test_project_tool_resolver_uses_path_when_empty();
    test_tmux_client_pid_parser();
    test_tmux_client_pid_parser_ignores_empty_output();
    test_zellij_cmdline_matches_short_attach();
    test_zellij_cmdline_matches_attach_create();
    test_zellij_cmdline_matches_session_option();
    test_zellij_cmdline_rejects_server_and_other_session();
    test_zellij_cmdline_rejects_action_with_session_option();
    test_zellij_cmdline_matches_configured_wrapper_basename();
    test_zellij_windowid_from_environ();
    test_zellij_windowid_rejects_invalid_environ();
    test_match_text_includes_short_backend_markers();
    test_slot_payloads_are_typed();
    test_parse_slot_payloads();
    test_caja_folder_window_matches_exact_basename();
    test_caja_folder_window_ignores_desktop_window();
    test_caja_folder_window_uses_topmost_duplicate();
    test_caja_folder_window_rejects_substring_match();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
