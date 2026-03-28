#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../src/match.h"
#include "../src/config.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_INT(desc, expected, actual) do { \
    if ((expected) != (actual)) { \
        printf("FAIL: %s - expected %d, got %d\n", (desc), (expected), (actual)); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } \
} while (0)

#define ASSERT_SCORE_EQ(desc, expected, actual) do { \
    if (fabs((double)(expected) - (double)(actual)) < 0.001) { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } else { \
        printf("FAIL: %s - expected %.2f, got %.2f\n", (desc), (double)(expected), (double)(actual)); \
        tests_failed++; \
    } \
} while (0)

#define ASSERT_TRUE(desc, cond) do { \
    if (cond) { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } else { \
        printf("FAIL: %s\n", (desc)); \
        tests_failed++; \
    } \
} while (0)

/* --- has_match tests --- */

static void test_has_match_basic(void) {
    printf("\n--- has_match: basic ---\n");

    ASSERT_INT("exact match", 1, has_match("foo", "foo"));
    ASSERT_INT("subsequence", 1, has_match("fb", "foobar"));
    ASSERT_INT("case insensitive", 1, has_match("fb", "FooBar"));
    ASSERT_INT("single char", 1, has_match("f", "foo"));
    ASSERT_INT("full string", 1, has_match("foobar", "foobar"));
}

static void test_has_match_no_match(void) {
    printf("\n--- has_match: no match ---\n");

    ASSERT_INT("reversed order", 0, has_match("ba", "abc"));
    ASSERT_INT("missing char", 0, has_match("xyz", "foobar"));
    ASSERT_INT("needle longer", 0, has_match("foobarx", "foobar"));
    ASSERT_INT("empty haystack", 0, has_match("a", ""));
}

static void test_has_match_edge_cases(void) {
    printf("\n--- has_match: edge cases ---\n");

    ASSERT_INT("empty needle", 1, has_match("", "foobar"));
    ASSERT_INT("both empty", 1, has_match("", ""));
    ASSERT_INT("special chars", 1, has_match("-_", "foo-bar_baz"));
    ASSERT_INT("repeated chars", 1, has_match("oo", "foobar"));
    ASSERT_INT("needle at end", 1, has_match("ar", "foobar"));
}

/* --- match() scoring tests --- */

static void test_match_exact(void) {
    printf("\n--- match: exact match returns SCORE_MAX ---\n");

    ASSERT_SCORE_EQ("exact match", SCORE_MAX, match("foo", "foo"));
    ASSERT_SCORE_EQ("single char exact", SCORE_MAX, match("a", "a"));
    // Case-insensitive exact
    ASSERT_SCORE_EQ("case insensitive exact", SCORE_MAX, match("foo", "FOO"));
}

static void test_match_no_match(void) {
    printf("\n--- match: no match returns SCORE_MIN ---\n");

    ASSERT_SCORE_EQ("empty needle", SCORE_MIN, match("", "foobar"));
    ASSERT_SCORE_EQ("needle longer", SCORE_MIN, match("foobarx", "foobar"));
    // Note: match() assumes has_match() was called first.
    // When n==m, it returns SCORE_MAX (assumes equal strings).
    // When n<m but chars not found, the DP produces very low scores.
    score_t s = match("xz", "abcdef");
    ASSERT_TRUE("non-subsequence gets very low score", s < 0);
}

static void test_match_scoring_order(void) {
    printf("\n--- match: relative scoring order ---\n");

    // Consecutive match should score higher than scattered
    score_t s_consecutive = match("abc", "abcdef");
    score_t s_scattered = match("abc", "aXbXcX");
    ASSERT_TRUE("consecutive > scattered", s_consecutive > s_scattered);

    // Word boundary should help (use same-length strings to control gap penalty)
    score_t s_boundary = match("fb", "foo-bar");
    score_t s_no_boundary = match("fb", "xxfxxbx");
    ASSERT_TRUE("boundary > no boundary", s_boundary > s_no_boundary);

    // Shorter gap is better
    score_t s_short_gap = match("ab", "aXb");
    score_t s_long_gap = match("ab", "aXXXXb");
    ASSERT_TRUE("short gap > long gap", s_short_gap > s_long_gap);
}

