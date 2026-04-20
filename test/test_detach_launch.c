#include <stdio.h>
#include <string.h>
#include <glib/gstdio.h>
#include "../src/detach_launch.h"

// Must be compiled with -DCOFI_TESTING

static int tests_run = 0;
static int tests_passed = 0;
static int tests_skipped = 0;

#define ASSERT_STR_EQ(msg, expected, actual) \
    do { \
        tests_run++; \
        const char *_exp = (expected); \
        const char *_act = (actual); \
        if (_act != NULL && _exp != NULL && strcmp(_exp, _act) == 0) { \
            tests_passed++; printf("PASS: %s\n", msg); \
        } else { \
            printf("FAIL: %s — expected '%s', got '%s' (line %d)\n", \
                   msg, _exp ? _exp : "(null)", _act ? _act : "(null)", __LINE__); \
        } \
    } while(0)

#define ASSERT_NULL(msg, actual) \
    do { \
        tests_run++; \
        if ((actual) == NULL) { \
            tests_passed++; printf("PASS: %s\n", msg); \
        } else { \
            printf("FAIL: %s — expected NULL, got '%s' (line %d)\n", msg, (actual), __LINE__); \
        } \
    } while(0)

#define SKIP_TEST(msg) do { tests_skipped++; printf("SKIP: %s\n", msg); return; } while(0)

// Resolver stubs
static const char *resolve_nothing(const char *p) { (void)p; return NULL; }
static const char *resolve_only_xterm(const char *p) { return strcmp(p, "xterm") == 0 ? p : NULL; }
static const char *resolve_only_alacritty(const char *p) { return strcmp(p, "alacritty") == 0 ? p : NULL; }
static const char *resolve_only_kitty(const char *p) { return strcmp(p, "kitty") == 0 ? p : NULL; }

static void test_fallback_returns_null_when_nothing_found(void) {
    // resolve_nothing returns NULL for everything — including xterm — so NULL is returned
    const char *term = detect_terminal_for_test(resolve_nothing);
    ASSERT_NULL("returns NULL when no terminal resolvable", term);
}

static void test_prefer_alacritty_over_xterm(void) {
    const char *term = detect_terminal_for_test(resolve_only_alacritty);
    ASSERT_STR_EQ("prefer alacritty over xterm", "alacritty", term);
}

static void test_prefer_kitty_over_xterm(void) {
    const char *term = detect_terminal_for_test(resolve_only_kitty);
    ASSERT_STR_EQ("prefer kitty over xterm", "kitty", term);
}

static void test_xterm_resolver_returns_xterm(void) {
    const char *term = detect_terminal_for_test(resolve_only_xterm);
    ASSERT_STR_EQ("xterm resolver returns xterm", "xterm", term);
}

// $TERMINAL env tests require g_setenv; terminal flag tests require GDesktopAppInfo
#include <glib.h>
#include <gio/gdesktopappinfo.h>

static const char *resolve_all(const char *p) { return p; }

static void test_env_terminal_takes_priority(void) {
    g_setenv("TERMINAL", "wezterm", TRUE);
    const char *term = detect_terminal_for_test(resolve_all);
    ASSERT_STR_EQ("$TERMINAL takes priority", "wezterm", term);
    g_unsetenv("TERMINAL");
}

static void test_env_terminal_ignored_if_not_resolvable(void) {
    g_setenv("TERMINAL", "no-such-terminal", TRUE);
    const char *term = detect_terminal_for_test(resolve_only_alacritty);
    ASSERT_STR_EQ("unresolvable $TERMINAL falls through chain", "alacritty", term);
    g_unsetenv("TERMINAL");
}

static void test_systemd_run_argv_prefix(void) {
    const char *inner[] = {"sleep", "30", NULL};
    char **argv = build_systemd_run_argv_for_test(inner);
    ASSERT_STR_EQ("srun argv[0] is systemd-run", "systemd-run", argv[0]);
    ASSERT_STR_EQ("srun argv[1] is --user", "--user", argv[1]);
    ASSERT_STR_EQ("srun argv[2] is --scope", "--scope", argv[2]);
    ASSERT_STR_EQ("srun argv[3] is --", "--", argv[3]);
    ASSERT_STR_EQ("srun argv[4] is sleep", "sleep", argv[4]);
    ASSERT_STR_EQ("srun argv[5] is 30", "30", argv[5]);
    tests_run++;
    if (argv[6] == NULL) {
        tests_passed++;
        printf("PASS: srun argv is NULL-terminated\n");
    } else {
        printf("FAIL: srun argv[6] should be NULL\n");
    }
    g_strfreev(argv);
}

