#ifndef KEY_HANDLER_H
#define KEY_HANDLER_H

#include <gtk/gtk.h>

#include "app_data.h"

gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, AppData *app);
void on_entry_changed(GtkEntry *entry, AppData *app);
gboolean handle_navigation_keys(GdkEventKey *event, AppData *app);
gboolean handle_harpoon_tab_keys(GdkEventKey *event, AppData *app);
gboolean handle_names_tab_keys(GdkEventKey *event, AppData *app);
gboolean handle_config_tab_keys(GdkEventKey *event, AppData *app);
gboolean handle_hotkeys_tab_keys(GdkEventKey *event, AppData *app);
gboolean handle_harpoon_assignment(GdkEventKey *event, AppData *app);
gboolean handle_harpoon_workspace_switching(GdkEventKey *event, AppData *app);

#endif
