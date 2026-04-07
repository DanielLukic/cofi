#ifndef COMMAND_HANDLERS_UI_H
#define COMMAND_HANDLERS_UI_H

#include <glib.h>

#include "command_api.h"

typedef struct AppData AppData;
typedef struct WindowInfo WindowInfo;

gboolean cmd_show(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_show_config(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_set_config(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_hotkeys(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_help(AppData *app, WindowInfo *window, const char *args);
char *generate_command_help_text(HelpFormat format);

#endif
