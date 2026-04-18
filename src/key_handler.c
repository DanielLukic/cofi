#include "key_handler.h"

#include <string.h>

#include "command_mode.h"
#include "config.h"
#include "display.h"
#include "filter.h"
#include "repeat_action.h"
#include "filter_names.h"
#include "harpoon.h"
#include "harpoon_config.h"
#include "hotkey_config.h"
#include "hotkeys.h"
#include "log.h"
#include "named_window.h"
#include "named_window_config.h"
#include "overlay_manager.h"
#include "apps.h"
#include "run_mode.h"
#include "selection.h"
#include "tab_switching.h"
#include "window_lifecycle.h"
#include "workspace_info.h"
#include "workspace_slots.h"
#include "x11_events.h"
#include "x11_utils.h"

static int get_harpoon_slot(GdkEventKey *event, gboolean is_assignment, AppData *app) {
    (void)app;

    if (event->keyval >= GDK_KEY_0 && event->keyval <= GDK_KEY_9) {
        return event->keyval - GDK_KEY_0;
    } else if (event->keyval >= GDK_KEY_KP_0 && event->keyval <= GDK_KEY_KP_9) {
        return event->keyval - GDK_KEY_KP_0;
    } else if (event->keyval >= GDK_KEY_a && event->keyval <= GDK_KEY_z) {
        if (is_assignment) {
            gboolean is_excluded_key = (event->keyval == GDK_KEY_j ||
                                       event->keyval == GDK_KEY_k ||
                                       event->keyval == GDK_KEY_u);
            if (is_excluded_key && !(event->state & GDK_SHIFT_MASK)) {
                return -1;
            }
        }
        return HARPOON_FIRST_LETTER + (event->keyval - GDK_KEY_a);
    } else if (event->keyval >= GDK_KEY_A && event->keyval <= GDK_KEY_Z) {
        return HARPOON_FIRST_LETTER + (event->keyval - GDK_KEY_A);
    }

    return -1;
}

gboolean handle_harpoon_assignment(GdkEventKey *event, AppData *app) {
    if (!(event->state & GDK_CONTROL_MASK) || app->current_tab != TAB_WINDOWS) {
        return FALSE;
    }

    int slot = get_harpoon_slot(event, TRUE, app);
    if (slot < 0 || app->filtered_count == 0) {
        return FALSE;
    }

    WindowInfo *selected_window = get_selected_window(app);
    if (!selected_window) {
        return FALSE;
    }

    WindowInfo *win = selected_window;
    Window current_window = get_slot_window(&app->harpoon, slot);
    if (current_window == win->id) {
        unassign_slot(&app->harpoon, slot);
        log_info("Unassigned window '%s' from slot %d", win->title, slot);
    } else {
        int old_slot = get_window_slot(&app->harpoon, win->id);
        if (old_slot >= 0) {
            unassign_slot(&app->harpoon, old_slot);
        }
        assign_window_to_slot(&app->harpoon, slot, win);
        log_info("Assigned window '%s' to slot %d", win->title, slot);
    }

    save_config(&app->config);
    save_harpoon_slots(&app->harpoon);
    update_display(app);
    return TRUE;
}

gboolean handle_harpoon_workspace_switching(GdkEventKey *event, AppData *app) {
    if (!(event->state & GDK_MOD1_MASK)) {
        return FALSE;
    }

    int slot = get_harpoon_slot(event, FALSE, app);
    if (slot < 0) {
        return FALSE;
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
        } else if (app->config.digit_slot_mode == DIGIT_MODE_PER_WORKSPACE &&
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
    } else {
        if (slot >= 1 && slot <= 9 && slot <= app->workspace_count) {
            switch_to_desktop(app->display, slot - 1);
            hide_window(app);
            log_info("Switched to workspace %d", slot);
            return TRUE;
        }
    }

    return FALSE;
}

gboolean handle_names_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != TAB_NAMES) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_e && (event->state & GDK_CONTROL_MASK)) {
        if (app->selection.names_index < app->filtered_names_count) {
            show_name_edit_overlay(app);
            return TRUE;
        }
    }

    if (event->keyval == GDK_KEY_d && (event->state & GDK_CONTROL_MASK)) {
        if (app->selection.names_index < app->filtered_names_count) {
            NamedWindow *named = &app->filtered_names[app->selection.names_index];
            int manager_index = find_named_window_by_name(&app->names, named->custom_name);
            if (manager_index >= 0) {
                delete_custom_name(&app->names, manager_index);
                save_named_windows(&app->names);

                const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
                filter_names(app, current_filter);
                update_display(app);

                log_info("USER: Deleted custom name '%s'", named->custom_name);
            }
        }
        return TRUE;
    }

    return FALSE;
}

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

