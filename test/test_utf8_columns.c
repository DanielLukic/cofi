#include <stdio.h>
#include <string.h>

#include "core/utils/utf8_columns.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

static void test_clean_preserves_utf8(void) {
    char out[256];
    utf8_clean_text("  Café 日本 👍🏽 👨‍💻 🇩🇪\nnext\tline  ", out, sizeof(out));

    ASSERT_TRUE("clean preserves accented text", strstr(out, "Café") != NULL);
    ASSERT_TRUE("clean preserves CJK text", strstr(out, "日本") != NULL);
    ASSERT_TRUE("clean preserves skin tone emoji", strstr(out, "👍🏽") != NULL);
    ASSERT_TRUE("clean preserves zwj emoji", strstr(out, "👨‍💻") != NULL);
    ASSERT_TRUE("clean preserves flag emoji", strstr(out, "🇩🇪") != NULL);
    ASSERT_TRUE("clean collapses line and tab whitespace",
                strcmp(out, "Café 日本 👍🏽 👨‍💻 🇩🇪 next line") == 0);
}

static void test_fit_preserves_and_pads_columns(void) {
    char out[256];
    utf8_fit_columns("Café 👍🏽 日本", 12, out, sizeof(out));

    ASSERT_TRUE("fit preserves accented text", strstr(out, "Café") != NULL);
    ASSERT_TRUE("fit preserves emoji cluster", strstr(out, "👍🏽") != NULL);
    ASSERT_TRUE("fit preserves CJK cluster", strstr(out, "日本") != NULL);
}

static void test_fit_does_not_split_clusters(void) {
    char out[256];
    utf8_fit_columns("👨‍💻X", 2, out, sizeof(out));
    ASSERT_TRUE("zwj emoji kept whole at width 2", strcmp(out, "👨‍💻") == 0);

    utf8_fit_columns("👨‍💻X", 1, out, sizeof(out));
    ASSERT_TRUE("zwj emoji not split at width 1", strcmp(out, " ") == 0);

    utf8_fit_columns("🇩🇪X", 2, out, sizeof(out));
    ASSERT_TRUE("flag emoji kept whole at width 2", strcmp(out, "🇩🇪") == 0);

    utf8_fit_columns("👍🏽X", 2, out, sizeof(out));
    ASSERT_TRUE("skin tone emoji kept whole at width 2", strcmp(out, "👍🏽") == 0);
}

static void test_clip_lines_does_not_split_clusters(void) {
    GString *text = g_string_new("ab👨‍💻cd\n👍🏽xyz");
    utf8_clip_lines_to_columns(text, 4);

    ASSERT_TRUE("clip keeps zwj emoji whole", strcmp(text->str, "ab👨‍💻\n👍🏽xy") == 0);
    g_string_free(text, TRUE);
}

static void test_vs16_emoji_presentation_width(void) {
    ASSERT_TRUE("smiling face + VS16 is width 2", utf8_text_columns("☺️") == 2);
    ASSERT_TRUE("up arrow + VS16 is width 2", utf8_text_columns("⬆️") == 2);
}

static void test_bare_arrow_stays_current_width(void) {
    ASSERT_TRUE("bare up arrow width remains 1", utf8_text_columns("↑") == 1);
}

int main(void) {
    printf("UTF-8 column tests\n");
    printf("==================\n");

    test_clean_preserves_utf8();
    test_fit_preserves_and_pads_columns();
    test_fit_does_not_split_clusters();
    test_clip_lines_does_not_split_clusters();
    test_vs16_emoji_presentation_width();
    test_bare_arrow_stays_current_width();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
