#include "core/utils/utf8_columns.h"

#include <string.h>

static gboolean is_regional_indicator(gunichar ch) {
    return ch >= 0x1F1E6 && ch <= 0x1F1FF;
}

static gboolean is_variation_selector(gunichar ch) {
    return (ch >= 0xFE00 && ch <= 0xFE0F) ||
           (ch >= 0xE0100 && ch <= 0xE01EF);
}

static gboolean is_emoji_modifier(gunichar ch) {
    return ch >= 0x1F3FB && ch <= 0x1F3FF;
}

static gboolean is_combining(gunichar ch) {
    GUnicodeType type = g_unichar_type(ch);
    return g_unichar_combining_class(ch) != 0 ||
           type == G_UNICODE_NON_SPACING_MARK ||
           type == G_UNICODE_SPACING_MARK ||
           type == G_UNICODE_ENCLOSING_MARK;
}

static gboolean is_zero_width_extend(gunichar ch) {
    return ch == 0x200D || ch == 0x20E3 ||
           is_variation_selector(ch) ||
           is_emoji_modifier(ch) ||
           is_combining(ch);
}

static gboolean is_control_or_linebreak(gunichar ch) {
    return ch < 0x20 || ch == 0x7f ||
           ch == 0x2028 || ch == 0x2029;
}

static int codepoint_columns(gunichar ch) {
    if (is_zero_width_extend(ch)) {
        return 0;
    }
    return g_unichar_iswide(ch) ? 2 : 1;
}

const char *utf8_next_cluster(const char *p, const char *end) {
    if (!p || p >= end || *p == '\0') {
        return p;
    }

    gunichar first = g_utf8_get_char_validated(p, end - p);
    if (first == (gunichar)-1 || first == (gunichar)-2) {
        return p + 1;
    }

    const char *next = g_utf8_next_char(p);

    if (is_regional_indicator(first) && next < end) {
        gunichar second = g_utf8_get_char_validated(next, end - next);
        if (is_regional_indicator(second)) {
            return g_utf8_next_char(next);
        }
    }

    while (next < end && *next) {
        gunichar ch = g_utf8_get_char_validated(next, end - next);
        if (ch == (gunichar)-1 || ch == (gunichar)-2) {
            break;
        }

        if (is_zero_width_extend(ch)) {
            next = g_utf8_next_char(next);
            if (ch == 0x200D && next < end && *next) {
                gunichar joined = g_utf8_get_char_validated(next, end - next);
                if (joined != (gunichar)-1 && joined != (gunichar)-2) {
                    next = g_utf8_next_char(next);
                }
            }
            continue;
        }

        break;
    }

    return next;
}

int utf8_cluster_columns(const char *cluster, size_t len) {
    if (!cluster || len == 0) {
        return 0;
    }

    int cols = 0;
    const char *p = cluster;
    const char *end = cluster + len;
    while (p < end) {
        gunichar ch = g_utf8_get_char_validated(p, end - p);
        if (ch == (gunichar)-1 || ch == (gunichar)-2) {
            cols = MAX(cols, 1);
            p++;
            continue;
        }
        if (is_regional_indicator(ch)) {
            cols = MAX(cols, 2);
        }
        if (ch == 0xFE0F) {
            cols = MAX(cols, 2);
        }
        cols = MAX(cols, codepoint_columns(ch));
        p = g_utf8_next_char(p);
    }
    return cols > 0 ? cols : 1;
}

int utf8_text_columns(const char *text) {
    if (!text) {
        return 0;
    }

    int cols = 0;
    const char *p = text;
    const char *end = text + strlen(text);
    while (p < end && *p) {
        const char *next = utf8_next_cluster(p, end);
        cols += utf8_cluster_columns(p, (size_t)(next - p));
        p = next;
    }
    return cols;
}

void utf8_clean_text(const char *text, char *output, size_t output_size) {
    if (!output || output_size == 0) {
        return;
    }
    output[0] = '\0';
    if (!text) {
        return;
    }

    GString *clean = g_string_sized_new(strlen(text));
    gboolean last_was_space = FALSE;
    const char *p = text;
    const char *end = text + strlen(text);
    while (p < end && *p) {
        gunichar ch = g_utf8_get_char_validated(p, end - p);
        if (ch == (gunichar)-1 || ch == (gunichar)-2) {
            if (!last_was_space) {
                g_string_append_c(clean, ' ');
                last_was_space = TRUE;
            }
            p++;
            continue;
        }

        if (is_control_or_linebreak(ch) || g_unichar_isspace(ch)) {
            if (!last_was_space) {
                g_string_append_c(clean, ' ');
                last_was_space = TRUE;
            }
        } else {
            const char *next = g_utf8_next_char(p);
            g_string_append_len(clean, p, next - p);
            last_was_space = FALSE;
        }
        p = g_utf8_next_char(p);
    }

    char *trimmed = g_strstrip(clean->str);
    g_strlcpy(output, trimmed, output_size);
    g_string_free(clean, TRUE);
}

