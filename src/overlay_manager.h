#ifndef OVERLAY_MANAGER_H
#define OVERLAY_MANAGER_H

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "app_data.h"

// Core overlay management functions
void init_overlay_system(AppData *app);
void show_overlay(AppData *app, OverlayType type, gpointer data);
void hide_overlay(AppData *app);
gboolean is_overlay_active(AppData *app);

// Event handling
gboolean handle_overlay_key_press(AppData *app, GdkEventKey *event);
gboolean on_modal_background_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data);

// Dialog-specific overlay functions
void show_tiling_overlay(AppData *app);
void show_workspace_move_overlay(AppData *app);
void show_workspace_jump_overlay(AppData *app);

// Note: Content creation functions are now static within overlay_manager.c
// following the new overlay pattern where content is added directly to parent_container

// Utility functions
void center_dialog_in_overlay(GtkWidget *dialog_content, AppData *app);

#endif // OVERLAY_MANAGER_H
