#include "command_parser.h"
#include "command_api.h"
#include "command_parse_defs.h"

#include <ctype.h>
#include <string.h>

const CommandParseDef COMMAND_PARSE_DEFS[] = {
    { "ab",      {"always-below", NULL},                         NULL },
    { "an",      {"assign-name", "n", NULL},                   NULL },
    { "as",      {"assign-slots", NULL},                         NULL },
    { "aot",     {"at", "always-on-top", NULL},                NULL },
    { "cl",      {"c", "close", "close-window", NULL},       NULL },
    { "config",  {"conf", "cfg", NULL},                        NULL },
    { "cw",      {"change-workspace", NULL},                     "0123456789hjkl" },
    { "ew",      {"every-workspace", NULL},                      NULL },
    { "harpoon", {"hp", NULL},                                   NULL },
    { "help",    {"h", "?", NULL},                             NULL },
    { "hmw",     {"hm", "horizontal-maximize-window", NULL},   NULL },
    { "hotkeys", {"hotkey", "hk", NULL},                       NULL },
    { "jw",      {"jump-workspace", "j", NULL},                "0123456789hjkl" },
    { "maw",     {"move-all-to-workspace", NULL},                "0123456789hjkl" },
    { "names",   {"nm", NULL},                                   NULL },
    { "miw",     {"min", "minimize-window", NULL},             NULL },
    { "mouse",   {"m", "ma", "ms", "mh", NULL},            "ash" },
    { "mw",      {"max", "maximize-window", NULL},             NULL },
    { "pw",      {"pull-window", "p", NULL},                   NULL },
    { "rw",      {"rename-workspace", NULL},                     NULL },
    { "sb",      {"skip-taskbar", NULL},                         NULL },
    { "set",     {NULL},                                           NULL },
    { "show",    {"s", NULL},                                    NULL },
    { "sw",      {"swap-windows", NULL},                         NULL },
    { "tm",      {"toggle-monitor", NULL},                       NULL },
    { "tw",      {"tile-window", "t", NULL},                   "0123456789LRTBFClrtbfc" },
    { "vmw",     {"vm", "vertical-maximize-window", NULL},     NULL },
    { "workspaces", {"ws", NULL},                                NULL },
    { NULL,        {NULL},                                           NULL }
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

static int is_exact_command(const char *token) {
    for (int i = 0; COMMAND_PARSE_DEFS[i].primary; i++) {
        if (strcmp(token, COMMAND_PARSE_DEFS[i].primary) == 0) return 1;
        for (int a = 0; a < 5 && COMMAND_PARSE_DEFS[i].aliases[a]; a++) {
            if (strcmp(token, COMMAND_PARSE_DEFS[i].aliases[a]) == 0) return 1;
        }
    }
    return 0;
}

static void split_compact_command(const char *token, char *cmd_out, char *arg_out,
                                  size_t cmd_size, size_t arg_size) {
    if (is_exact_command(token)) {
        return;
    }

    size_t token_len = strlen(token);
    const char *best_primary = NULL;
    size_t best_name_len = 0;

    for (int i = 0; COMMAND_PARSE_DEFS[i].primary; i++) {
        const char *suffix = COMMAND_PARSE_DEFS[i].compact_suffix;
        if (!suffix) {
            continue;
        }

        const char *names[7];
        int count = 0;
        names[count++] = COMMAND_PARSE_DEFS[i].primary;
        for (int a = 0; a < 5 && COMMAND_PARSE_DEFS[i].aliases[a]; a++) {
            names[count++] = COMMAND_PARSE_DEFS[i].aliases[a];
        }

        for (int n = 0; n < count; n++) {
            size_t name_len = strlen(names[n]);
            if (token_len <= name_len) {
                continue;
            }
            if (strncmp(token, names[n], name_len) != 0) {
                continue;
            }
            if (!strchr(suffix, token[name_len])) {
                continue;
            }
            if (name_len > best_name_len) {
                best_primary = COMMAND_PARSE_DEFS[i].primary;
                best_name_len = name_len;
            }
        }
    }

    if (best_primary) {
        strncpy(cmd_out, best_primary, cmd_size - 1);
        strncpy(arg_out, token + best_name_len, arg_size - 1);
    }
}

gboolean resolve_command_primary(const char *cmd_name, char *primary_out, size_t primary_size) {
    if (!cmd_name || !primary_out || primary_size == 0) {
        return FALSE;
    }

    primary_out[0] = '\0';
    for (int i = 0; COMMAND_PARSE_DEFS[i].primary; i++) {
        if (strcmp(cmd_name, COMMAND_PARSE_DEFS[i].primary) == 0) {
            strncpy(primary_out, COMMAND_PARSE_DEFS[i].primary, primary_size - 1);
            return TRUE;
        }

        for (int a = 0; a < 5 && COMMAND_PARSE_DEFS[i].aliases[a]; a++) {
            if (strcmp(cmd_name, COMMAND_PARSE_DEFS[i].aliases[a]) == 0) {
                strncpy(primary_out, COMMAND_PARSE_DEFS[i].primary, primary_size - 1);
                return TRUE;
            }
        }
    }
    return FALSE;
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

gboolean parse_command_for_execution(const char *input, char *cmd_out, char *arg_out,
                                     size_t cmd_size, size_t arg_size) {
    char parsed_cmd[128] = {0};
    if (!parse_command_and_arg(input, parsed_cmd, arg_out, sizeof(parsed_cmd), arg_size)) {
        return FALSE;
    }

    if (parsed_cmd[0] == '\0') {
        cmd_out[0] = '\0';
        return TRUE;
    }

    if (!resolve_command_primary(parsed_cmd, cmd_out, cmd_size)) {
        strncpy(cmd_out, parsed_cmd, cmd_size - 1);
        cmd_out[cmd_size - 1] = '\0';
    }

    return TRUE;
}

gboolean next_command_segment(char **cursor, char *segment_out, size_t segment_size) {
    if (!cursor || !*cursor || !segment_out || segment_size == 0) {
        return FALSE;
    }

    char *start = *cursor;
    while (*start && (*start == ',' || isspace((unsigned char)*start))) {
        start++;
    }

    if (*start == '\0') {
        *cursor = start;
        return FALSE;
    }

    char *end = start;
    while (*end && *end != ',') {
        end++;
    }

    size_t len = (size_t)(end - start);
    if (len >= segment_size) {
        len = segment_size - 1;
    }

    memcpy(segment_out, start, len);
    segment_out[len] = '\0';
    trim_whitespace_in_place(segment_out);

    *cursor = (*end == ',') ? end + 1 : end;
    if (segment_out[0] == '\0') {
        return next_command_segment(cursor, segment_out, segment_size);
    }

    return TRUE;
}

gboolean visit_command_segments(const char *command,
                                CommandSegmentVisitor visitor,
                                void *user_data) {
    if (!command || !visitor) {
        return FALSE;
    }

    char local[512] = {0};
    strncpy(local, command, sizeof(local) - 1);
    trim_whitespace_in_place(local);
    if (local[0] == '\0') {
        return TRUE;
    }

    char *cursor = local;
    char segment[256] = {0};
    while (next_command_segment(&cursor, segment, sizeof(segment))) {
        if (!visitor(segment, user_data)) {
            return FALSE;
        }
    }

    return TRUE;
}

