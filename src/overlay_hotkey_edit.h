#ifndef OVERLAY_HOTKEY_EDIT_H
#define OVERLAY_HOTKEY_EDIT_H

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "app_data.h"

void create_hotkey_edit_overlay_content(GtkWidget *parent_container, AppData *app);
gboolean handle_hotkey_edit_key_press(AppData *app, GdkEventKey *event);

#endif
