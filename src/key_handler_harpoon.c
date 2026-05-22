#include "key_handler_harpoon.h"

#include "config.h"
#include "cofi_tab_provider.h"
#include "display.h"
#include "harpoon.h"
#include "harpoon_config.h"
#include "log.h"
#include "match_entry_config.h"
#include "matching_gc.h"
#include "selection.h"
#include "slot_store.h"
#include "window_highlight.h"
#include "window_lifecycle.h"
#include "workspace_slots.h"
#include "x11_events.h"
#include "x11_utils.h"

#include <string.h>

static int get_harpoon_slot(GdkEventKey *event, gboolean is_assignment) {
    if (event->keyval >= GDK_KEY_0 && event->keyval <= GDK_KEY_9) {
        return event->keyval - GDK_KEY_0;
    }

    if (event->keyval >= GDK_KEY_KP_0 && event->keyval <= GDK_KEY_KP_9) {
        return event->keyval - GDK_KEY_KP_0;
    }

    if (event->keyval >= GDK_KEY_a && event->keyval <= GDK_KEY_z) {
        if (is_assignment) {
            gboolean is_excluded_key = (event->keyval == GDK_KEY_j ||
                                        event->keyval == GDK_KEY_k ||
                                        event->keyval == GDK_KEY_u);
            if (is_excluded_key && !(event->state & GDK_SHIFT_MASK)) {
                return -1;
            }
        }
        return HARPOON_FIRST_LETTER + (event->keyval - GDK_KEY_a);
    }

    if (event->keyval >= GDK_KEY_A && event->keyval <= GDK_KEY_Z) {
        return HARPOON_FIRST_LETTER + (event->keyval - GDK_KEY_A);
    }

    return -1;
}

gboolean handle_harpoon_assignment(GdkEventKey *event, AppData *app) {
    if (!(event->state & GDK_CONTROL_MASK)) {
        return FALSE;
    }

    int slot = get_harpoon_slot(event, TRUE);
    if (slot < 0) {
        return FALSE;
    }

    const CofiTabProvider *provider = cofi_get_provider_for_tab(app->current_tab);
    if (provider && provider->slot_store_enabled && provider->slot_payload_for) {
        int provider_id = cofi_get_provider_id_for_tab(app->current_tab);
        int selected = get_selected_index(app);
        int raw = cofi_filtered_to_raw(provider_id, selected);
        if (raw < 0) raw = selected;
        const char *payload = provider->slot_payload_for(app, raw);
        if (!payload || payload[0] == '\0') return FALSE;
        char slot_key = slot_key_from_index(slot);
        const char *current = slot_lookup(&app->harpoon.store, provider->id, slot_key);
        if (current && strcmp(current, payload) == 0) {
            slot_clear(&app->harpoon.store, provider->id, slot_key);
        } else {
            char old_slot = slot_for_payload(&app->harpoon.store, provider->id, payload);
            if (old_slot != '\0') {
                slot_clear(&app->harpoon.store, provider->id, old_slot);
            }
            slot_assign(&app->harpoon.store, slot_key, provider->id, payload);
        }
        save_harpoon_slots(&app->harpoon);
        update_display(app);
        return TRUE;
    }

    if (app->current_tab != TAB_WINDOWS) {
        return FALSE;
    }

    if (app->filtered_count == 0) {
        return FALSE;
    }

    WindowInfo *selected_window = get_selected_window(app);
    if (!selected_window) {
        return FALSE;
    }

    Window current_window = get_slot_window(&app->harpoon, slot);
    int gc_removed = 0;
    gboolean matching_changed = FALSE;
    if (current_window == selected_window->id) {
        unassign_slot(&app->harpoon, slot);
        gc_removed = matching_run_gc(app);
        matching_changed = gc_removed > 0;
        log_info("Unassigned window '%s' from slot %d", selected_window->title, slot);
    } else {
        int old_slot = get_window_slot(&app->harpoon, selected_window->id);
        if (old_slot >= 0) {
            unassign_slot(&app->harpoon, old_slot);
        }
        assign_window_to_slot(&app->harpoon, slot, selected_window);
        gc_removed = matching_run_gc(app);
        matching_changed = TRUE;
        log_info("Assigned window '%s' to slot %d", selected_window->title, slot);
    }

    save_config(&app->config);
    if (matching_changed) {
        save_match_entries(&app->matching);
    }
    save_harpoon_slots(&app->harpoon);
    update_display(app);
    return TRUE;
}

gboolean handle_harpoon_workspace_switching(GdkEventKey *event, AppData *app) {
    if (!(event->state & GDK_MOD1_MASK)) {
        return FALSE;
    }

    int slot = get_harpoon_slot(event, FALSE);
    if (slot < 0) {
        return FALSE;
    }

    const CofiTabProvider *provider = cofi_get_provider_for_tab(app->current_tab);
    if (provider && provider->slot_store_enabled && provider->slot_recall) {
        const char *payload = slot_lookup(&app->harpoon.store, provider->id,
                                          slot_key_from_index(slot));
        if (!payload) {
            log_warn("No %s slot assigned for %c", provider->id,
                     slot_key_from_index(slot));
            return TRUE;
        }
        CofiActionStatus status = provider->slot_recall(app, payload);
        if (status == COFI_HANDLED_HIDE) hide_window(app);
        else if (status == COFI_ACTION_ERROR) update_display(app);
        return TRUE;
    }

    if (slot >= 1 && slot <= 9) {
        if (app->config.digit_slot_mode == DIGIT_MODE_WORKSPACES) {
            int workspace_num = slot - 1;
            int workspace_count = get_number_of_desktops(app->display);
            if (workspace_num >= 0 && workspace_num < workspace_count) {
                switch_to_desktop(app->display, workspace_num);
                hide_window(app);
                log_info("Quick workspace switch to %d", slot);
                return TRUE;
            }
            return FALSE;
        }

        if (app->config.digit_slot_mode == DIGIT_MODE_PER_WORKSPACE &&
            app->current_tab == TAB_WINDOWS) {
            log_debug("Alt+%d: per-workspace mode, assigning slots", slot);
            assign_workspace_slots(app);
            Window target = get_workspace_slot_window(&app->workspace_slots, slot);
            log_debug("Alt+%d: target=0x%lx, manager.count=%d", slot, target, app->workspace_slots.count);
            if (target != 0) {
                set_workspace_switch_state(1);
                activate_window(app->display, target);
                highlight_window(app, target);
                hide_window(app);
                log_info("Workspace slot %d -> window 0x%lx", slot, target);
                return TRUE;
            }
            return FALSE;
        }
    }

    if (app->current_tab == TAB_WINDOWS) {
        Window target_window = get_slot_window(&app->harpoon, slot);
        if (target_window != 0) {
            set_workspace_switch_state(1);
            activate_window(app->display, target_window);
            highlight_window(app, target_window);
            hide_window(app);
            log_info("Switched to harpooned window in slot %d", slot);
            return TRUE;
        }
    } else if (slot >= 1 && slot <= 9 && slot <= app->workspace_count) {
        switch_to_desktop(app->display, slot - 1);
        hide_window(app);
        log_info("Switched to workspace %d", slot);
        return TRUE;
    }

    return FALSE;
}
