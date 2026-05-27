#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "core/utils/utf8_columns.h"

// Copy of generate_scrollbar and overlay_scrollbar from display.c for isolated testing.
// When the real implementation changes, this test must be updated to match.

void generate_scrollbar(int total_items, int visible_items, int scroll_offset, char *scrollbar, int scrollbar_height) {
    if (!scrollbar || scrollbar_height <= 0) return;
    if (total_items <= visible_items) {
        for (int i = 0; i < scrollbar_height; i++) scrollbar[i] = ' ';
        scrollbar[scrollbar_height] = '\0';
        return;
    }
    double visible_ratio = (double)visible_items / total_items;
    double position_ratio = (double)scroll_offset / (total_items - visible_items);
    int thumb_size = (int)(visible_ratio * scrollbar_height);
    if (thumb_size < 1) thumb_size = 1;
    if (thumb_size > scrollbar_height) thumb_size = scrollbar_height;
    if (scrollbar_height > 1 && thumb_size == scrollbar_height) thumb_size = scrollbar_height - 1;
    int thumb_start = (int)(position_ratio * (scrollbar_height - thumb_size));
    if (thumb_start < 0) thumb_start = 0;
    if (thumb_start + thumb_size > scrollbar_height) thumb_start = scrollbar_height - thumb_size;
    for (int i = 0; i < scrollbar_height; i++)
        scrollbar[i] = (i >= thumb_start && i < thumb_start + thumb_size) ? '#' : '.';
    scrollbar[scrollbar_height] = '\0';
}

void overlay_scrollbar(GString *text, int total_items, int visible_items, int scroll_offset, int target_columns) {
    if (!text || text->len == 0 || total_items <= visible_items || target_columns <= 0) return;
    // Find max line width in display columns so scrollbar column is past all content
    int max_line_cols = 0;
    const char *scan = text->str;
    while (*scan) {
        const char *nl = strchr(scan, '\n');
        int len = nl ? (int)(nl - scan) : (int)strlen(scan);
        GString *line = g_string_new_len(scan, len);
        int cols = utf8_text_columns(line->str);
        g_string_free(line, TRUE);
        if (cols > max_line_cols) max_line_cols = cols;
        if (!nl) break;
        scan = nl + 1;
    }
    int content_columns = max_line_cols + 2;
    if (content_columns > target_columns) target_columns = content_columns;
    char sb[visible_items + 1];
    generate_scrollbar(total_items, visible_items, scroll_offset, sb, visible_items);
    GString *result = g_string_new(NULL);
    const char *p = text->str;
    int source_lines = 1;
    for (const char *scan = text->str; *scan; scan++) {
        if (*scan == '\n') source_lines++;
    }
    int lines_to_overlay = source_lines < visible_items ? source_lines : visible_items;
    for (int line = 0; line < lines_to_overlay; line++) {
        const char *nl = strchr(p, '\n');
        int line_len = nl ? (int)(nl - p) : (int)strlen(p);
        GString *line_text = g_string_new_len(p, line_len);
        utf8_clip_lines_to_columns(line_text, target_columns - 1);
        int clipped_cols = utf8_text_columns(line_text->str);
        g_string_append(result, line_text->str);
        for (int i = clipped_cols; i < target_columns - 1; i++) {
            g_string_append_c(result, ' ');
        }
        g_string_free(line_text, TRUE);
        g_string_append_c(result, sb[line]);
        g_string_append_c(result, '\n');
        p = nl ? nl + 1 : p + line_len;
    }
    if (*p) g_string_append(result, p);
    g_string_assign(text, result->str);
    g_string_free(result, TRUE);
}

static int pass = 0, fail = 0;

#define ASSERT(name, cond) do { \
    if (cond) { printf("  PASS: %s\n", name); pass++; } \
    else { printf("  FAIL: %s\n", name); fail++; } \
} while(0)

// Helper: check that every line in text has scrollbar at column target_columns-1
static int scrollbar_at_column(const char *text, int target_columns) {
    const char *p = text;
    int line = 0;
    while (*p) {
        const char *nl = strchr(p, '\n');
        if (!nl) break;
        int len = (int)(nl - p);
        if (len != target_columns) return 0; // line not padded correctly
        line++;
        p = nl + 1;
    }
    return line > 0;
}

// Helper: check scrollbar char is one of '#' or '.'
static int is_scrollbar_char(char c) {
    return c == '#' || c == '.';
}