static void test_match_capital_bonus(void) {
    printf("\n--- match: capital letter bonus ---\n");

    score_t s_camel = match("fb", "FooBar");
    score_t s_lower = match("fb", "foobar");
    ASSERT_TRUE("camelCase gets bonus", s_camel > s_lower);
}

/* --- match_positions tests --- */

static void test_match_positions_exact(void) {
    printf("\n--- match_positions: exact match ---\n");

    size_t pos[16];
    score_t s = match_positions("abc", "abc", pos);
    ASSERT_SCORE_EQ("exact match score", SCORE_MAX, s);
    ASSERT_INT("pos[0] = 0", 0, (int)pos[0]);
    ASSERT_INT("pos[1] = 1", 1, (int)pos[1]);
    ASSERT_INT("pos[2] = 2", 2, (int)pos[2]);
}

static void test_match_positions_subsequence(void) {
    printf("\n--- match_positions: subsequence ---\n");

    size_t pos[16];
    score_t s = match_positions("fb", "foobar", pos);
    ASSERT_TRUE("positive score", s > SCORE_MIN);
    ASSERT_INT("f at pos 0", 0, (int)pos[0]);
    ASSERT_INT("b at pos 3", 3, (int)pos[1]);
}

static void test_match_positions_null_positions(void) {
    printf("\n--- match_positions: NULL positions array ---\n");

    // Should not crash with NULL positions
    score_t s = match_positions("abc", "abcdef", NULL);
    ASSERT_TRUE("works with NULL positions", s > SCORE_MIN);
}

static void test_match_positions_no_match(void) {
    printf("\n--- match_positions: no match ---\n");

    size_t pos[16];
    ASSERT_SCORE_EQ("empty needle", SCORE_MIN, match_positions("", "foobar", pos));
    ASSERT_SCORE_EQ("needle longer", SCORE_MIN, match_positions("foobarx", "foobar", pos));
}

/* --- Long string edge cases --- */

static void test_match_long_haystack(void) {
    printf("\n--- match: long haystack ---\n");

    // Build a string near MATCH_MAX_LEN
    char long_str[MATCH_MAX_LEN + 10];
    memset(long_str, 'x', MATCH_MAX_LEN + 5);
    long_str[MATCH_MAX_LEN + 5] = '\0';
    long_str[0] = 'a';
    long_str[MATCH_MAX_LEN + 4] = 'b';

    // Over MATCH_MAX_LEN should return SCORE_MIN
    score_t s = match("ab", long_str);
    ASSERT_SCORE_EQ("over max len returns SCORE_MIN", SCORE_MIN, s);

    // At exactly MATCH_MAX_LEN it should still work
    char exact_str[MATCH_MAX_LEN + 1];
    memset(exact_str, 'x', MATCH_MAX_LEN);
    exact_str[MATCH_MAX_LEN] = '\0';
    exact_str[0] = 'a';
    exact_str[MATCH_MAX_LEN - 1] = 'b';
    s = match("ab", exact_str);
    ASSERT_TRUE("at max len works", s > SCORE_MIN);
}

int main(void) {
    printf("Match Scoring Tests (fzy algorithm)\n");
    printf("====================================\n");

    test_has_match_basic();
    test_has_match_no_match();
    test_has_match_edge_cases();
    test_match_exact();
    test_match_no_match();
    test_match_scoring_order();
    test_match_capital_bonus();
    test_match_positions_exact();
    test_match_positions_subsequence();
    test_match_positions_null_positions();
    test_match_positions_no_match();
    test_match_long_haystack();

    printf("\n=====================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
