#include <glib.h>
#include <stdio.h>
#include <string.h>

#define COFI_TMUX_PARSER_TEST
#include "../src/tmux.c"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s (line %d)\n", name, __LINE__); fail++; } \
} while (0)

#define ASSERT_EQ_INT(name, expected, actual) \
    ASSERT_TRUE(name, (expected) == (actual))

#define ASSERT_STR_EQ(name, expected, actual) \
    ASSERT_TRUE(name, strcmp((expected), (actual)) == 0)

static void test_parse_formatted_sessions(void) {
    const char *output =
        "cofi\t2\t1\n"
        "linear work\t1\t0\n"
        "scrcpy:debug\t3\t2\n";
    TmuxSession sessions[MAX_TMUX_SESSIONS];
    char error[256];

    int count = tmux_parse_session_list_test_hook(output, sessions, MAX_TMUX_SESSIONS,
                                                  error, sizeof(error));

    ASSERT_EQ_INT("parse count", 3, count);
    ASSERT_STR_EQ("first session name", "cofi", sessions[0].name);
    ASSERT_EQ_INT("first windows", 2, sessions[0].windows);
    ASSERT_EQ_INT("first attached", 1, sessions[0].attached);
    ASSERT_STR_EQ("space in session name preserved", "linear work", sessions[1].name);
    ASSERT_STR_EQ("colon in session name preserved", "scrcpy:debug", sessions[2].name);
    ASSERT_STR_EQ("no parse error", "", error);
}

static void test_empty_output_reports_no_sessions(void) {
    TmuxSession sessions[MAX_TMUX_SESSIONS];
    char error[256];

    int count = tmux_parse_session_list_test_hook("", sessions, MAX_TMUX_SESSIONS,
                                                  error, sizeof(error));

    ASSERT_EQ_INT("empty output count", 0, count);
    ASSERT_STR_EQ("empty output message", "No tmux sessions", error);
}

static void test_malformed_lines_are_ignored(void) {
    const char *output =
        "not enough fields\n"
        "bad\tNaN\t0\n"
        "valid\t4\t0\n";
    TmuxSession sessions[MAX_TMUX_SESSIONS];
    char error[256];

    int count = tmux_parse_session_list_test_hook(output, sessions, MAX_TMUX_SESSIONS,
                                                  error, sizeof(error));

    ASSERT_EQ_INT("malformed lines skipped", 1, count);
    ASSERT_STR_EQ("valid line parsed", "valid", sessions[0].name);
    ASSERT_EQ_INT("valid windows parsed", 4, sessions[0].windows);
    ASSERT_STR_EQ("malformed skipped without error when valid rows exist", "", error);
}

static void test_attach_command_uses_exact_shell_quoted_target(void) {
    gchar *cmd = tmux_build_attach_command("work:api session");

    ASSERT_STR_EQ("attach command exact target quoted",
                  "tmux attach-session -t '=work:api session'", cmd);
    g_free(cmd);
}

static void test_parse_zoxide_folders(void) {
    const char *output =
        "/home/user/Projects/cofi\n"
        "/home/user/Projects/codex-msgnr\n";
    TmuxFolder folders[MAX_TMUX_FOLDERS];
    char error[256];

    int count = tmux_parse_zoxide_list_test_hook(output, folders, MAX_TMUX_FOLDERS,
                                                 error, sizeof(error));

    ASSERT_EQ_INT("zoxide folder count", 2, count);
    ASSERT_STR_EQ("first zoxide path", "/home/user/Projects/cofi", folders[0].path);
    ASSERT_STR_EQ("first zoxide label", "cofi", folders[0].label);
    ASSERT_STR_EQ("second zoxide label", "codex-msgnr", folders[1].label);
    ASSERT_STR_EQ("zoxide parse no error", "", error);
    for (int i = 0; i < count; i++) {
        g_free(folders[i].path);
        g_free(folders[i].label);
    }
}

static void test_folder_session_command_uses_start_directory(void) {
    gchar *cmd = tmux_build_folder_session_command("/home/user/Projects/cofi");

    ASSERT_STR_EQ("folder command creates or attaches in directory",
                  "tmux new-session -A -s 'cofi' -c '/home/user/Projects/cofi'", cmd);
    g_free(cmd);
}

static void test_folder_session_command_quotes_path_and_sanitizes_name(void) {
    gchar *cmd = tmux_build_folder_session_command("/tmp/work:api session");

    ASSERT_STR_EQ("folder command quotes path and sanitizes session name",
                  "tmux new-session -A -s 'work_api_session' -c '/tmp/work:api session'", cmd);
    g_free(cmd);
}

