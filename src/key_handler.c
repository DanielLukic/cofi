#include "key_handler.h"

#include <string.h>

#include "command_mode.h"
#include "display.h"
#include "filter.h"
#include "filter_names.h"
#include "key_handler_harpoon.h"
#include "key_handler_tabs.h"
#include "log.h"
#include "overlay_manager.h"
#include "repeat_action.h"
#include "run_mode.h"
#include "selection.h"
#include "tab_switching.h"
#include "window_highlight.h"
#include "window_lifecycle.h"
#include "x11_events.h"
#include "x11_utils.h"

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
                    log_debug("USER: ENTER pressed -> Activating window '%s' (ID: 0x%lx)", win->title, win->id);
                    store_last_windows_query(app, gtk_entry_get_text(GTK_ENTRY(app->entry)));
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
        case GDK_KEY_Up: move_selection_up(app); return TRUE;
        case GDK_KEY_Down: move_selection_down(app); return TRUE;
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
    if (handle_harpoon_assignment(event, app) ||
        handle_harpoon_workspace_switching(event, app) ||
        handle_harpoon_tab_keys(event, app) ||
        handle_names_tab_keys(event, app) ||
        handle_config_tab_keys(event, app) ||
        handle_hotkeys_tab_keys(event, app)) {
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
    if (event->keyval == GDK_KEY_period && app->current_tab == TAB_WINDOWS &&
        strlen(gtk_entry_get_text(GTK_ENTRY(app->entry))) == 0) {
        log_debug("USER: '.' pressed with empty query -> repeat last action");
        handle_repeat_key(app);
        return TRUE;
    }
    return handle_navigation_keys(event, app);
}

void on_entry_changed(GtkEntry *entry, AppData *app) {
    if (app->command_mode.state == CMD_MODE_COMMAND) {
        command_update_candidates(&app->command_mode, gtk_entry_get_text(entry));
        update_display(app);
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
