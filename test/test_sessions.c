#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "../src/sessions.h"
#include "../src/sessions_commands.h"
#include "../src/sessions_folder_windows.h"
#include "../src/sessions_parse.h"

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
    SessionEntry sessions[MAX_SESSIONS];
    char error[256];

    int count = sessions_parse_tmux_list(output, sessions, MAX_SESSIONS,
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
    SessionEntry sessions[MAX_SESSIONS];
    char error[256];

    int count = sessions_parse_tmux_list("", sessions, MAX_SESSIONS,
                                         error, sizeof(error));

    ASSERT_EQ_INT("empty output count", 0, count);
    ASSERT_STR_EQ("empty output message", "No tmux sessions", error);
}

static void test_malformed_lines_are_ignored(void) {
    const char *output =
        "not enough fields\n"
        "bad\tNaN\t0\n"
        "valid\t4\t0\n";
    SessionEntry sessions[MAX_SESSIONS];
    char error[256];

    int count = sessions_parse_tmux_list(output, sessions, MAX_SESSIONS,
                                         error, sizeof(error));

    ASSERT_EQ_INT("malformed lines skipped", 1, count);
    ASSERT_STR_EQ("valid line parsed", "valid", sessions[0].name);
    ASSERT_EQ_INT("valid windows parsed", 4, sessions[0].windows);
    ASSERT_STR_EQ("malformed skipped without error when valid rows exist", "", error);
}

static void test_attach_command_uses_exact_shell_quoted_target(void) {
    gchar *cmd = sessions_build_tmux_attach_command("work:api session");

    ASSERT_STR_EQ("attach command exact target quoted",
                  "tmux attach-session -t '=work:api session'", cmd);
    g_free(cmd);
}

static void test_parse_zoxide_folders(void) {
    const char *output =
        "/home/user/Projects/cofi\n"
        "/home/user/Projects/codex-msgnr\n";
    SessionFolder folders[MAX_SESSION_FOLDERS];
    char error[256];

    int count = sessions_parse_zoxide_list(output, folders, MAX_SESSION_FOLDERS,
                                           error, sizeof(error));

    ASSERT_EQ_INT("zoxide folder count", 2, count);
    ASSERT_STR_EQ("first zoxide path", "/home/user/Projects/cofi", folders[0].path);
    ASSERT_STR_EQ("first zoxide label", "cofi", folders[0].label);
    ASSERT_STR_EQ("second zoxide label", "codex-msgnr", folders[1].label);
    ASSERT_STR_EQ("zoxide parse no error", "", error);
    sessions_clear_folders(folders, count);
}

static void test_folder_session_command_uses_start_directory(void) {
    gchar *session_name = sessions_build_folder_session_name("/home/user/Projects/cofi");
    gchar *cmd = sessions_build_tmux_new_command(session_name, "/home/user/Projects/cofi");

    ASSERT_STR_EQ("folder command creates or attaches in directory",
                  "tmux new-session -A -s 'cofi' -c '/home/user/Projects/cofi'", cmd);
    g_free(cmd);
    g_free(session_name);
}

static void test_folder_session_command_quotes_path_and_sanitizes_name(void) {
    gchar *session_name = sessions_build_folder_session_name("/tmp/work:api session");
    gchar *cmd = sessions_build_tmux_new_command(session_name, "/tmp/work:api session");

    ASSERT_STR_EQ("folder command quotes path and sanitizes session name",
                  "tmux new-session -A -s 'work_api_session' -c '/tmp/work:api session'", cmd);
    g_free(cmd);
    g_free(session_name);
}

static void test_folder_session_name_replaces_tmux_separators(void) {
    gchar *session_name = sessions_build_folder_session_name("/tmp/my.project");
    gchar *cmd = sessions_build_tmux_new_command(session_name, "/tmp/my.project");

    ASSERT_STR_EQ("folder command replaces dot in session name",
                  "tmux new-session -A -s 'my_project' -c '/tmp/my.project'", cmd);
    g_free(cmd);
    g_free(session_name);
}

static void test_kill_command_uses_exact_target(void) {
    gchar *cmd = sessions_build_tmux_kill_command("work:api session");

    ASSERT_STR_EQ("kill command exact target quoted",
                  "tmux kill-session -t '=work:api session'", cmd);
    g_free(cmd);
}

static void test_rename_command_quotes_old_and_new_names(void) {
    gchar *cmd = sessions_build_tmux_rename_command("work:api session", "renamed session");

    ASSERT_STR_EQ("rename command quotes exact target and new name",
                  "tmux rename-session -t '=work:api session' 'renamed session'", cmd);
    g_free(cmd);
}

static void test_new_session_command_uses_home_directory(void) {
    gchar *cmd = sessions_build_tmux_new_command("scratch", "/home/user");

    ASSERT_STR_EQ("new session command starts in home",
                  "tmux new-session -A -s 'scratch' -c '/home/user'", cmd);
    g_free(cmd);
}

