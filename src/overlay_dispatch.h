#ifndef OVERLAY_DISPATCH_H
#define OVERLAY_DISPATCH_H

#include <gtk/gtk.h>
#include "app_data.h"

void overlay_create_content(AppData *app, OverlayType type, gpointer data);
gboolean overlay_dispatch_key_press(AppData *app, GdkEventKey *event);

void show_tiling_overlay(AppData *app);
void show_workspace_move_overlay(AppData *app);
void show_workspace_jump_overlay(AppData *app);
void show_workspace_move_all_overlay(AppData *app);
void show_workspace_rename_overlay(AppData *app, int workspace_index);
void show_harpoon_delete_overlay(AppData *app, int slot_index);
void show_harpoon_edit_overlay(AppData *app, int slot_index);
void show_name_assign_overlay(AppData *app);
void show_name_edit_overlay(AppData *app);

#endif
