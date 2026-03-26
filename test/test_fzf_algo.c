/*
 * test_fzf_algo.c - Tests for fzf FuzzyMatchV2 port
 *
 * Test cases ported from:
 *   https://github.com/junegunn/fzf/blob/master/src/algo/algo_test.go
 *
 * Plus additional tests for cofi-specific requirements.
 *
 * Compile and run:
 *   gcc -o test/test_fzf_algo test/test_fzf_algo.c src/fzf_algo.c -lm && ./test/test_fzf_algo
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../src/fzf_algo.h"

/* --- Scoring constants (must match fzf_algo.c) --- */
#define SCORE_MATCH         16
#define SCORE_GAP_START     -3
#define SCORE_GAP_EXTENSION -1
#define BONUS_BOUNDARY      (SCORE_MATCH / 2)
#define BONUS_NON_WORD      (SCORE_MATCH / 2)
#define BONUS_CAMEL123      (BONUS_BOUNDARY + SCORE_GAP_EXTENSION)
#define BONUS_CONSECUTIVE   (-(SCORE_GAP_START + SCORE_GAP_EXTENSION))
#define BONUS_BOUNDARY_WHITE     (BONUS_BOUNDARY + 2)
#define BONUS_BOUNDARY_DELIMITER (BONUS_BOUNDARY + 1)
#define BONUS_FIRST_CHAR_MULTIPLIER 2

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_SCORE(haystack, needle, expected_score, desc) do { \
    tests_run++; \
    score_t actual = fzf_fuzzy_match(needle, haystack); \
    if (fabs(actual - (expected_score)) < 0.001) { \
        tests_passed++; \
        printf("  PASS: %s\n", desc); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s\n", desc); \
        printf("        needle=\"%s\" haystack=\"%s\"\n", needle ? (const char *)needle : "(null)", haystack ? (const char *)haystack : "(null)"); \
        printf("        expected=%d got=%.0f\n", (int)(expected_score), actual); \
    } \
} while (0)

#define ASSERT_MATCH(haystack, needle, desc) do { \
    tests_run++; \
    int result = fzf_has_match(needle, haystack); \
    if (result) { \
        tests_passed++; \
        printf("  PASS: %s\n", desc); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s (expected match)\n", desc); \
    } \
} while (0)

#define ASSERT_NO_MATCH(haystack, needle, desc) do { \
    tests_run++; \
    int result = fzf_has_match(needle, haystack); \
    if (!result) { \
        tests_passed++; \
        printf("  PASS: %s\n", desc); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s (expected no match)\n", desc); \
    } \
} while (0)

#define ASSERT_HIGHER(haystack1, haystack2, needle, desc) do { \
    tests_run++; \
    score_t s1 = fzf_fuzzy_match(needle, haystack1); \
    score_t s2 = fzf_fuzzy_match(needle, haystack2); \
    if (s1 > s2) { \
        tests_passed++; \
        printf("  PASS: %s\n", desc); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s\n", desc); \
        printf("        \"%s\" scored %.0f, \"%s\" scored %.0f\n", \
               haystack1, s1, haystack2, s2); \
    } \
} while (0)

/* ---- Test cases ported from algo_test.go ---- */

