#ifndef COMMAND_HANDLERS_TILING_H
#define COMMAND_HANDLERS_TILING_H

#include <glib.h>

typedef struct AppData AppData;
typedef struct WindowInfo WindowInfo;

gboolean cmd_tile_window(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_mouse(AppData *app, WindowInfo *window, const char *args);

#endif
