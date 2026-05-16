#include "key_handler_tabs.h"

#include "overlay_manager.h"

gboolean handle_harpoon_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != TAB_HARPOON) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_d && (event->state & GDK_CONTROL_MASK)) {
        if (app->selection.harpoon_index < app->filtered_harpoon_count) {
            HarpoonSlot *slot = &app->filtered_harpoon[app->selection.harpoon_index];
            if (slot->assigned) {
                int actual_slot = app->filtered_harpoon_indices[app->selection.harpoon_index];
                show_harpoon_delete_overlay(app, actual_slot);
                return TRUE;
            }
        }
    }

    if (event->keyval == GDK_KEY_e && (event->state & GDK_CONTROL_MASK)) {
        if (app->selection.harpoon_index < app->filtered_harpoon_count) {
            HarpoonSlot *slot = &app->filtered_harpoon[app->selection.harpoon_index];
            if (slot->assigned) {
                int actual_slot = app->filtered_harpoon_indices[app->selection.harpoon_index];
                show_harpoon_edit_overlay(app, actual_slot);
                return TRUE;
            }
        }
    }

    return FALSE;
}