static void test_fzf_basic_matching(void) {
    printf("\n--- Basic Matching ---\n");

    /* From TestFuzzyMatch in algo_test.go (case-insensitive, forward) */

    /* "fooBarbaz1", "oBZ" -> match at 2..9 */
    ASSERT_SCORE("fooBarbaz1", "oBZ",
        SCORE_MATCH * 3 + BONUS_CAMEL123 + SCORE_GAP_START + SCORE_GAP_EXTENSION * 3,
        "fooBarbaz1/oBZ: camelCase + gap");

    /* "foo bar baz", "fbb" */
    ASSERT_SCORE("foo bar baz", "fbb",
        SCORE_MATCH * 3 + BONUS_BOUNDARY_WHITE * BONUS_FIRST_CHAR_MULTIPLIER +
        BONUS_BOUNDARY_WHITE * 2 + 2 * SCORE_GAP_START + 4 * SCORE_GAP_EXTENSION,
        "foo bar baz/fbb: whitespace boundaries");

    /* "/AutomatorDocument.icns", "rdoc" */
    ASSERT_SCORE("/AutomatorDocument.icns", "rdoc",
        SCORE_MATCH * 4 + BONUS_CAMEL123 + BONUS_CONSECUTIVE * 2,
        "/AutomatorDocument.icns/rdoc: camelCase consecutive");

    /* "/man1/zshcompctl.1", "zshc" */
    ASSERT_SCORE("/man1/zshcompctl.1", "zshc",
        SCORE_MATCH * 4 + BONUS_BOUNDARY_DELIMITER * BONUS_FIRST_CHAR_MULTIPLIER +
        BONUS_BOUNDARY_DELIMITER * 3,
        "/man1/zshcompctl.1/zshc: delimiter boundary");

    /* "/.oh-my-zsh/cache", "zshc" */
    ASSERT_SCORE("/.oh-my-zsh/cache", "zshc",
        SCORE_MATCH * 4 + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER +
        BONUS_BOUNDARY * 2 + SCORE_GAP_START + BONUS_BOUNDARY_DELIMITER,
        "/.oh-my-zsh/cache/zshc: mixed boundary");

    /* "ab0123 456", "12356" */
    ASSERT_SCORE("ab0123 456", "12356",
        SCORE_MATCH * 5 + BONUS_CONSECUTIVE * 3 + SCORE_GAP_START + SCORE_GAP_EXTENSION,
        "ab0123 456/12356: consecutive + gap");

    /* "abc123 456", "12356" */
    ASSERT_SCORE("abc123 456", "12356",
        SCORE_MATCH * 5 + BONUS_CAMEL123 * BONUS_FIRST_CHAR_MULTIPLIER +
        BONUS_CAMEL123 * 2 + BONUS_CONSECUTIVE + SCORE_GAP_START + SCORE_GAP_EXTENSION,
        "abc123 456/12356: camelCase123 + consecutive");

    /* "foo/bar/baz", "fbb" */
    ASSERT_SCORE("foo/bar/baz", "fbb",
        SCORE_MATCH * 3 + BONUS_BOUNDARY_WHITE * BONUS_FIRST_CHAR_MULTIPLIER +
        BONUS_BOUNDARY_DELIMITER * 2 + 2 * SCORE_GAP_START + 4 * SCORE_GAP_EXTENSION,
        "foo/bar/baz/fbb: delimiter boundaries");

    /* "fooBarBaz", "fbb" */
    ASSERT_SCORE("fooBarBaz", "fbb",
        SCORE_MATCH * 3 + BONUS_BOUNDARY_WHITE * BONUS_FIRST_CHAR_MULTIPLIER +
        BONUS_CAMEL123 * 2 + 2 * SCORE_GAP_START + 2 * SCORE_GAP_EXTENSION,
        "fooBarBaz/fbb: camelCase");

    /* "foo barbaz", "fbb" */
    ASSERT_SCORE("foo barbaz", "fbb",
        SCORE_MATCH * 3 + BONUS_BOUNDARY_WHITE * BONUS_FIRST_CHAR_MULTIPLIER +
        BONUS_BOUNDARY_WHITE + SCORE_GAP_START * 2 + SCORE_GAP_EXTENSION * 3,
        "foo barbaz/fbb: whitespace + word boundary");

    /* "fooBar Baz", "foob" */
    ASSERT_SCORE("fooBar Baz", "foob",
        SCORE_MATCH * 4 + BONUS_BOUNDARY_WHITE * BONUS_FIRST_CHAR_MULTIPLIER +
        BONUS_BOUNDARY_WHITE * 3,
        "fooBar Baz/foob: consecutive from start");

    /* "xFoo-Bar Baz", "foo-b" */
    ASSERT_SCORE("xFoo-Bar Baz", "foo-b",
        SCORE_MATCH * 5 + BONUS_CAMEL123 * BONUS_FIRST_CHAR_MULTIPLIER +
        BONUS_CAMEL123 * 2 + BONUS_NON_WORD + BONUS_BOUNDARY,
        "xFoo-Bar Baz/foo-b: camelCase with non-word");
}

static void test_fzf_no_match(void) {
    printf("\n--- No Match Cases ---\n");

    ASSERT_SCORE("fooBarbaz", "fooBarbazz", SCORE_MIN,
        "needle longer than available match");

    score_t s = fzf_fuzzy_match("xyz", "abc");
    tests_run++;
    if (s <= SCORE_MIN + 1.0) {
        tests_passed++;
        printf("  PASS: completely unrelated strings\n");
    } else {
        tests_failed++;
        printf("  FAIL: completely unrelated strings (got %.0f)\n", s);
    }
}

static void test_fzf_has_match(void) {
    printf("\n--- has_match ---\n");

    ASSERT_MATCH("foobar", "fb", "fb in foobar");
    ASSERT_MATCH("foobar", "foobar", "exact match");
    ASSERT_MATCH("FooBar", "fb", "case-insensitive");
    ASSERT_MATCH("foobar", "", "empty needle matches everything");
    ASSERT_NO_MATCH("foobar", "xyz", "no subsequence");
    ASSERT_NO_MATCH("foobar", "foobarx", "needle longer");
    ASSERT_NO_MATCH("", "a", "empty haystack");
}

