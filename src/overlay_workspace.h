#ifndef OVERLAY_WORKSPACE_H
#define OVERLAY_WORKSPACE_H

#include <gtk/gtk.h>
#include "app_data.h"

void create_workspace_rename_overlay_content(GtkWidget *parent_container,
                                             AppData *app,
                                             int workspace_index);
gboolean handle_workspace_rename_key_press(AppData *app, guint keyval);

#endif
