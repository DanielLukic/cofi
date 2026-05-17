#include "command_parse_defs.h"

#include <stddef.h>

const CommandParseDef COMMAND_PARSE_DEFS[] = {
    { "ab",      {"always-below", NULL},                         "+-", COMMAND_OWNER_CORE },
    { "an",      {"assign-name", "n", NULL},                   NULL, COMMAND_OWNER_CORE },
    { "as",      {"assign-slots", NULL},                         NULL, COMMAND_OWNER_CORE },
    { "aot",     {"at", "always-on-top", NULL},                "+-", COMMAND_OWNER_CORE },
    { "cl",      {"c", "close", "close-window", NULL},       NULL, COMMAND_OWNER_CORE },
    { "cw",      {"change-workspace", NULL},                     "0123456789hjkl", COMMAND_OWNER_CORE },
    { "ew",      {"every-workspace", NULL},                      "+-", COMMAND_OWNER_CORE },
    { "help",    {"h", "?", NULL},                             NULL, COMMAND_OWNER_CORE },
    { "hmw",     {"hm", "horizontal-maximize-window", NULL},   NULL, COMMAND_OWNER_CORE },
    { "jw",      {"jump-workspace", "j", NULL},                "0123456789hjkl", COMMAND_OWNER_CORE },
    { "jump-slot", {"js", NULL},                                "123456789", COMMAND_OWNER_CORE },
    { "maw",     {"move-all-to-workspace", NULL},                "0123456789hjkl", COMMAND_OWNER_CORE },
    { "miw",     {"min", "minimize-window", NULL},             NULL, COMMAND_OWNER_CORE },
    { "mouse",   {"m", "ma", "ms", "mh", NULL},            "ash", COMMAND_OWNER_CORE },
    { "mw",      {"max", "maximize-window", NULL},             NULL, COMMAND_OWNER_CORE },
    { "pw",      {"pull-window", "p", NULL},                   NULL, COMMAND_OWNER_CORE },
    { "rw",      {"rename-workspace", NULL},                     NULL, COMMAND_OWNER_CORE },
    { "sb",      {"skip-taskbar", NULL},                         "+-", COMMAND_OWNER_CORE },
    { "set",     {NULL},                                           NULL, COMMAND_OWNER_CORE },
    { "show",    {"s", NULL},                                    NULL, COMMAND_OWNER_CORE },
    { "sw",      {"swap-windows", NULL},                         NULL, COMMAND_OWNER_CORE },
    { "tm",      {"toggle-monitor", NULL},                       NULL, COMMAND_OWNER_CORE },
    { "tw",      {"tile-window", "t", NULL},                   "0123456789LRTBFClrtbfc", COMMAND_OWNER_CORE },
    { "vmw",     {"vm", "vertical-maximize-window", NULL},     NULL, COMMAND_OWNER_CORE },
    { NULL,        {NULL},                                           NULL, NULL }
};
