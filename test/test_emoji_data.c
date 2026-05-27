#include <stdio.h>
#include <string.h>

#include "emoji/emoji_data.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

static void test_table_basics(void) {
    ASSERT_TRUE("emoji table len > 0", EMOJI_TABLE_LEN > 0);
    ASSERT_TRUE("row0 glyph non-empty", EMOJI_TABLE[0].glyph && EMOJI_TABLE[0].glyph[0] != '\0');
    ASSERT_TRUE("row0 name non-empty", EMOJI_TABLE[0].name && EMOJI_TABLE[0].name[0] != '\0');
    ASSERT_TRUE("row0 name_norm non-empty", EMOJI_TABLE[0].name_norm && EMOJI_TABLE[0].name_norm[0] != '\0');
    ASSERT_TRUE("row0 keywords non-empty", EMOJI_TABLE[0].keywords && EMOJI_TABLE[0].keywords[0] != '\0');
}

static void test_no_null_fields(void) {
    for (int i = 0; i < EMOJI_TABLE_LEN; i++) {
        ASSERT_TRUE("emoji glyph non-null", EMOJI_TABLE[i].glyph != NULL);
        ASSERT_TRUE("emoji name non-null", EMOJI_TABLE[i].name != NULL);
        ASSERT_TRUE("emoji name_norm non-null", EMOJI_TABLE[i].name_norm != NULL);
        ASSERT_TRUE("emoji keywords non-null", EMOJI_TABLE[i].keywords != NULL);
    }
}

static void test_known_joy_entry_exists(void) {
    int found = 0;
    for (int i = 0; i < EMOJI_TABLE_LEN; i++) {
        if (strcmp(EMOJI_TABLE[i].glyph, "😂") == 0) {
            found = 1;
            ASSERT_TRUE("joy name contains tears", strstr(EMOJI_TABLE[i].name, "tears") != NULL);
            ASSERT_TRUE("joy keywords include joy", strstr(EMOJI_TABLE[i].keywords, "joy") != NULL);
            break;
        }
    }
    ASSERT_TRUE("joy emoji present", found == 1);
}

int main(void) {
    printf("Emoji dataset tests\n");
    printf("===================\n");

    test_table_basics();
    test_no_null_fields();
    test_known_joy_entry_exists();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
