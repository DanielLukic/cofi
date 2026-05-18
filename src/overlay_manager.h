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
void show_workspace_rename_overlay(AppData *app, int workspace_index);
void show_workspace_move_all_overlay(AppData *app);
void show_harpoon_delete_overlay(AppData *app, int slot_index);
void show_harpoon_edit_overlay(AppData *app, int slot_index);
void show_name_assign_overlay(AppData *app);
void show_name_edit_overlay(AppData *app);
void show_name_delete_overlay(AppData *app, const char *custom_name, int manager_index);
void show_rule_add_overlay(AppData *app);
void show_rule_edit_overlay(AppData *app);
void show_rule_delete_overlay(AppData *app, int rule_index);
void show_project_kill_overlay(AppData *app, const char *session_name, ProjectBackend backend);
void show_project_rename_overlay(AppData *app, const char *session_name);
void show_project_new_overlay(AppData *app,
                              ProjectBackend backend,
                              const char *start_dir,
                              const char *initial_name);
void show_session_delete_overlay(AppData *app,
                                       const char *source,
                                       const char *session_id,
                                       const char *path);
void show_session_rename_overlay(AppData *app,
                                       const char *source,
                                       const char *session_id,
                                       const char *path,
                                       const char *current_name);

// Utility functions
static inline gboolean overlay_should_focus_name_entry(OverlayType type) {
    return type == OVERLAY_NAME_ASSIGN || type == OVERLAY_NAME_EDIT ||
           type == OVERLAY_CONFIG_EDIT || type == OVERLAY_HOTKEY_ADD ||
           type == OVERLAY_HOTKEY_REBIND ||
           type == OVERLAY_HOTKEY_EDIT || type == OVERLAY_RULE_ADD ||
           type == OVERLAY_RULE_EDIT || type == OVERLAY_PROJECT_RENAME ||
           type == OVERLAY_PROJECT_NEW ||
           type == OVERLAY_SESSION_RENAME;
}

#endif // OVERLAY_MANAGER_H
