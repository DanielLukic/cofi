#ifndef COMMAND_PARSE_DEFS_H
#define COMMAND_PARSE_DEFS_H

typedef struct {
    const char *primary;
    const char *aliases[5];
    const char *compact_suffix;
} CommandParseDef;

extern const CommandParseDef COMMAND_PARSE_DEFS[];

#endif
