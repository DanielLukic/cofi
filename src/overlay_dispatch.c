#include "overlay_dispatch.h"

#include "log.h"
#include "overlay_config.h"
#include "overlay_harpoon.h"
#include "overlay_hotkey_add.h"
#include "overlay_hotkey_edit.h"
#include "overlay_manager.h"
#include "overlay_name.h"
#include "overlay_workspace.h"
#include "tiling_overlay.h"
#include "workspace_overlay.h"

void overlay_create_content(AppData *app, OverlayType type, gpointer data) {
    switch (type) {
        case OVERLAY_TILING:
            create_tiling_overlay_content(app->dialog_container, app);
            return;
        case OVERLAY_WORKSPACE_MOVE:
            create_workspace_move_overlay_content(app->dialog_container, app);
            return;
        case OVERLAY_WORKSPACE_JUMP:
            create_workspace_jump_overlay_content(app->dialog_container, app);
            return;
        case OVERLAY_WORKSPACE_MOVE_ALL:
            create_workspace_move_all_overlay_content(app->dialog_container, app);
            return;
        case OVERLAY_WORKSPACE_RENAME:
            create_workspace_rename_overlay_content(
                app->dialog_container, app, GPOINTER_TO_INT(data));
            return;
        case OVERLAY_HARPOON_DELETE:
            create_harpoon_delete_overlay_content(
                app->dialog_container, app, app->harpoon_delete.delete_slot);
            return;
        case OVERLAY_HARPOON_EDIT:
            create_harpoon_edit_overlay_content(
                app->dialog_container, app, app->harpoon_edit.editing_slot);
            return;
        case OVERLAY_NAME_ASSIGN:
            create_name_assign_overlay_content(app->dialog_container, app);
            return;
        case OVERLAY_NAME_EDIT:
            create_name_edit_overlay_content(app->dialog_container, app);
            return;
        case OVERLAY_CONFIG_EDIT:
            create_config_edit_overlay_content(app->dialog_container, app);
            return;
        case OVERLAY_HOTKEY_ADD:
            create_hotkey_add_overlay_content(app->dialog_container, app);
            return;
        case OVERLAY_HOTKEY_EDIT:
            create_hotkey_edit_overlay_content(app->dialog_container, app);
            return;
        case OVERLAY_NONE:
        default:
            log_error("Invalid overlay type: %d", type);
            return;
    }
}

gboolean overlay_dispatch_key_press(AppData *app, GdkEventKey *event) {
    switch (app->current_overlay) {
        case OVERLAY_TILING:
            return handle_tiling_overlay_key_press(app, event);
        case OVERLAY_WORKSPACE_MOVE:
            return handle_workspace_move_key_press(app, event);
        case OVERLAY_WORKSPACE_JUMP:
            return handle_workspace_jump_key_press(app, event);
        case OVERLAY_WORKSPACE_MOVE_ALL:
            return handle_workspace_move_all_key_press(app, event);
        case OVERLAY_WORKSPACE_RENAME:
            return handle_workspace_rename_key_press(app, event->keyval);
        case OVERLAY_HARPOON_DELETE:
            return handle_harpoon_delete_key_press(app, event);
        case OVERLAY_HARPOON_EDIT:
            return handle_harpoon_edit_key_press(app, event);
        case OVERLAY_NAME_ASSIGN:
            return handle_name_assign_key_press(app, event);
        case OVERLAY_NAME_EDIT:
            return handle_name_edit_key_press(app, event);
        case OVERLAY_CONFIG_EDIT:
            return handle_config_edit_key_press(app, event);
        case OVERLAY_HOTKEY_ADD:
            return handle_hotkey_add_key_press(app, event);
        case OVERLAY_HOTKEY_EDIT:
            return handle_hotkey_edit_key_press(app, event);
        case OVERLAY_NONE:
        default:
            return FALSE;
    }
}

void show_tiling_overlay(AppData *app) {
    show_overlay(app, OVERLAY_TILING, NULL);
}

void show_workspace_move_overlay(AppData *app) {
    show_overlay(app, OVERLAY_WORKSPACE_MOVE, NULL);
}

void show_workspace_jump_overlay(AppData *app) {
    show_overlay(app, OVERLAY_WORKSPACE_JUMP, NULL);
}

void show_workspace_move_all_overlay(AppData *app) {
    show_overlay(app, OVERLAY_WORKSPACE_MOVE_ALL, NULL);
}

void show_workspace_rename_overlay(AppData *app, int workspace_index) {
    show_overlay(app, OVERLAY_WORKSPACE_RENAME, GINT_TO_POINTER(workspace_index));
}

void show_harpoon_delete_overlay(AppData *app, int slot_index) {
    app->harpoon_delete.pending_delete = TRUE;
    app->harpoon_delete.delete_slot = slot_index;
    show_overlay(app, OVERLAY_HARPOON_DELETE, NULL);
}

void show_harpoon_edit_overlay(AppData *app, int slot_index) {
    app->harpoon_edit.editing = TRUE;
    app->harpoon_edit.editing_slot = slot_index;
    show_overlay(app, OVERLAY_HARPOON_EDIT, NULL);
}

void show_name_assign_overlay(AppData *app) {
    show_overlay(app, OVERLAY_NAME_ASSIGN, NULL);
}

void show_name_edit_overlay(AppData *app) {
    show_overlay(app, OVERLAY_NAME_EDIT, NULL);
}
