#ifndef COMMAND_HANDLERS_WORKSPACE_H
#define COMMAND_HANDLERS_WORKSPACE_H

#include <glib.h>

typedef struct AppData AppData;
typedef struct WindowInfo WindowInfo;

gboolean cmd_change_workspace(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_jump_workspace(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_rename_workspace(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_move_all_to_workspace(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_assign_slots(AppData *app, WindowInfo *window, const char *args);

#endif
