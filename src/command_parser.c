#include "command_parser.h"

#include <ctype.h>
#include <string.h>

// Compact form table: {primary, aliases[], suffix_chars}
// Kept in sync with command_definitions.h but without handler/function pointers
// so that command_parser.o can link independently (e.g. in tests).
typedef struct {
    const char *primary;
    const char *aliases[5];
    const char *suffix;
} CompactForm;

static const CompactForm COMPACT_FORMS[] = {
    { "cw",    {"change-workspace", NULL},          "0123456789hjkl" },
    { "jw",    {"jump-workspace", "j", NULL},       "0123456789hjkl" },
    { "maw",   {"move-all-to-workspace", NULL},     "0123456789hjkl" },
    { "mouse", {"m", "ma", "ms", "mh", NULL},       "ash" },
    { "tw",    {"tile-window", "t", NULL},           "0123456789LRTBFClrtbfc" },
    { NULL,    {NULL},                               NULL }
};

void trim_whitespace_in_place(char *text) {
    if (!text || text[0] == '\0') {
        return;
    }

    char *start = text;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }

    size_t len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1])) {
        text[--len] = '\0';
    }
}

// Data-driven compact command splitting.
// Iterates COMPACT_FORMS, tries primary + aliases, picks longest match.
static int is_exact_command(const char *token) {
    for (int i = 0; COMPACT_FORMS[i].primary; i++) {
        if (strcmp(token, COMPACT_FORMS[i].primary) == 0) return 1;
        for (int a = 0; a < 5 && COMPACT_FORMS[i].aliases[a]; a++) {
            if (strcmp(token, COMPACT_FORMS[i].aliases[a]) == 0) return 1;
        }
    }
    return 0;
}

static void split_compact_command(const char *token, char *cmd_out, char *arg_out,
                                  size_t cmd_size, size_t arg_size) {
    // Don't split if token is already an exact command name
    if (is_exact_command(token)) return;

    size_t token_len = strlen(token);
    const char *best_primary = NULL;
    size_t best_name_len = 0;

    for (int i = 0; COMPACT_FORMS[i].primary; i++) {
        const char *suffix = COMPACT_FORMS[i].suffix;

        const char *names[7];
        int n = 0;
        names[n++] = COMPACT_FORMS[i].primary;
        for (int a = 0; a < 5 && COMPACT_FORMS[i].aliases[a]; a++)
            names[n++] = COMPACT_FORMS[i].aliases[a];

        for (int j = 0; j < n; j++) {
            size_t name_len = strlen(names[j]);
            if (token_len > name_len &&
                strncmp(token, names[j], name_len) == 0 &&
                strchr(suffix, token[name_len]) &&
                name_len > best_name_len) {
                best_primary = COMPACT_FORMS[i].primary;
                best_name_len = name_len;
            }
        }
    }

    if (best_primary) {
        strncpy(cmd_out, best_primary, cmd_size - 1);
        strncpy(arg_out, token + best_name_len, arg_size - 1);
    }
}

gboolean parse_command_and_arg(const char *input, char *cmd_out, char *arg_out,
                               size_t cmd_size, size_t arg_size) {
    if (!input || !cmd_out || !arg_out || cmd_size == 0 || arg_size == 0) {
        return FALSE;
    }

    cmd_out[0] = '\0';
    arg_out[0] = '\0';

    char local[512] = {0};
    strncpy(local, input, sizeof(local) - 1);
    trim_whitespace_in_place(local);

    if (local[0] == '\0') {
        return TRUE;
    }

    char *cursor = local;
    while (*cursor && !isspace((unsigned char)*cursor)) {
        cursor++;
    }

    size_t token_len = (size_t)(cursor - local);
    char token[512] = {0};
    if (token_len >= sizeof(token)) {
        token_len = sizeof(token) - 1;
    }
    memcpy(token, local, token_len);
    token[token_len] = '\0';

    strncpy(cmd_out, token, cmd_size - 1);
    cmd_out[cmd_size - 1] = '\0';

    while (*cursor && isspace((unsigned char)*cursor)) {
        cursor++;
    }

    if (*cursor != '\0') {
        strncpy(arg_out, cursor, arg_size - 1);
        arg_out[arg_size - 1] = '\0';
        trim_whitespace_in_place(arg_out);
        return TRUE;
    }

    split_compact_command(token, cmd_out, arg_out, cmd_size, arg_size);
    cmd_out[cmd_size - 1] = '\0';
    arg_out[arg_size - 1] = '\0';
    return TRUE;
}
