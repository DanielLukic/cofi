#ifndef COMMAND_HANDLERS_WINDOW_H
#define COMMAND_HANDLERS_WINDOW_H

#include <glib.h>

typedef struct AppData AppData;
typedef struct WindowInfo WindowInfo;

gboolean cmd_pull_window(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_close_window(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_minimize_window(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_maximize_window(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_horizontal_maximize(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_vertical_maximize(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_skip_taskbar(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_always_on_top(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_always_below(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_every_workspace(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_toggle_monitor(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_swap_windows(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_assign_name(AppData *app, WindowInfo *window, const char *args);

#endif