static void test_fzf_edge_cases(void) {
    printf("\n--- Edge Cases ---\n");

    /* Empty needle */
    ASSERT_SCORE("foobar", "", 0, "empty needle returns 0");

    /* Empty haystack */
    ASSERT_SCORE("", "a", SCORE_MIN, "empty haystack returns SCORE_MIN");

    /* Needle longer than haystack */
    ASSERT_SCORE("ab", "abc", SCORE_MIN, "needle longer than haystack");

    /* Exact length match */
    score_t s = fzf_fuzzy_match("abc", "abc");
    tests_run++;
    if (s > 0) {
        tests_passed++;
        printf("  PASS: exact match scores positive (%.0f)\n", s);
    } else {
        tests_failed++;
        printf("  FAIL: exact match should score positive (got %.0f)\n", s);
    }

    /* Single character */
    s = fzf_fuzzy_match("a", "a");
    tests_run++;
    if (s > 0) {
        tests_passed++;
        printf("  PASS: single char match scores positive (%.0f)\n", s);
    } else {
        tests_failed++;
        printf("  FAIL: single char match should score positive (got %.0f)\n", s);
    }

    /* NULL inputs */
    ASSERT_SCORE(NULL, "a", SCORE_MIN, "NULL haystack");

    s = fzf_fuzzy_match(NULL, "abc");
    tests_run++;
    if (s <= SCORE_MIN + 1.0) {
        tests_passed++;
        printf("  PASS: NULL needle returns SCORE_MIN\n");
    } else {
        tests_failed++;
        printf("  FAIL: NULL needle should return SCORE_MIN (got %.0f)\n", s);
    }
}

static void test_fzf_word_boundary_bonus(void) {
    printf("\n--- Word Boundary Bonus ---\n");

    /* "fb" should match "FooBar" better than "foobar" because of camelCase */
    ASSERT_HIGHER("FooBar", "foobar", "fb",
        "FooBar > foobar for 'fb' (camelCase bonus)");

    /* Word boundary should beat non-boundary */
    ASSERT_HIGHER("foo-bar", "fxxobar", "fb",
        "foo-bar > fxxobar for 'fb' (word boundary)");

    /* Whitespace boundary is best */
    ASSERT_HIGHER("foo bar", "foo-bar", "fb",
        "foo bar > foo-bar for 'fb' (whitespace > delimiter)");
}

static void test_fzf_consecutive_bonus(void) {
    printf("\n--- Consecutive Match Bonus ---\n");

    /* Consecutive matches should score higher */
    ASSERT_HIGHER("abcdef", "aXbXcXdXeXf", "abcdef",
        "consecutive > scattered for 'abcdef'");

    /* Substring should score well */
    score_t s = fzf_fuzzy_match("abc", "xxabcxx");
    tests_run++;
    if (s > 0) {
        tests_passed++;
        printf("  PASS: substring match scores positive (%.0f)\n", s);
    } else {
        tests_failed++;
        printf("  FAIL: substring match should score positive (got %.0f)\n", s);
    }
}

static void test_fzf_long_string_handling(void) {
    printf("\n--- Long String Handling (key difference from fzy) ---\n");

    /*
     * This is the critical test: fzf's algorithm floors scores at 0,
     * so a long string with the match near the beginning should NOT
     * be penalized vs a shorter string with scattered matches.
     */

    /* Build a long string with "Tribal Heritage" near the beginning */
    char long_str[256];
    snprintf(long_str, sizeof(long_str),
        "Tribal Heritage - A Very Long Window Title That Goes On And On "
        "With Many Words And Extra Content To Make This String Quite Long "
        "Indeed For Testing Purposes");

    /* "tri" in the long string should score well (found in "Tribal") */
    score_t score_long = fzf_fuzzy_match("tri", long_str);

    /* "tri" in "mate-terminal" where t-r-i are scattered */
    score_t score_scattered = fzf_fuzzy_match("tri", "mate-terminal");

    tests_run++;
    if (score_long > score_scattered) {
        tests_passed++;
        printf("  PASS: 'tri' in long 'Tribal...' (%.0f) > 'mate-terminal' (%.0f)\n",
               score_long, score_scattered);
    } else {
        tests_failed++;
        printf("  FAIL: 'tri' in long 'Tribal...' (%.0f) should > 'mate-terminal' (%.0f)\n",
               score_long, score_scattered);
    }

    /* Score should be positive, not deeply negative */
    tests_run++;
    if (score_long > 0) {
        tests_passed++;
        printf("  PASS: long string match scores positive (%.0f)\n", score_long);
    } else {
        tests_failed++;
        printf("  FAIL: long string match should be positive (got %.0f)\n", score_long);
    }

    /* Another test: "tri" in "Tribal Heritage" vs "tri" in "tribulation" */
    { score_t st1 = fzf_fuzzy_match("tri", "Tribal Heritage");
      score_t st2 = fzf_fuzzy_match("tri", "tribulation");
      tests_run++;
      if (fabs(st1-st2)<0.001 && st1>0) { tests_passed++; printf("  PASS: tie ok (%.0f)\n",st1); }
      else { tests_failed++; printf("  FAIL: %.0f vs %.0f\n",st1,st2); } }
}

