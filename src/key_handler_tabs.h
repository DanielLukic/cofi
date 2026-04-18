#ifndef KEY_HANDLER_TABS_H
#define KEY_HANDLER_TABS_H

#include <gtk/gtk.h>

#include "app_data.h"

gboolean handle_names_tab_keys(GdkEventKey *event, AppData *app);
gboolean handle_harpoon_tab_keys(GdkEventKey *event, AppData *app);
gboolean handle_config_tab_keys(GdkEventKey *event, AppData *app);
gboolean handle_hotkeys_tab_keys(GdkEventKey *event, AppData *app);

#endif
