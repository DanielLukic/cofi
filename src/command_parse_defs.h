#ifndef COMMAND_PARSE_DEFS_H
#define COMMAND_PARSE_DEFS_H

#define COMMAND_OWNER_CORE "core"

typedef struct {
    const char *primary;
    const char *aliases[5];
    const char *compact_suffix;
    const char *owner_provider_id;
} CommandParseDef;

extern const CommandParseDef COMMAND_PARSE_DEFS[];

#endif
