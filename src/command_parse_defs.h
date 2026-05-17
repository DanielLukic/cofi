#ifndef COMMAND_PARSE_DEFS_H
#define COMMAND_PARSE_DEFS_H

typedef struct {
    const char *primary;
    const char *aliases[5];
    const char *compact_suffix;
    const char *provider_command;
} CommandParseDef;

extern const CommandParseDef COMMAND_PARSE_DEFS[];

#endif
