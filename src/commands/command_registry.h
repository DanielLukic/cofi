#ifndef COMMAND_REGISTRY_H
#define COMMAND_REGISTRY_H

#include "commands/command_api.h"

#define COMMAND_OWNER_CORE "core"

typedef gboolean (*CommandHandler)(AppData *app, WindowInfo *window,
                                   const char *args);

typedef struct {
    const char *primary;
    const char *aliases[5];
    const char *compact_suffix;
    const char *owner_provider_id;
    CommandHandler handler;
    const char *description;
    const char *help_format;
    int activates;
    int closes_cofi_after_execute;
    int keeps_open_on_hotkey_auto;
    int keeps_open_without_arg;
} CommandSpec;

int cofi_register_command(const CommandSpec *spec);
void cofi_command_registry_reset(void);

int cofi_command_count(void);
const CommandSpec *cofi_command_at(int index);
const CommandSpec *cofi_command_by_primary(const char *primary);
const CommandSpec *cofi_command_for_token(const char *token);

#endif /* COMMAND_REGISTRY_H */