static int scrollbar_column_is_aligned_utf8(const char *text) {
    const char *p = text;
    int expected = -1;
    int lines = 0;
    while (*p) {
        const char *nl = strchr(p, '\n');
        if (!nl) break;
        if (nl == p) {
            p = nl + 1;
            continue;
        }
        if (!is_scrollbar_char(*(nl - 1))) {
            p = nl + 1;
            continue;
        }

        GString *left = g_string_new_len(p, (gssize)((nl - 1) - p));
        int cols = utf8_text_columns(left->str);
        g_string_free(left, TRUE);
        if (expected < 0) {
            expected = cols;
        } else if (cols != expected) {
            return 0;
        }
        lines++;
        p = nl + 1;
    }
    return lines > 0;
}

static void test_generate_scrollbar(void) {
    printf("\n--- generate_scrollbar ---\n");

    char sb[10];
    generate_scrollbar(20, 5, 0, sb, 5);
    ASSERT("generates 5-char scrollbar", strlen(sb) == 5);
    ASSERT("contains # and/or .", sb[0] == '#' || sb[0] == '.');

    generate_scrollbar(5, 5, 0, sb, 5);
    ASSERT("no scroll needed -> all spaces", sb[0] == ' ' && sb[4] == ' ');

    generate_scrollbar(100, 10, 0, sb, 10);
    ASSERT("at top: thumb starts at 0", sb[0] == '#');

    generate_scrollbar(100, 10, 90, sb, 10);
    ASSERT("at bottom: thumb ends at last", sb[9] == '#');
}

static void test_overlay_pads_short_lines(void) {
    printf("\n--- overlay: pads short lines ---\n");

    GString *text = g_string_new("abc\ndef\n");
    // 20 total, 2 visible, scroll at 0, target 10 cols
    overlay_scrollbar(text, 20, 2, 0, 10);

    // Each line should be exactly 10 chars + \n
    ASSERT("line 1 padded to 10 chars", text->str[10] == '\n');
    ASSERT("line 2 padded to 10 chars", text->str[21] == '\n');
    ASSERT("scrollbar char at col 9 line 1", is_scrollbar_char(text->str[9]));
    ASSERT("scrollbar char at col 9 line 2", is_scrollbar_char(text->str[20]));
    ASSERT("padding spaces inserted", text->str[3] == ' ');

    g_string_free(text, TRUE);
}

static void test_overlay_expands_for_long_lines(void) {
    printf("\n--- overlay: expands target for long lines ---\n");

    // Lines are 15 and 14 chars, target is 8 — should expand to 17 (max_len + 2)
    GString *text = g_string_new("abcdefghijklmno\n12345678901234\n");
    overlay_scrollbar(text, 20, 2, 0, 8);

    // max line is 15, so target becomes 17 (15+2: space gap + scrollbar)
    ASSERT("line 1 is 17 chars wide", text->str[17] == '\n');
    ASSERT("scrollbar at col 16", is_scrollbar_char(text->str[16]));
    ASSERT("space gap before scrollbar", text->str[15] == ' ');
    ASSERT("content preserved", strncmp(text->str, "abcdefghijklmno", 15) == 0);

    g_string_free(text, TRUE);
}

static void test_overlay_mixed_lengths(void) {
    printf("\n--- overlay: mixed line lengths ---\n");

    // Longest line is 26 chars ("this is a longer line here"), target 15 → expands to 28
    GString *text = g_string_new("short\nthis is a longer line here\nmed\n");
    overlay_scrollbar(text, 30, 3, 0, 15);

    // max line is 26, so target becomes 28 (26+2)
    ASSERT("all lines same width", scrollbar_at_column(text->str, 28));

    // Check scrollbar is at column 14 for each line
    const char *p = text->str;
    for (int i = 0; i < 3; i++) {
        const char *nl = strchr(p, '\n');
        ASSERT("scrollbar char present", nl && is_scrollbar_char(*(nl - 1)));
        p = nl + 1;
    }

    g_string_free(text, TRUE);
}

static void test_overlay_no_scroll_needed(void) {
    printf("\n--- overlay: no scroll needed ---\n");

    GString *text = g_string_new("line1\nline2\n");
    char *before = g_strdup(text->str);
    overlay_scrollbar(text, 2, 5, 0, 10);

    ASSERT("text unchanged when total <= visible", strcmp(text->str, before) == 0);

    g_free(before);
    g_string_free(text, TRUE);
}