void utf8_fit_columns(const char *text, int width, char *output, size_t output_size) {
    if (!output || output_size == 0) {
        return;
    }
    output[0] = '\0';
    if (width <= 0) {
        return;
    }

    char clean_buffer[2048];
    utf8_clean_text(text, clean_buffer, sizeof(clean_buffer));

    GString *fitted = g_string_sized_new((gsize)width + 1);
    int cols = 0;
    const char *p = clean_buffer;
    const char *end = clean_buffer + strlen(clean_buffer);
    while (p < end && *p) {
        const char *next = utf8_next_cluster(p, end);
        int cluster_cols = utf8_cluster_columns(p, (size_t)(next - p));
        if (cols + cluster_cols > width) {
            break;
        }
        g_string_append_len(fitted, p, next - p);
        cols += cluster_cols;
        p = next;
    }

    while (cols < width) {
        g_string_append_c(fitted, ' ');
        cols++;
    }

    g_strlcpy(output, fitted->str, output_size);
    g_string_free(fitted, TRUE);
}

void utf8_fit_columns_aligned(const char *text, int width, int align_right,
                              char *output, size_t output_size) {
    if (!align_right) {
        utf8_fit_columns(text, width, output, output_size);
        return;
    }
    if (!output || output_size == 0) {
        return;
    }
    output[0] = '\0';
    if (width <= 0) {
        return;
    }

    char clean_buffer[2048];
    utf8_clean_text(text, clean_buffer, sizeof(clean_buffer));
    int clean_cols = utf8_text_columns(clean_buffer);
    if (clean_cols > width) {
        utf8_fit_columns(clean_buffer, width, output, output_size);
        return;
    }

    GString *fitted = g_string_sized_new((gsize)width + strlen(clean_buffer) + 1);
    for (int i = clean_cols; i < width; i++) {
        g_string_append_c(fitted, ' ');
    }
    g_string_append(fitted, clean_buffer);
    g_strlcpy(output, fitted->str, output_size);
    g_string_free(fitted, TRUE);
}

void utf8_fit_columns_ellipsis(const char *text, int width, char *output, size_t output_size) {
    if (!output || output_size == 0) {
        return;
    }
    output[0] = '\0';
    if (width <= 0) {
        return;
    }
    if (width <= 3) {
        memset(output, '.', MIN((size_t)width, output_size - 1));
        output[MIN((size_t)width, output_size - 1)] = '\0';
        return;
    }

    char clean_buffer[2048];
    utf8_clean_text(text, clean_buffer, sizeof(clean_buffer));

    int clean_cols = 0;
    const char *p = clean_buffer;
    const char *end = clean_buffer + strlen(clean_buffer);
    while (p < end && *p) {
        const char *next = utf8_next_cluster(p, end);
        clean_cols += utf8_cluster_columns(p, (size_t)(next - p));
        p = next;
    }
    if (clean_cols <= width) {
        utf8_fit_columns(clean_buffer, width, output, output_size);
        return;
    }

    char prefix[2048];
    utf8_fit_columns(clean_buffer, width - 3, prefix, sizeof(prefix));
    g_strchomp(prefix);
    g_snprintf(output, output_size, "%s...", prefix);
}

void utf8_clip_lines_to_columns(GString *text, int target_columns) {
    if (!text || target_columns <= 0) {
        return;
    }

    GString *clipped = g_string_sized_new(text->len);
    const char *p = text->str;
    while (*p) {
        const char *nl = strchr(p, '\n');
        const char *line_end = nl ? nl : p + strlen(p);
        int cols = 0;
        while (p < line_end && cols < target_columns) {
            const char *next = utf8_next_cluster(p, line_end);
            int cluster_cols = utf8_cluster_columns(p, (size_t)(next - p));
            if (cols + cluster_cols > target_columns) {
                break;
            }
            g_string_append_len(clipped, p, next - p);
            p = next;
            cols += cluster_cols;
        }
        p = line_end;
        if (nl) {
            g_string_append_c(clipped, '\n');
            p = nl + 1;
        }
    }
    g_string_assign(text, clipped->str);
    g_string_free(clipped, TRUE);
}