static void test_strip_field_codes_evince(void) {
    gchar *result = detach_strip_field_codes("evince %f");
    ASSERT_STR_EQ("strip %f from evince", "evince", result);
    g_free(result);
}

static void test_strip_field_codes_libreoffice(void) {
    gchar *result = detach_strip_field_codes("libreoffice --writer %U");
    ASSERT_STR_EQ("strip %U from libreoffice", "libreoffice --writer", result);
    g_free(result);
}

static void test_strip_field_codes_no_codes(void) {
    gchar *result = detach_strip_field_codes("gedit");
    ASSERT_STR_EQ("no field codes unchanged", "gedit", result);
    g_free(result);
}

static void test_strip_field_codes_percent_percent(void) {
    // XDG spec: %% → literal %
    gchar *result = detach_strip_field_codes("100%% free");
    ASSERT_STR_EQ("%% becomes literal %", "100% free", result);
    g_free(result);
}

static void test_strip_field_codes_percent_percent_only(void) {
    gchar *result = detach_strip_field_codes("%%");
    ASSERT_STR_EQ("%% alone becomes %", "%", result);
    g_free(result);
}

static void test_terminal_flag_detected_via_desktop_file(void) {
    // Write a minimal .desktop file with Terminal=true and verify GIO detects it.
    // This validates the g_desktop_app_info_get_boolean call we use in apps_launch.
    gchar *tmp_path = NULL;
    GError *err = NULL;
    int fd = g_file_open_tmp("cofi-test-XXXXXX.desktop", &tmp_path, &err);
    if (fd < 0 || !tmp_path) {
        if (err) g_error_free(err);
        SKIP_TEST("terminal flag test — could not create temp file");
    }
    close(fd);

    const char *content =
        "[Desktop Entry]\nType=Application\nName=TestHTOP\nExec=htop\nTerminal=true\n";
    g_file_set_contents(tmp_path, content, -1, NULL);

    GDesktopAppInfo *di = g_desktop_app_info_new_from_filename(tmp_path);
    g_remove(tmp_path);
    g_free(tmp_path);

    if (!di)
        SKIP_TEST("terminal flag test — GDesktopAppInfo not loadable");

    gboolean terminal = g_desktop_app_info_get_boolean(di, "Terminal");
    g_object_unref(di);

    tests_run++;
    if (terminal) {
        tests_passed++;
        printf("PASS: Terminal=true detected via GDesktopAppInfo\n");
    } else {
        printf("FAIL: Terminal=true not detected (line %d)\n", __LINE__);
    }
}

static void test_terminal_flag_false_for_gui_app(void) {
    gchar *tmp_path = NULL;
    GError *err = NULL;
    int fd = g_file_open_tmp("cofi-test-XXXXXX.desktop", &tmp_path, &err);
    if (fd < 0 || !tmp_path) {
        if (err) g_error_free(err);
        SKIP_TEST("terminal=false flag test — could not create temp file");
    }
    close(fd);

    const char *content =
        "[Desktop Entry]\nType=Application\nName=TestGUI\nExec=gedit\nTerminal=false\n";
    g_file_set_contents(tmp_path, content, -1, NULL);

    GDesktopAppInfo *di = g_desktop_app_info_new_from_filename(tmp_path);
    g_remove(tmp_path);
    g_free(tmp_path);

    if (!di)
        SKIP_TEST("terminal=false flag test — GDesktopAppInfo not loadable");

    gboolean terminal = g_desktop_app_info_get_boolean(di, "Terminal");
    g_object_unref(di);

    tests_run++;
    if (!terminal) {
        tests_passed++;
        printf("PASS: Terminal=false not set for GUI app\n");
    } else {
        printf("FAIL: Terminal unexpectedly true for GUI app (line %d)\n", __LINE__);
    }
}

// ---- Desktop terminal getter stubs ----

static const char *desktop_getter_mate(const char *desktop, ProgramResolver resolver) {
    if (strstr(desktop, "MATE")) {
        const char *term = "mate-terminal";
        return resolver(term) ? term : NULL;
    }
    return NULL;
}

static const char *desktop_getter_returns_null(const char *desktop, ProgramResolver resolver) {
    (void)desktop; (void)resolver;
    return NULL;
}

