#ifndef COMMAND_HANDLERS_LAYOUT_H
#define COMMAND_HANDLERS_LAYOUT_H

#include <glib.h>

typedef struct AppData AppData;
typedef struct WindowInfo WindowInfo;

gboolean cmd_layout(AppData *app, WindowInfo *window, const char *args);

#endif