static void test_fzf_case_bonus(void) {
    printf("\n--- Case Sensitivity ---\n");

    /*
     * Our implementation is case-insensitive, but camelCase transitions
     * still provide bonus. Verify matching still works.
     */
    score_t s1 = fzf_fuzzy_match("fb", "FooBar");
    score_t s2 = fzf_fuzzy_match("fb", "foobar");

    tests_run++;
    if (s1 > s2) {
        tests_passed++;
        printf("  PASS: FooBar (%.0f) > foobar (%.0f) for 'fb'\n", s1, s2);
    } else {
        tests_failed++;
        printf("  FAIL: FooBar (%.0f) should > foobar (%.0f) for 'fb'\n", s1, s2);
    }

    /* Both should be valid matches */
    tests_run++;
    if (s1 > 0 && s2 > 0) {
        tests_passed++;
        printf("  PASS: both FooBar and foobar score positive\n");
    } else {
        tests_failed++;
        printf("  FAIL: both should score positive (%.0f, %.0f)\n", s1, s2);
    }
}

static void test_fzf_comparison_with_expected_order(void) {
    printf("\n--- Ordering Comparisons ---\n");

    /* Consecutive at word boundary > scattered */
    ASSERT_HIGHER("foo bar", "fXXoXXbar", "foo",
        "consecutive 'foo' > scattered 'foo'");

    /* Shorter gap > longer gap */
    ASSERT_HIGHER("ab", "aXb", "ab",
        "'ab' (no gap) > 'aXb' (gap of 1)");

    ASSERT_HIGHER("aXb", "aXXb", "ab",
        "'aXb' (gap 1) > 'aXXb' (gap 2)");

    /* Word boundary > middle of word */
    ASSERT_HIGHER("foo-bar", "fooXbar", "bar",
        "'foo-bar' (boundary) > 'fooXbar' (no boundary) for 'bar'");
}

static void test_fzf_specific_scores(void) {
    printf("\n--- Specific Score Verification (from fzf tests) ---\n");

    /* "foo-bar", "o-ba" (case-sensitive in Go test, but we check ordering) */
    /* In fzf: score = scoreMatch*4 + bonusBoundary*3 = 64 + 24 = 88 */
    /* We match case-insensitively, so same result */
    ASSERT_SCORE("foo-bar", "o-ba",
        SCORE_MATCH * 4 + BONUS_BOUNDARY * 3,
        "foo-bar/o-ba: consecutive boundary bonus");
}

static void test_fzf_real_world(void) {
    printf("\n--- Real World Window Matching ---\n");

    /* Window titles a user might search */
    ASSERT_HIGHER(
        "Firefox - Google Search",
        "Filesystem Manager",
        "fire",
        "'Firefox...' > 'Filesystem...' for 'fire'");

    { score_t st1 = fzf_fuzzy_match("term", "Terminal - bash");
      score_t st2 = fzf_fuzzy_match("term", "Settings - Preferences - Terminal");
      tests_run++;
      if (fabs(st1-st2)<0.001 && st1>0) { tests_passed++; printf("  PASS: term tie ok (%.0f)\n",st1); }
      else { tests_failed++; printf("  FAIL: %.0f vs %.0f\n",st1,st2); } }

    /* Class names */
    ASSERT_HIGHER(
        "gnome-characters",
        "google-chrome",
        "gc",
        "'gnome-characters' > 'google-chrome' for 'gc' (shorter gap)");
}

int main(void) {
    printf("=== fzf FuzzyMatchV2 Algorithm Tests ===\n");

    test_fzf_basic_matching();
    test_fzf_no_match();
    test_fzf_has_match();
    test_fzf_edge_cases();
    test_fzf_word_boundary_bonus();
    test_fzf_consecutive_bonus();
    test_fzf_long_string_handling();
    test_fzf_case_bonus();
    test_fzf_comparison_with_expected_order();
    test_fzf_specific_scores();
    test_fzf_real_world();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