static void test_parse_zellij_sessions(void) {
    SessionEntry sessions[4];
    char error[128];
    int count = sessions_parse_zellij_list(
        "home\nwork api\ncofi\n", sessions, 4, error, sizeof(error));

    ASSERT_EQ_INT("zellij parse count", 3, count);
    ASSERT_STR_EQ("zellij first session name", "home", sessions[0].name);
    ASSERT_STR_EQ("zellij preserves spaces", "work api", sessions[1].name);
    ASSERT_EQ_INT("zellij backend marker", SESSION_BACKEND_ZELLIJ, sessions[0].backend);
    ASSERT_STR_EQ("zellij parse no error", "", error);
}

static void test_zellij_attach_command_quotes_name(void) {
    gchar *cmd = sessions_build_zellij_attach_command("work api");

    ASSERT_STR_EQ("zellij attach command creates missing session",
                  "zellij attach --create 'work api'", cmd);
    g_free(cmd);
}

static void test_zellij_kill_command_quotes_name(void) {
    gchar *cmd = sessions_build_zellij_kill_command("work api");

    ASSERT_STR_EQ("zellij kill command quotes name",
                  "zellij kill-session 'work api'", cmd);
    g_free(cmd);
}

static void test_zellij_new_command_starts_in_directory(void) {
    gchar *cmd = sessions_build_zellij_new_command("work api", "/tmp/work api");

    ASSERT_STR_EQ("zellij new command changes directory before attach",
                  "cd '/tmp/work api' && zellij attach --create 'work api'", cmd);
    g_free(cmd);
}

static void test_match_text_includes_short_backend_markers(void) {
    SessionEntry tmux_session = {
        .backend = SESSION_BACKEND_TMUX,
        .windows = 2,
        .attached = 1,
    };
    g_strlcpy(tmux_session.name, "cofi", sizeof(tmux_session.name));

    SessionEntry zellij_session = {
        .backend = SESSION_BACKEND_ZELLIJ,
        .windows = -1,
        .attached = -1,
    };
    g_strlcpy(zellij_session.name, "cofi", sizeof(zellij_session.name));

    SessionFolder folder = {
        .path = "/home/user/Projects/cofi",
        .label = "cofi",
    };

    char text[256];
    sessions_format_session_match_text(&tmux_session, text, sizeof(text));
    ASSERT_STR_EQ("tmux match row has [t] marker",
                  "[t] cofi 2 wins 1 client", text);

    sessions_format_session_match_text(&zellij_session, text, sizeof(text));
    ASSERT_STR_EQ("zellij match row has [z] marker", "[z] cofi", text);

    sessions_format_folder_match_text(&folder, text, sizeof(text));
    ASSERT_STR_EQ("folder match row has [d] marker",
                  "[d] cofi /home/user/Projects/cofi", text);
}

static void test_slot_payloads_are_typed(void) {
    gchar *tmux = sessions_build_session_slot_payload(SESSION_BACKEND_TMUX, "cofi");
    gchar *zellij = sessions_build_session_slot_payload(SESSION_BACKEND_ZELLIJ, "cofi");
    gchar *folder = sessions_build_folder_slot_payload("/home/user/Projects/cofi");

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
    SessionSlotTarget target;

    ASSERT_TRUE("parse tmux slot payload",
                sessions_parse_slot_payload("session:tmux:work:api", &target));
    ASSERT_EQ_INT("tmux slot kind", SESSION_SLOT_SESSION, target.kind);
    ASSERT_EQ_INT("tmux slot backend", SESSION_BACKEND_TMUX, target.backend);
    ASSERT_STR_EQ("tmux slot value preserves colon", "work:api", target.value);

    ASSERT_TRUE("parse zellij slot payload",
                sessions_parse_slot_payload("session:zellij:work", &target));
    ASSERT_EQ_INT("zellij slot backend", SESSION_BACKEND_ZELLIJ, target.backend);
    ASSERT_STR_EQ("zellij slot value", "work", target.value);

    ASSERT_TRUE("parse folder slot payload",
                sessions_parse_slot_payload("folder:/home/user/Projects/cofi", &target));
    ASSERT_EQ_INT("folder slot kind", SESSION_SLOT_FOLDER, target.kind);
    ASSERT_STR_EQ("folder slot value", "/home/user/Projects/cofi", target.value);

    ASSERT_TRUE("reject empty typed payload",
                !sessions_parse_slot_payload("session:tmux:", &target));
    ASSERT_TRUE("reject unknown typed payload",
                !sessions_parse_slot_payload("tmux:cofi", &target));
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
                sessions_find_caja_folder_window(windows, 3, NULL, 0,
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
                sessions_find_caja_folder_window(windows, 2, NULL, 0,
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
                sessions_find_caja_folder_window(windows, 2, stack, 2,
                                                 "/home/user", &found));
    ASSERT_EQ_INT("topmost duplicate selected", 30, (int)found);
}

static void test_caja_folder_window_rejects_substring_match(void) {
    WindowInfo windows[] = {
        test_window(40, "dl-work", "caja", "Caja", WINDOW_TYPE_NORMAL),
    };
    Window found = 0;

    ASSERT_TRUE("no substring match for caja folder",
                !sessions_find_caja_folder_window(windows, 1, NULL, 0,
                                                  "/home/user", &found));
    ASSERT_EQ_INT("no substring window", 0, (int)found);
}

int main(void) {
    printf("sessions parser tests\n");
    printf("=====================\n\n");

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
    test_zellij_new_command_starts_in_directory();
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
