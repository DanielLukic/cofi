#ifndef COMMAND_DEFINITIONS_H
#define COMMAND_DEFINITIONS_H

#include "command_api.h"

typedef gboolean (*CommandHandler)(AppData *app, WindowInfo *window, const char *args);

typedef struct {
    const char *primary;
    const char *aliases[5];
    CommandHandler handler;
    const char *description;
    const char *help_format;
    int activates;
    int keeps_open_on_hotkey_auto;
} CommandDef;

/* Legacy/core command definitions. Provider-owned commands live on CofiTabProvider. */
extern const CommandDef COMMAND_DEFINITIONS[];

#endif // COMMAND_DEFINITIONS_H
