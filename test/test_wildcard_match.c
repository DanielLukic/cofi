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

/* --- window_matches_harpoon_slot tests --- */

static void test_harpoon_slot_matching(void) {
    printf("\n--- window_matches_harpoon_slot ---\n");

    WindowInfo w = {0};
    w.id = 100;
    strncpy(w.title, "Terminal - bash", sizeof(w.title) - 1);
    strncpy(w.class_name, "gnome-terminal", sizeof(w.class_name) - 1);
    strncpy(w.instance, "gnome-terminal-server", sizeof(w.instance) - 1);
    strncpy(w.type, "Normal", sizeof(w.type) - 1);

    HarpoonSlot slot = {0};
    slot.assigned = 1;
    strncpy(slot.title, "Terminal - *", sizeof(slot.title) - 1);
    strncpy(slot.class_name, "gnome-terminal", sizeof(slot.class_name) - 1);
    strncpy(slot.instance, "gnome-terminal-server", sizeof(slot.instance) - 1);
    strncpy(slot.type, "Normal", sizeof(slot.type) - 1);

    ASSERT_TRUE("matching slot", window_matches_harpoon_slot(&w, &slot));

    // Wrong class
    HarpoonSlot slot_bad_class = slot;
    strncpy(slot_bad_class.class_name, "other", sizeof(slot_bad_class.class_name) - 1);
    ASSERT_TRUE("wrong class", !window_matches_harpoon_slot(&w, &slot_bad_class));

    // Wrong instance
    HarpoonSlot slot_bad_inst = slot;
    strncpy(slot_bad_inst.instance, "other", sizeof(slot_bad_inst.instance) - 1);
    ASSERT_TRUE("wrong instance", !window_matches_harpoon_slot(&w, &slot_bad_inst));

    // Wrong type
    HarpoonSlot slot_bad_type = slot;
    strncpy(slot_bad_type.type, "Special", sizeof(slot_bad_type.type) - 1);
    ASSERT_TRUE("wrong type", !window_matches_harpoon_slot(&w, &slot_bad_type));

    // Unassigned slot
    HarpoonSlot slot_unassigned = slot;
    slot_unassigned.assigned = 0;
    ASSERT_TRUE("unassigned slot", !window_matches_harpoon_slot(&w, &slot_unassigned));

    // NULL safety
    ASSERT_TRUE("NULL window", !window_matches_harpoon_slot(NULL, &slot));
    ASSERT_TRUE("NULL slot", !window_matches_harpoon_slot(&w, NULL));
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
    test_harpoon_slot_matching();

    printf("\n=====================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