static void test_overlay_empty_text(void) {
    printf("\n--- overlay: empty text ---\n");

    GString *text = g_string_new("");
    overlay_scrollbar(text, 10, 5, 0, 10);
    ASSERT("empty text unchanged", text->len == 0);

    g_string_free(text, TRUE);
}

static void test_overlay_help_style(void) {
    printf("\n--- overlay: help-style variable lines ---\n");

    GString *text = g_string_new(
        "Available commands:\n"
        "  cw, change-workspace       - Move window\n"
        "  pw                         - Pull window\n"
        "  help                       - Show help\n"
    );
    overlay_scrollbar(text, 40, 4, 0, 50);

    ASSERT("all help lines same width", scrollbar_at_column(text->str, 50));

    g_string_free(text, TRUE);
}

static void test_overlay_content_wider_than_target(void) {
    printf("\n--- overlay: content wider than target expands ---\n");

    // Lines are 15 chars, target is 10 — should expand to 17 (content+2)
    GString *text = g_string_new("123456789012345\nabcdefghijklmno\n");
    overlay_scrollbar(text, 20, 2, 0, 10);

    // Should expand to content_width+2 = 17
    ASSERT("line 1 is 17 chars wide", text->str[17] == '\n');
    ASSERT("scrollbar at col 16", is_scrollbar_char(text->str[16]));
    ASSERT("content preserved", strncmp(text->str, "123456789012345", 15) == 0);

    g_string_free(text, TRUE);
}

static void test_overlay_preserves_content(void) {
    printf("\n--- overlay: preserves content ---\n");

    GString *text = g_string_new("hello world\ngoodbye now\n");
    overlay_scrollbar(text, 20, 2, 0, 20);

    ASSERT("line 1 starts with 'hello'", strncmp(text->str, "hello world", 11) == 0);
    // After "hello world" there should be padding spaces then scrollbar
    ASSERT("padding after content", text->str[11] == ' ');

    g_string_free(text, TRUE);
}

static void test_overlay_utf8_aligns_scrollbar_columns(void) {
    printf("\n--- overlay: utf8 column alignment ---\n");
    GString *text = g_string_new(
        "Cafe 日本 👍🏽\n"
        "ASCII only line\n"
        "☺️ smile VS16\n"
        "ZWJ 👨‍💻 dev\n");

    overlay_scrollbar(text, 40, 4, 0, 20);
    ASSERT("utf8 scrollbar column aligned by display width",
           scrollbar_column_is_aligned_utf8(text->str));
    g_string_free(text, TRUE);
}

static void test_overlay_utf8_does_not_split_clusters(void) {
    printf("\n--- overlay: utf8 cluster-safe clipping ---\n");
    GString *text = g_string_new(
        "prefix 👨‍💻👨‍💻👨‍💻 long line\n"
        "prefix ☺️☺️☺️ long line\n");

    overlay_scrollbar(text, 20, 2, 0, 8);
    ASSERT("result remains valid UTF-8", g_utf8_validate(text->str, -1, NULL));
    g_string_free(text, TRUE);
}

static void test_overlay_preserves_trailing_empty_line(void) {
    printf("\n--- overlay: preserves trailing empty line ---\n");
    GString *text = g_string_new("header\n\n");
    overlay_scrollbar(text, 20, 2, 0, 10);

    int newlines = 0;
    for (const char *p = text->str; *p; p++) {
        if (*p == '\n') {
            newlines++;
        }
    }

    ASSERT("two rendered rows preserved", newlines >= 2);
    ASSERT("blank second row still has scrollbar width",
           strlen(text->str) >= 22 && text->str[21] == '\n');
    g_string_free(text, TRUE);
}

int main(void) {
    printf("Scrollbar overlay tests\n");
    printf("=======================\n");

    test_generate_scrollbar();
    test_overlay_pads_short_lines();
    test_overlay_expands_for_long_lines();
    test_overlay_mixed_lengths();
    test_overlay_no_scroll_needed();
    test_overlay_empty_text();
    test_overlay_help_style();
    test_overlay_content_wider_than_target();
    test_overlay_preserves_content();
    test_overlay_utf8_aligns_scrollbar_columns();
    test_overlay_utf8_does_not_split_clusters();
    test_overlay_preserves_trailing_empty_line();

    printf("\n=== Summary: %d/%d passed ===\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
