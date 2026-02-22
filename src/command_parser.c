#include "command_parser.h"

#include <ctype.h>
#include <string.h>

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

static void split_compact_command(const char *token, char *cmd_out, char *arg_out,
                                  size_t cmd_size, size_t arg_size) {
    size_t len = strlen(token);

    if (len >= 3 && strncmp(token, "cw", 2) == 0 && isdigit((unsigned char)token[2])) {
        strncpy(cmd_out, "cw", cmd_size - 1);
        strncpy(arg_out, token + 2, arg_size - 1);
        return;
    }

    if (len >= 3 && strncmp(token, "jw", 2) == 0 && isdigit((unsigned char)token[2])) {
        strncpy(cmd_out, "jw", cmd_size - 1);
        strncpy(arg_out, token + 2, arg_size - 1);
        return;
    }

    if (len >= 4 && strncmp(token, "maw", 3) == 0 && isdigit((unsigned char)token[3])) {
        strncpy(cmd_out, "maw", cmd_size - 1);
        strncpy(arg_out, token + 3, arg_size - 1);
        return;
    }

    if (len >= 2 && token[0] == 'j' && isdigit((unsigned char)token[1])) {
        strncpy(cmd_out, "j", cmd_size - 1);
        strncpy(arg_out, token + 1, arg_size - 1);
        return;
    }

    if (len >= 3 && strncmp(token, "tw", 2) == 0 &&
        (isdigit((unsigned char)token[2]) || strchr("LRTBFClrtbfc", token[2]))) {
        strncpy(cmd_out, "tw", cmd_size - 1);
        strncpy(arg_out, token + 2, arg_size - 1);
        return;
    }

    if (len >= 3 && token[0] == 't' && strchr("lrtbcLRTBC", token[1]) && strchr("1234", token[2])) {
        strncpy(cmd_out, "t", cmd_size - 1);
        strncpy(arg_out, token + 1, arg_size - 1);
        return;
    }

    if (len >= 2 && token[0] == 't' &&
        (isdigit((unsigned char)token[1]) || strchr("LRTBFClrtbfc", token[1]))) {
        strncpy(cmd_out, "t", cmd_size - 1);
        strncpy(arg_out, token + 1, arg_size - 1);
        return;
    }

    if (len == 2 && token[0] == 'm' && strchr("ash", token[1])) {
        strncpy(cmd_out, "m", cmd_size - 1);
        strncpy(arg_out, token + 1, arg_size - 1);
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
