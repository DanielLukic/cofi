#include <stdio.h>
#include <string.h>

#include "profiles/chrome_launch.h"

static int tests_run = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(name, cond) do { \
    tests_run++; \
    if (cond) printf("PASS: %s\n", name); \
    else { printf("FAIL: %s\n", name); tests_failed++; } \
} while (0)

static const char *g_last_request;
static int g_resolver_calls;
static const char *g_resolver_response_for_primary;

static gchar *fake_resolver(const char *program) {
    g_resolver_calls++;
    g_last_request = program;
    if (program && strcmp(program, "google-chrome") == 0 && g_resolver_response_for_primary) {
        return g_strdup(g_resolver_response_for_primary);
    }
    if (program && strcmp(program, "google-chrome-stable") == 0) {
        return g_strdup("/usr/bin/google-chrome-stable");
    }
    return NULL;
}

static BrowserProfileEntry make_chrome_entry(void) {
    BrowserProfileEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.backend = BROWSER_PROFILE_CHROME;
    g_strlcpy(entry.browser_id, "chrome", sizeof(entry.browser_id));
    g_strlcpy(entry.browser_name, "Chrome", sizeof(entry.browser_name));
    g_strlcpy(entry.executable, "google-chrome", sizeof(entry.executable));
    g_strlcpy(entry.profile_dir, "Profile 14", sizeof(entry.profile_dir));
    g_strlcpy(entry.name, "GS", sizeof(entry.name));
    return entry;
}

static void test_argv_without_url(void) {
    char **argv = chrome_launch_build_argv("/usr/bin/google-chrome", "Profile 14", NULL, FALSE);
    ASSERT_TRUE("no-url argv path", strcmp(argv[0], "/usr/bin/google-chrome") == 0);
    ASSERT_TRUE("no-url argv profile flag",
                strcmp(argv[1], "--profile-directory=Profile 14") == 0);
    ASSERT_TRUE("no-url argv null terminated at slot 2", argv[2] == NULL);
    g_strfreev(argv);
}

static void test_argv_with_empty_url(void) {
    char **argv = chrome_launch_build_argv("/usr/bin/google-chrome", "Default", "", FALSE);
    ASSERT_TRUE("empty-url argv path", strcmp(argv[0], "/usr/bin/google-chrome") == 0);
    ASSERT_TRUE("empty-url argv profile flag",
                strcmp(argv[1], "--profile-directory=Default") == 0);
    ASSERT_TRUE("empty-url argv null terminated at slot 2", argv[2] == NULL);
    g_strfreev(argv);
}

static void test_argv_with_url(void) {
    char **argv = chrome_launch_build_argv("/usr/bin/google-chrome",
                                           "Profile 14",
                                           "https://example.com/path?q=1",
                                           FALSE);
    ASSERT_TRUE("url argv path", strcmp(argv[0], "/usr/bin/google-chrome") == 0);
    ASSERT_TRUE("url argv profile flag",
                strcmp(argv[1], "--profile-directory=Profile 14") == 0);
    ASSERT_TRUE("url argv url slot",
                strcmp(argv[2], "https://example.com/path?q=1") == 0);
    ASSERT_TRUE("url argv null terminated at slot 3", argv[3] == NULL);
    g_strfreev(argv);
}

static void test_argv_new_window_with_url(void) {
    char **argv = chrome_launch_build_argv("/usr/bin/google-chrome",
                                           "Profile 14",
                                           "https://example.com/path?q=1",
                                           TRUE);
    ASSERT_TRUE("new-window argv path", strcmp(argv[0], "/usr/bin/google-chrome") == 0);
    ASSERT_TRUE("new-window argv profile flag",
                strcmp(argv[1], "--profile-directory=Profile 14") == 0);
    ASSERT_TRUE("new-window argv flag at slot 2",
                strcmp(argv[2], "--new-window") == 0);
    ASSERT_TRUE("new-window argv url slot",
                strcmp(argv[3], "https://example.com/path?q=1") == 0);
    ASSERT_TRUE("new-window argv null terminated at slot 4", argv[4] == NULL);
    g_strfreev(argv);
}

static void test_resolve_primary_hit(void) {
    g_resolver_calls = 0;
    g_resolver_response_for_primary = "/usr/bin/google-chrome";
    chrome_launch_set_program_resolver_test_hook(fake_resolver);

    BrowserProfileEntry entry = make_chrome_entry();
    gchar *path = chrome_launch_resolve_executable(&entry);
    ASSERT_TRUE("resolver returns primary hit", path && strcmp(path, "/usr/bin/google-chrome") == 0);
    ASSERT_TRUE("resolver called once for primary", g_resolver_calls == 1);
    g_free(path);
}

static void test_resolve_fallback_to_stable(void) {
    g_resolver_calls = 0;
    g_resolver_response_for_primary = NULL;
    chrome_launch_set_program_resolver_test_hook(fake_resolver);

    BrowserProfileEntry entry = make_chrome_entry();
    gchar *path = chrome_launch_resolve_executable(&entry);
    ASSERT_TRUE("resolver falls back to google-chrome-stable",
                path && strcmp(path, "/usr/bin/google-chrome-stable") == 0);
    ASSERT_TRUE("resolver called twice", g_resolver_calls == 2);
    g_free(path);
}

static void test_resolve_rejects_null_entry(void) {
    gchar *path = chrome_launch_resolve_executable(NULL);
    ASSERT_TRUE("null entry returns null", path == NULL);
}

int main(void) {
    test_argv_without_url();
    test_argv_with_empty_url();
    test_argv_with_url();
    test_argv_new_window_with_url();
    test_resolve_primary_hit();
    test_resolve_fallback_to_stable();
    test_resolve_rejects_null_entry();

    printf("\nResults: %d/%d tests passed\n", tests_run - tests_failed, tests_run);
    return tests_failed == 0 ? 0 : 1;
}
