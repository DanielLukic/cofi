#ifndef OVERLAY_CONFIG_H
#define OVERLAY_CONFIG_H

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "app_data.h"

void create_config_edit_overlay_content(GtkWidget *parent_container, AppData *app);
void create_provider_enablement_overlay_content(GtkWidget *parent_container, AppData *app);
gboolean handle_config_edit_key_press(AppData *app, GdkEventKey *event);
gboolean handle_provider_enablement_key_press(AppData *app, GdkEventKey *event);

#endif
