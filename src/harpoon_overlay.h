#ifndef HARPOON_OVERLAY_H
#define HARPOON_OVERLAY_H

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "app_data.h"

// Harpoon overlay content creation
void create_harpoon_delete_overlay_content(GtkWidget *parent_container, AppData *app, int slot_index);
void create_harpoon_edit_overlay_content(GtkWidget *parent_container, AppData *app, int slot_index);

// Harpoon overlay key press handling
gboolean handle_harpoon_delete_key_press(AppData *app, GdkEventKey *event);
gboolean handle_harpoon_edit_key_press(AppData *app, GdkEventKey *event);

// Utility functions
void focus_harpoon_edit_entry_delayed(AppData *app);

#endif // HARPOON_OVERLAY_H