static void test_mate_desktop_returns_mate_terminal(void) {
    g_setenv("XDG_CURRENT_DESKTOP", "MATE", TRUE);
    g_unsetenv("TERMINAL");
    const char *term = detect_terminal_with_desktop_for_test(resolve_all, desktop_getter_mate);
    ASSERT_STR_EQ("MATE desktop returns mate-terminal", "mate-terminal", term);
    g_unsetenv("XDG_CURRENT_DESKTOP");
}

static void test_terminal_env_wins_over_desktop_getter(void) {
    g_setenv("XDG_CURRENT_DESKTOP", "MATE", TRUE);
    g_setenv("TERMINAL", "alacritty", TRUE);
    const char *term = detect_terminal_with_desktop_for_test(resolve_all, desktop_getter_mate);
    ASSERT_STR_EQ("$TERMINAL wins over desktop getter", "alacritty", term);
    g_unsetenv("TERMINAL");
    g_unsetenv("XDG_CURRENT_DESKTOP");
}

static void test_unknown_desktop_falls_through_to_chain(void) {
    g_setenv("XDG_CURRENT_DESKTOP", "ObscureWM", TRUE);
    g_unsetenv("TERMINAL");
    // desktop_getter_returns_null simulates unknown desktop; resolve_only_xterm means xterm wins
    const char *term = detect_terminal_with_desktop_for_test(resolve_only_xterm, desktop_getter_returns_null);
    ASSERT_STR_EQ("unknown desktop falls through to chain", "xterm", term);
    g_unsetenv("XDG_CURRENT_DESKTOP");
}

static void test_null_desktop_getter_skips_desktop_step(void) {
    g_setenv("XDG_CURRENT_DESKTOP", "MATE", TRUE);
    g_unsetenv("TERMINAL");
    // NULL getter → skip desktop step; resolve_only_xterm means xterm from candidate list
    const char *term = detect_terminal_with_desktop_for_test(resolve_only_xterm, NULL);
    ASSERT_STR_EQ("NULL getter skips desktop step", "xterm", term);
    g_unsetenv("XDG_CURRENT_DESKTOP");
}

static void test_desktop_getter_empty_result_falls_through(void) {
    g_setenv("XDG_CURRENT_DESKTOP", "MATE", TRUE);
    g_unsetenv("TERMINAL");
    // desktop_getter_returns_null returns NULL → fall through to chain
    const char *term = detect_terminal_with_desktop_for_test(resolve_only_kitty, desktop_getter_returns_null);
    ASSERT_STR_EQ("empty desktop result falls through", "kitty", term);
    g_unsetenv("XDG_CURRENT_DESKTOP");
}

#include <glib.h>  // already included above; harmless duplicate guard

static void test_fork_setsid_exec_nonexistent_binary_returns_false(void) {
    // Exercises the errno-pipe path: execvp fails → grandchild writes errno →
    // parent reads it and returns FALSE instead of silently reporting success.
    const char *argv[] = {"/nonexistent/cofi-test-binary-xyzzy-404", NULL};
    gboolean result = fork_setsid_exec_for_test(argv);
    tests_run++;
    if (!result) {
        tests_passed++;
        printf("PASS: fork_setsid_exec returns FALSE for nonexistent binary\n");
    } else {
        printf("FAIL: fork_setsid_exec returned TRUE for nonexistent binary (line %d)\n",
               __LINE__);
    }
}

static void test_fork_setsid_exec_real_binary_returns_true(void) {
    // Sanity check: a real binary should still return TRUE.
    const char *argv[] = {"true", NULL};
    gboolean result = fork_setsid_exec_for_test(argv);
    tests_run++;
    if (result) {
        tests_passed++;
        printf("PASS: fork_setsid_exec returns TRUE for real binary\n");
    } else {
        printf("FAIL: fork_setsid_exec returned FALSE for 'true' (line %d)\n", __LINE__);
    }
}

// ---- C1: terminal cmd argv shape tests ----

static void test_terminal_cmd_argv_has_sh_wrapper(void) {
    // Terminal=true with args: verify {term, "-e", sh, "-c", cmd, NULL}
    g_unsetenv("TERMINAL");
    char **argv = build_terminal_cmd_argv_for_test("htop --delay 10", resolve_only_xterm);
    tests_run++;
    if (!argv) {
        printf("FAIL: build_terminal_cmd_argv_for_test returned NULL (line %d)\n", __LINE__);
        return;
    }
    // expected: {xterm, -e, /bin/sh, -c, htop --delay 10, NULL}
    gboolean shape_ok = (strcmp(argv[0], "xterm") == 0 &&
                         strcmp(argv[1], "-e")    == 0 &&
                         strcmp(argv[3], "-c")    == 0 &&
                         strcmp(argv[4], "htop --delay 10") == 0 &&
                         argv[5] == NULL);
    if (shape_ok) {
        tests_passed++;
        printf("PASS: terminal cmd argv has sh wrapper\n");
    } else {
        printf("FAIL: terminal cmd argv shape wrong: [%s][%s][%s][%s][%s] (line %d)\n",
               argv[0], argv[1], argv[2], argv[3], argv[4], __LINE__);
    }
    g_strfreev(argv);
}

