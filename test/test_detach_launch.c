#include <stdio.h>
#include <string.h>
#include "../src/detach_launch.h"

// Must be compiled with -DCOFI_TESTING

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_STR_EQ(msg, expected, actual) \
    do { \
        tests_run++; \
        if (strcmp((expected), (actual)) == 0) { \
            tests_passed++; printf("PASS: %s\n", msg); \
        } else { \
            printf("FAIL: %s — expected '%s', got '%s' (line %d)\n", msg, expected, actual, __LINE__); \
        } \
    } while(0)

// Resolver stubs
static const char *resolve_nothing(const char *p) { (void)p; return NULL; }
static const char *resolve_only_xterm(const char *p) { return strcmp(p, "xterm") == 0 ? p : NULL; }
static const char *resolve_only_alacritty(const char *p) { return strcmp(p, "alacritty") == 0 ? p : NULL; }
static const char *resolve_only_kitty(const char *p) { return strcmp(p, "kitty") == 0 ? p : NULL; }

static void test_fallback_to_xterm_when_nothing_found(void) {
    // resolve_nothing returns NULL for everything — must fall through to hardcoded "xterm"
    const char *term = detect_terminal_for_test(resolve_nothing);
    ASSERT_STR_EQ("fallback to xterm when nothing found", "xterm", term);
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
    tests_run++;
    if (fd < 0 || !tmp_path) {
        printf("SKIP: terminal flag test — could not create temp file\n");
        if (err) g_error_free(err);
        return;
    }
    close(fd);

    const char *content =
        "[Desktop Entry]\nType=Application\nName=TestHTOP\nExec=htop\nTerminal=true\n";
    g_file_set_contents(tmp_path, content, -1, NULL);

    GDesktopAppInfo *di = g_desktop_app_info_new_from_filename(tmp_path);
    g_remove(tmp_path);
    g_free(tmp_path);

    if (!di) {
        printf("SKIP: terminal flag test — GDesktopAppInfo not loadable\n");
        return;
    }

    gboolean terminal = g_desktop_app_info_get_boolean(di, "Terminal");
    g_object_unref(di);

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
    tests_run++;
    if (fd < 0 || !tmp_path) {
        printf("SKIP: terminal=false flag test — could not create temp file\n");
        if (err) g_error_free(err);
        return;
    }
    close(fd);

    const char *content =
        "[Desktop Entry]\nType=Application\nName=TestGUI\nExec=gedit\nTerminal=false\n";
    g_file_set_contents(tmp_path, content, -1, NULL);

    GDesktopAppInfo *di = g_desktop_app_info_new_from_filename(tmp_path);
    g_remove(tmp_path);
    g_free(tmp_path);

    if (!di) {
        printf("SKIP: terminal=false flag test — GDesktopAppInfo not loadable\n");
        return;
    }

    gboolean terminal = g_desktop_app_info_get_boolean(di, "Terminal");
    g_object_unref(di);

    if (!terminal) {
        tests_passed++;
        printf("PASS: Terminal=false not set for GUI app\n");
    } else {
        printf("FAIL: Terminal unexpectedly true for GUI app (line %d)\n", __LINE__);
    }
}

int main(void) {
    test_fallback_to_xterm_when_nothing_found();
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

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
