#include <stdio.h>
#include <string.h>
#include "../src/window_matcher.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(desc, cond) do { \
    if (cond) { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } else { \
        printf("FAIL: %s\n", (desc)); \
        tests_failed++; \
    } \
} while (0)

/* --- wildcard_match tests --- */

static void test_exact_match(void) {
    printf("\n--- wildcard_match: exact ---\n");

    ASSERT_TRUE("identical strings", wildcard_match("hello", "hello"));
    ASSERT_TRUE("empty pattern empty string", wildcard_match("", ""));
    ASSERT_TRUE("single char", wildcard_match("a", "a"));
    ASSERT_TRUE("mismatch", !wildcard_match("hello", "world"));
    ASSERT_TRUE("different length", !wildcard_match("hello", "hell"));
    ASSERT_TRUE("different length reversed", !wildcard_match("hell", "hello"));
}

static void test_dot_wildcard(void) {
    printf("\n--- wildcard_match: dot (single char) ---\n");

    ASSERT_TRUE("dot matches one char", wildcard_match("h.llo", "hello"));
    ASSERT_TRUE("dot at start", wildcard_match(".ello", "hello"));
    ASSERT_TRUE("dot at end", wildcard_match("hell.", "hello"));
    ASSERT_TRUE("multiple dots", wildcard_match("...", "abc"));
    ASSERT_TRUE("all dots", wildcard_match(".....", "hello"));
    ASSERT_TRUE("dot does not match empty", !wildcard_match(".", ""));
    ASSERT_TRUE("dots too many", !wildcard_match("......", "hello"));
    ASSERT_TRUE("dots too few", !wildcard_match("....", "hello"));
}

static void test_star_wildcard(void) {
    printf("\n--- wildcard_match: star (any sequence) ---\n");

    ASSERT_TRUE("star matches everything", wildcard_match("*", "hello world"));
    ASSERT_TRUE("star matches empty", wildcard_match("*", ""));
    ASSERT_TRUE("star at end", wildcard_match("hello*", "hello world"));
    ASSERT_TRUE("star at start", wildcard_match("*world", "hello world"));
    ASSERT_TRUE("star in middle", wildcard_match("h*d", "hello world"));
    ASSERT_TRUE("star matches empty substring", wildcard_match("hello*", "hello"));
    ASSERT_TRUE("double star", wildcard_match("**", "anything"));
    ASSERT_TRUE("star between words", wildcard_match("foo*bar", "foobar"));
    ASSERT_TRUE("star between words 2", wildcard_match("foo*bar", "foo123bar"));
    ASSERT_TRUE("star no match", !wildcard_match("foo*baz", "foobar"));
    ASSERT_TRUE("multiple stars", wildcard_match("*foo*bar*", "XXfooYYbarZZ"));
    ASSERT_TRUE("trailing star after no match", !wildcard_match("xyz*", "abc"));
}

static void test_combined_wildcards(void) {
    printf("\n--- wildcard_match: combined . and * ---\n");

    ASSERT_TRUE("dot and star", wildcard_match("h.l*", "hello world"));
    ASSERT_TRUE("star then dot", wildcard_match("*o.ld", "hello world"));
    ASSERT_TRUE("complex pattern", wildcard_match("*.c", "main.c"));
    ASSERT_TRUE("complex pattern 2", wildcard_match("test_*.c", "test_main.c"));
    ASSERT_TRUE("dot star dot", wildcard_match(".*.*", "a.b"));
}

static void test_null_safety(void) {
    printf("\n--- wildcard_match: NULL safety ---\n");

    ASSERT_TRUE("NULL pattern", !wildcard_match(NULL, "hello"));
    ASSERT_TRUE("NULL string", !wildcard_match("hello", NULL));
    ASSERT_TRUE("both NULL", !wildcard_match(NULL, NULL));
}

static void test_real_world_titles(void) {
    printf("\n--- wildcard_match: real window titles ---\n");

    // Titles stored with * replaced by .
    ASSERT_TRUE("terminal title", wildcard_match("Terminal - bash", "Terminal - bash"));
    ASSERT_TRUE("terminal wildcard", wildcard_match("Terminal - .*", "Terminal - bash"));
    ASSERT_TRUE("terminal wildcard 2", wildcard_match("Terminal - .*", "Terminal - zsh"));
    ASSERT_TRUE("firefox page", wildcard_match("Firefox - *", "Firefox - Google Search"));
    ASSERT_TRUE("vscode file", wildcard_match("* - Visual Studio Code", "main.c - Visual Studio Code"));
    ASSERT_TRUE("exact class", wildcard_match("gnome-terminal-server", "gnome-terminal-server"));

    // Edge case: title with special characters
    ASSERT_TRUE("parens in title", wildcard_match("file (1)*", "file (1).txt"));
    ASSERT_TRUE("brackets", wildcard_match("[*] - *", "[5] - Slack"));
}

/* --- glob_match tests (dormant helper) --- */

static void test_glob_match_exact_and_empty(void) {
    printf("\n--- glob_match: exact and empty ---\n");
    ASSERT_TRUE("glob exact", glob_match("hello", "hello"));
    ASSERT_TRUE("glob mismatch", !glob_match("hello", "world"));
    ASSERT_TRUE("glob non-empty pattern empty string", !glob_match("abc", ""));
    ASSERT_TRUE("glob empty-empty", glob_match("", ""));
    ASSERT_TRUE("glob empty-nonempty", !glob_match("", "a"));
}

static void test_glob_match_question_mark(void) {
    printf("\n--- glob_match: question mark ---\n");
    ASSERT_TRUE("'?' matches one char", glob_match("h?llo", "hello"));
    ASSERT_TRUE("'?' at end", glob_match("hell?", "hello"));
    ASSERT_TRUE("'?' does not match empty", !glob_match("?", ""));
    ASSERT_TRUE("multiple '?' count must match", !glob_match("??", "a"));
}

static void test_glob_match_star(void) {
    printf("\n--- glob_match: star ---\n");
    ASSERT_TRUE("'*' matches empty", glob_match("*", ""));
    ASSERT_TRUE("'*' matches run", glob_match("a*", "abcdef"));
    ASSERT_TRUE("leading '*' matches run", glob_match("*def", "abcdef"));
    ASSERT_TRUE("middle '*' matches run", glob_match("a*f", "abcdef"));
    ASSERT_TRUE("middle '*' matches empty", glob_match("ab*cd", "abcd"));
}

static void test_glob_match_leading_and_trailing_star(void) {
    printf("\n--- glob_match: leading/trailing star ---\n");
    ASSERT_TRUE("leading and trailing star", glob_match("*core*", "xxcoreyy"));
    ASSERT_TRUE("trailing star no match", !glob_match("xyz*", "abc"));
    ASSERT_TRUE("leading star no match", !glob_match("*xyz", "abc"));
}

int main(void) {
    printf("Wildcard Match & Harpoon Slot Tests\n");
    printf("====================================\n");

    test_exact_match();
    test_dot_wildcard();
    test_star_wildcard();
    test_combined_wildcards();
    test_null_safety();
    test_real_world_titles();
    test_glob_match_exact_and_empty();
    test_glob_match_question_mark();
    test_glob_match_star();
    test_glob_match_leading_and_trailing_star();
    printf("\n=====================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
