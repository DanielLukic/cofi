#ifndef OVERLAY_NAMES_H
#define OVERLAY_NAMES_H

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "core/app/app_data.h"

void create_name_assign_overlay_content(GtkWidget *parent_container, AppData *app);
void create_name_edit_overlay_content(GtkWidget *parent_container, AppData *app);
gboolean handle_name_assign_key_press(AppData *app, GdkEventKey *event);
gboolean handle_name_edit_key_press(AppData *app, GdkEventKey *event);
void show_name_delete_confirm(AppData *app, const char *custom_name, int match_id);
void focus_name_entry_delayed(AppData *app);

#endif