gboolean handle_config_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != TAB_CONFIG) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_t && (event->state & GDK_CONTROL_MASK)) {
        if (app->selection.config_index < app->filtered_config_count) {
            ConfigEntry *entry = &app->filtered_config[app->selection.config_index];
            const char *new_value = NULL;
            if (entry->type == CONFIG_TYPE_BOOL) {
                new_value = (strcmp(entry->value, "true") == 0) ? "false" : "true";
            } else if (entry->type == CONFIG_TYPE_ENUM) {
                new_value = get_next_enum_value(entry->key, entry->value);
            }
            if (new_value) {
                char err_buf[128];
                if (apply_config_setting(&app->config, entry->key, new_value, err_buf, sizeof(err_buf))) {
                    save_config(&app->config);
                    const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
                    filter_config(app, current_filter);
                    update_display(app);
                    log_info("USER: Cycled config '%s' to %s", entry->key, new_value);
                } else {
                    log_error("Failed to cycle config '%s': %s", entry->key, err_buf);
                }
                return TRUE;
            }
        }
    }

    if (event->keyval == GDK_KEY_e && (event->state & GDK_CONTROL_MASK)) {
        if (app->selection.config_index < app->filtered_config_count) {
            show_overlay(app, OVERLAY_CONFIG_EDIT, NULL);
            return TRUE;
        }
    }

    return FALSE;
}

gboolean handle_hotkeys_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != TAB_HOTKEYS) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_a && (event->state & GDK_CONTROL_MASK)) {
        cleanup_hotkeys(app);
        app->hotkey_capture_active = TRUE;
        show_overlay(app, OVERLAY_HOTKEY_ADD, NULL);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_d && (event->state & GDK_CONTROL_MASK)) {
        if (app->selection.hotkeys_index < app->filtered_hotkeys_count) {
            HotkeyBinding *binding = &app->filtered_hotkeys[app->selection.hotkeys_index];
            remove_hotkey_binding(&app->hotkey_config, binding->key);
            save_hotkey_config(&app->hotkey_config);
            regrab_hotkeys(app);

            const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
            filter_hotkeys(app, current_filter);

            if (app->selection.hotkeys_index >= app->filtered_hotkeys_count && app->filtered_hotkeys_count > 0) {
                app->selection.hotkeys_index = app->filtered_hotkeys_count - 1;
            }

            update_display(app);
            log_info("USER: Deleted hotkey binding '%s'", binding->key);
        }
        return TRUE;
    }

    if (event->keyval == GDK_KEY_e && (event->state & GDK_CONTROL_MASK)) {
        if (app->selection.hotkeys_index < app->filtered_hotkeys_count) {
            show_overlay(app, OVERLAY_HOTKEY_EDIT, NULL);
            return TRUE;
        }
    }

    return FALSE;
}

gboolean handle_navigation_keys(GdkEventKey *event, AppData *app) {
    switch (event->keyval) {
        case GDK_KEY_Escape:
            if (app->current_tab == TAB_HARPOON && app->harpoon_delete.pending_delete) {
                app->harpoon_delete.pending_delete = FALSE;
                log_info("Cancelled harpoon delete");
                update_display(app);
                return TRUE;
            }
            log_debug("USER: ESCAPE pressed -> Closing cofi");
            hide_window(app);
            return TRUE;

        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            if (app->current_tab == TAB_WINDOWS) {
                WindowInfo *win = get_selected_window(app);
                if (win) {
                    log_debug("USER: ENTER pressed -> Activating window '%s' (ID: 0x%lx)",
                             win->title, win->id);
                    store_last_windows_query(app,
                        gtk_entry_get_text(GTK_ENTRY(app->entry)));
                    set_workspace_switch_state(1);
                    activate_window(app->display, win->id);
                    highlight_window(app, win->id);
                    hide_window(app);
                }
            } else if (app->current_tab == TAB_APPS) {
                if (app->selection.apps_index < app->filtered_apps_count) {
                    AppEntry *entry = &app->filtered_apps[app->selection.apps_index];
                    log_info("USER: ENTER pressed -> Launching app '%s'", entry->name);
                    apps_launch(entry);
                    hide_window(app);
                }
            } else {
                WorkspaceInfo *ws = get_selected_workspace(app);
                if (ws) {
                    log_info("USER: ENTER pressed -> Switching to workspace %d: %s", ws->id, ws->name);
                    switch_to_desktop(app->display, ws->id);
                    hide_window(app);
                }
            }
            return TRUE;

        case GDK_KEY_Up:
            move_selection_up(app);
            return TRUE;

        case GDK_KEY_Down:
            move_selection_down(app);
            return TRUE;

        case GDK_KEY_k:
            if (event->state & GDK_CONTROL_MASK) {
                move_selection_up(app);
                return TRUE;
            }
            break;

        case GDK_KEY_j:
            if (event->state & GDK_CONTROL_MASK) {
                move_selection_down(app);
                return TRUE;
            }
            break;
    }

    return FALSE;
}

gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, AppData *app) {
    (void)widget;

    if (is_overlay_active(app)) {
        return handle_overlay_key_press(app, event);
    }

    if (app->command_mode.state == CMD_MODE_COMMAND) {
        if (handle_command_key(event, app)) {
            return TRUE;
        }
    } else if (app->command_mode.state == CMD_MODE_RUN) {
        if (handle_run_key(event, app)) {
            return TRUE;
        }
    }

    if (app->command_mode.state == CMD_MODE_NORMAL && event->keyval == GDK_KEY_colon) {
        log_debug("USER: ':' pressed -> Entering command mode");
        enter_command_mode(app);
        return TRUE;
    }

    if (app->command_mode.state == CMD_MODE_NORMAL && event->keyval == GDK_KEY_exclam) {
        log_debug("USER: '!' pressed -> Entering run mode");
        enter_run_mode(app, NULL);
        return TRUE;
    }

    if (handle_harpoon_assignment(event, app)) {
        return TRUE;
    }

    if (handle_harpoon_workspace_switching(event, app)) {
        return TRUE;
    }

    if (handle_harpoon_tab_keys(event, app)) {
        return TRUE;
    }

    if (handle_names_tab_keys(event, app)) {
        return TRUE;
    }

    if (handle_config_tab_keys(event, app)) {
        return TRUE;
    }

    if (handle_hotkeys_tab_keys(event, app)) {
        return TRUE;
    }

    if (app->current_tab == TAB_WINDOWS && (event->state & GDK_MOD1_MASK)) {
        if (event->keyval == GDK_KEY_Tab) {
            move_selection_up(app);
            return TRUE;
        }
        if (event->keyval == GDK_KEY_ISO_Left_Tab ||
            (event->keyval == GDK_KEY_Tab && (event->state & GDK_SHIFT_MASK))) {
            move_selection_down(app);
            return TRUE;
        }
    }

    if (handle_tab_switching(event, app)) {
        return TRUE;
    }

    if (event->keyval == GDK_KEY_period &&
        app->current_tab == TAB_WINDOWS &&
        strlen(gtk_entry_get_text(GTK_ENTRY(app->entry))) == 0) {
        log_debug("USER: '.' pressed with empty query -> repeat last action");
        handle_repeat_key(app);
        return TRUE;
    }

    if (handle_navigation_keys(event, app)) {
        return TRUE;
    }

    return FALSE;
}

void on_entry_changed(GtkEntry *entry, AppData *app) {
    if (app->command_mode.state == CMD_MODE_COMMAND) {
        return;
    }
    if (app->command_mode.state == CMD_MODE_RUN) {
        handle_run_entry_changed(entry, app);
        return;
    }

    const char *text = gtk_entry_get_text(entry);

    if (strlen(text) > 0) {
        log_debug("USER: Filter text changed -> '%s'", text);
    }

    if (app->current_tab == TAB_WINDOWS) {
        filter_windows(app, text);
    } else if (app->current_tab == TAB_WORKSPACES) {
        filter_workspaces(app, text);
    } else if (app->current_tab == TAB_HARPOON) {
        filter_harpoon(app, text);
    } else if (app->current_tab == TAB_NAMES) {
        filter_names(app, text);
    } else if (app->current_tab == TAB_CONFIG) {
        filter_config(app, text);
    } else if (app->current_tab == TAB_HOTKEYS) {
        filter_hotkeys(app, text);
    } else if (app->current_tab == TAB_APPS) {
        filter_apps(app, text);
    }

    reset_selection(app);
    update_display(app);
}
