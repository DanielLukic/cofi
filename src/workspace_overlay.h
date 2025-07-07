#ifndef WORKSPACE_OVERLAY_H
#define WORKSPACE_OVERLAY_H

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "app_data.h"

// Workspace overlay content creation
void create_workspace_jump_overlay_content(GtkWidget *parent_container, AppData *app);
void create_workspace_move_overlay_content(GtkWidget *parent_container, AppData *app);

// Workspace overlay key press handling
gboolean handle_workspace_jump_key_press(AppData *app, GdkEventKey *event);
gboolean handle_workspace_move_key_press(AppData *app, GdkEventKey *event);

#endif // WORKSPACE_OVERLAY_H