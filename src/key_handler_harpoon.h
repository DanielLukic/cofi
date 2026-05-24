#ifndef KEY_HANDLER_HARPOON_H
#define KEY_HANDLER_HARPOON_H

#include <gtk/gtk.h>

#include "app_data.h"

gboolean handle_harpoon_assignment(GdkEventKey *event, AppData *app);
gboolean handle_harpoon_workspace_switching(GdkEventKey *event, AppData *app);
gboolean harpoon_assign_or_toggle_window(AppData *app, WindowInfo *selected_window, int slot);

#endif
