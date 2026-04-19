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

// $TERMINAL env tests require g_setenv — include glib
#include <glib.h>

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

int main(void) {
    test_fallback_to_xterm_when_nothing_found();
    test_prefer_alacritty_over_xterm();
    test_prefer_kitty_over_xterm();
    test_xterm_resolver_returns_xterm();
    test_env_terminal_takes_priority();
    test_env_terminal_ignored_if_not_resolvable();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
