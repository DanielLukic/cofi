#ifndef OVERLAY_NAME_H
#define OVERLAY_NAME_H

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "app_data.h"

void create_name_assign_overlay_content(GtkWidget *parent_container, AppData *app);
void create_name_edit_overlay_content(GtkWidget *parent_container, AppData *app);
void create_name_delete_overlay_content(GtkWidget *parent_container, AppData *app);

gboolean handle_name_assign_key_press(AppData *app, GdkEventKey *event);
gboolean handle_name_edit_key_press(AppData *app, GdkEventKey *event);
gboolean handle_name_delete_key_press(AppData *app, GdkEventKey *event);

void focus_name_entry_delayed(AppData *app);

#endif
