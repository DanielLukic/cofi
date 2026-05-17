#include "tab_header.h"

#include <stdio.h>
#include <string.h>

#include "tab_metadata.h"
#include "tab_switching.h"

#define TAB_TOKEN_LEN 32

static int header_token_len(char tokens[][TAB_TOKEN_LEN], int start, int end,
                            gboolean has_left, gboolean has_right) {
    int len = 2; /* left margin */
    if (has_left) len += 4;
    for (int i = start; i <= end; i++) {
        if (i > start) len += 4;
        len += (int)strlen(tokens[i]);
    }
    if (has_right) len += 4;
    return len;
}

static void append_header_slice(GString *output, char tokens[][TAB_TOKEN_LEN],
                                int start, int end, gboolean has_left,
                                gboolean has_right) {
    g_string_append(output, "  ");
    if (has_left) {
        g_string_append(output, "<   ");
    }
    for (int i = start; i <= end; i++) {
        if (i > start) {
            g_string_append(output, "    ");
        }
        g_string_append(output, tokens[i]);
    }
    if (has_right) {
        g_string_append(output, "   >");
    }
}

void tab_header_format(AppData *app, TabMode current_tab, int max_columns,
                       GString *output) {
    if (!output) return;

    g_string_append(output, "\n");

    char tokens[TAB_COUNT][TAB_TOKEN_LEN];
    int count = 0;
    int active = 0;

    for (int tab = TAB_WINDOWS; tab < TAB_COUNT; tab++) {
        if (!tab_is_visible(app, (TabMode)tab)) {
            continue;
        }
        if ((TabMode)tab == current_tab) {
            g_snprintf(tokens[count], sizeof(tokens[count]), "[ %s ]",
                       tab_active_name((TabMode)tab));
            active = count;
        } else {
            g_snprintf(tokens[count], sizeof(tokens[count]), "  %s  ",
                       tab_display_name((TabMode)tab));
        }
        count++;
    }

    if (count == 0) {
        g_string_append(output, "\n");
        return;
    }

    int budget = max_columns > 0 ? max_columns : 80;
    int full_len = header_token_len(tokens, 0, count - 1, FALSE, FALSE);
    if (full_len <= budget) {
        append_header_slice(output, tokens, 0, count - 1, FALSE, FALSE);
        g_string_append(output, "\n");
        return;
    }

    int start = active;
    int end = active;
    gboolean changed = TRUE;
    while (changed) {
        changed = FALSE;

        if (start > 0) {
            int next_start = start - 1;
            if (header_token_len(tokens, next_start, end,
                                 next_start > 0, end < count - 1) <= budget) {
                start = next_start;
                changed = TRUE;
            }
        }

        if (end < count - 1) {
            int next_end = end + 1;
            if (header_token_len(tokens, start, next_end,
                                 start > 0, next_end < count - 1) <= budget) {
                end = next_end;
                changed = TRUE;
            }
        }
    }

    append_header_slice(output, tokens, start, end, start > 0, end < count - 1);
    g_string_append(output, "\n");
}