static void test_folder_session_name_replaces_tmux_separators(void) {
    gchar *cmd = tmux_build_folder_session_command("/tmp/my.project");

    ASSERT_STR_EQ("folder command replaces dot in session name",
                  "tmux new-session -A -s 'my_project' -c '/tmp/my.project'", cmd);
    g_free(cmd);
}

static void test_kill_command_uses_exact_target(void) {
    gchar *cmd = tmux_build_kill_command("work:api session");

    ASSERT_STR_EQ("kill command exact target quoted",
                  "tmux kill-session -t '=work:api session'", cmd);
    g_free(cmd);
}

static void test_rename_command_quotes_old_and_new_names(void) {
    gchar *cmd = tmux_build_rename_command("work:api session", "renamed session");

    ASSERT_STR_EQ("rename command quotes exact target and new name",
                  "tmux rename-session -t '=work:api session' 'renamed session'", cmd);
    g_free(cmd);
}

static void test_new_session_command_uses_home_directory(void) {
    gchar *cmd = tmux_build_new_session_command("scratch", "/home/user");

    ASSERT_STR_EQ("new session command starts in home",
                  "tmux new-session -A -s 'scratch' -c '/home/user'", cmd);
    g_free(cmd);
}

static void test_parse_zellij_sessions(void) {
    TmuxSession sessions[4];
    char error[128];
    int count = tmux_parse_zellij_session_list_test_hook(
        "home\nwork api\ncofi\n", sessions, 4, error, sizeof(error));

    ASSERT_EQ_INT("zellij parse count", 3, count);
    ASSERT_STR_EQ("zellij first session name", "home", sessions[0].name);
    ASSERT_STR_EQ("zellij preserves spaces", "work api", sessions[1].name);
    ASSERT_EQ_INT("zellij backend marker", TMUX_SESSION_ZELLIJ, sessions[0].backend);
    ASSERT_STR_EQ("zellij parse no error", "", error);
}

static void test_zellij_attach_command_quotes_name(void) {
    gchar *cmd = tmux_build_zellij_attach_command("work api");

    ASSERT_STR_EQ("zellij attach command creates missing session",
                  "zellij attach --create 'work api'", cmd);
    g_free(cmd);
}

static void test_zellij_kill_command_quotes_name(void) {
    gchar *cmd = tmux_build_zellij_kill_command("work api");

    ASSERT_STR_EQ("zellij kill command quotes name",
                  "zellij kill-session 'work api'", cmd);
    g_free(cmd);
}

static void test_match_text_includes_short_backend_markers(void) {
    TmuxSession tmux_session = {
        .backend = TMUX_SESSION_TMUX,
        .windows = 2,
        .attached = 1,
    };
    g_strlcpy(tmux_session.name, "cofi", sizeof(tmux_session.name));

    TmuxSession zellij_session = {
        .backend = TMUX_SESSION_ZELLIJ,
        .windows = -1,
        .attached = -1,
    };
    g_strlcpy(zellij_session.name, "cofi", sizeof(zellij_session.name));

    TmuxFolder folder = {
        .path = "/home/user/Projects/cofi",
        .label = "cofi",
    };

    char text[256];
    tmux_format_session_match_text_test_hook(&tmux_session, text, sizeof(text));
    ASSERT_STR_EQ("tmux match row has [t] marker",
                  "[t] cofi 2 wins 1 client", text);

    tmux_format_session_match_text_test_hook(&zellij_session, text, sizeof(text));
    ASSERT_STR_EQ("zellij match row has [z] marker", "[z] cofi", text);

    tmux_format_folder_match_text_test_hook(&folder, text, sizeof(text));
    ASSERT_STR_EQ("folder match row has [d] marker",
                  "[d] cofi /home/user/Projects/cofi", text);
}

int main(void) {
    printf("tmux session parser tests\n");
    printf("=========================\n\n");

    test_parse_formatted_sessions();
    test_empty_output_reports_no_sessions();
    test_malformed_lines_are_ignored();
    test_attach_command_uses_exact_shell_quoted_target();
    test_parse_zoxide_folders();
    test_folder_session_command_uses_start_directory();
    test_folder_session_command_quotes_path_and_sanitizes_name();
    test_folder_session_name_replaces_tmux_separators();
    test_kill_command_uses_exact_target();
    test_rename_command_quotes_old_and_new_names();
    test_new_session_command_uses_home_directory();
    test_parse_zellij_sessions();
    test_zellij_attach_command_quotes_name();
    test_zellij_kill_command_quotes_name();
    test_match_text_includes_short_backend_markers();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