static void test_terminal_cmd_bare_path_still_works(void) {
    // Bare path (no args) also goes through sh -c — shape unchanged from caller's perspective
    g_unsetenv("TERMINAL");
    char **argv = build_terminal_cmd_argv_for_test("/usr/bin/htop", resolve_only_xterm);
    tests_run++;
    if (!argv) {
        printf("FAIL: build_terminal_cmd_argv_for_test returned NULL for bare path (line %d)\n",
               __LINE__);
        return;
    }
    gboolean has_sh = (strcmp(argv[3], "-c") == 0 &&
                       strcmp(argv[4], "/usr/bin/htop") == 0);
    if (has_sh) {
        tests_passed++;
        printf("PASS: terminal cmd bare path has sh wrapper\n");
    } else {
        printf("FAIL: terminal cmd bare path shape wrong (line %d)\n", __LINE__);
    }
    g_strfreev(argv);
}

// ---- C2: argv-not-shell parse tests ----

static void test_shell_parse_no_variable_expansion(void) {
    // g_shell_parse_argv does NOT expand $FOO — it stays literal.
    // This documents the expected behaviour of the GUI desktop entry path.
    int argc = 0;
    char **argv = NULL;
    gboolean ok = g_shell_parse_argv("echo $FOO", &argc, &argv, NULL);
    tests_run++;
    if (!ok || argc != 2 || strcmp(argv[1], "$FOO") != 0) {
        printf("FAIL: expected argv[1]==$FOO, got %s (line %d)\n",
               (argv && argc > 1) ? argv[1] : "(null)", __LINE__);
        g_strfreev(argv);
        return;
    }
    tests_passed++;
    printf("PASS: g_shell_parse_argv keeps $FOO literal (no expansion)\n");
    g_strfreev(argv);
}

static void test_shell_parse_malformed_returns_false(void) {
    int argc = 0;
    char **argv = NULL;
    GError *err = NULL;
    gboolean ok = g_shell_parse_argv("echo 'unbalanced", &argc, &argv, &err);
    tests_run++;
    if (!ok) {
        tests_passed++;
        printf("PASS: malformed Exec parse returns FALSE\n");
    } else {
        printf("FAIL: malformed Exec parse should return FALSE (line %d)\n", __LINE__);
    }
    if (err) g_error_free(err);
    g_strfreev(argv);
}

int main(void) {
    test_fallback_returns_null_when_nothing_found();
    test_prefer_alacritty_over_xterm();
    test_prefer_kitty_over_xterm();
    test_xterm_resolver_returns_xterm();
    test_env_terminal_takes_priority();
    test_env_terminal_ignored_if_not_resolvable();
    test_systemd_run_argv_prefix();
    test_strip_field_codes_evince();
    test_strip_field_codes_libreoffice();
    test_strip_field_codes_no_codes();
    test_strip_field_codes_percent_percent();
    test_strip_field_codes_percent_percent_only();
    test_terminal_flag_detected_via_desktop_file();
    test_terminal_flag_false_for_gui_app();
    test_mate_desktop_returns_mate_terminal();
    test_terminal_env_wins_over_desktop_getter();
    test_unknown_desktop_falls_through_to_chain();
    test_null_desktop_getter_skips_desktop_step();
    test_desktop_getter_empty_result_falls_through();
    test_fork_setsid_exec_nonexistent_binary_returns_false();
    test_fork_setsid_exec_real_binary_returns_true();
    test_terminal_cmd_argv_has_sh_wrapper();
    test_terminal_cmd_bare_path_still_works();
    test_shell_parse_no_variable_expansion();
    test_shell_parse_malformed_returns_false();

    printf("\nResults: %d/%d tests passed", tests_passed, tests_run);
    if (tests_skipped > 0)
        printf(" (%d skipped)", tests_skipped);
    printf("\n");
    return (tests_passed == tests_run) ? 0 : 1;
}
