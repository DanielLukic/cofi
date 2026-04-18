#include "hotkey_dispatch.h"

#include "command_mode.h"
#include "display.h"
#include "filter.h"
#include "key_handler.h"
#include "log.h"
#include "run_mode.h"
#include "selection.h"
#include "tab_switching.h"
#include "window_lifecycle.h"
#include "x11_utils.h"


void dispatch_hotkey_mode(AppData *app, ShowMode mode) {
    if (!app->window_visible) {
        switch (mode) {
            case SHOW_MODE_COMMAND:
                app->current_tab = TAB_WINDOWS;
                app->command_target_id = (Window)get_active_window_id(app->display);
                show_window(app);
                enter_command_mode(app);
                break;
            case SHOW_MODE_RUN:
                app->current_tab = TAB_WINDOWS;
                show_window(app);
                enter_run_mode(app, NULL);
                break;
            case SHOW_MODE_WORKSPACES:
                app->current_tab = TAB_WORKSPACES;
                show_window(app);
                break;
            default:
                app->current_tab = TAB_WINDOWS;
                show_window(app);
                break;
        }
        return;
    }

    if (app->focus_loss_timer > 0) {
        g_source_remove(app->focus_loss_timer);
        app->focus_loss_timer = 0;
    }

    ensure_cofi_on_current_workspace(app);

    if (app->command_mode.state == CMD_MODE_COMMAND) {
        exit_command_mode(app);
    } else if (app->command_mode.state == CMD_MODE_RUN) {
        exit_run_mode(app);
    }

    switch (mode) {
        case SHOW_MODE_WINDOWS:
            if (app->current_tab == TAB_WINDOWS) {
                move_selection_up(app);
                return;
            }
            app->current_tab = TAB_WINDOWS;
            gtk_entry_set_text(GTK_ENTRY(app->entry), "");
            reset_selection(app);
            filter_windows(app, "");
            update_display(app);
            gtk_widget_grab_focus(app->entry);
            break;

        case SHOW_MODE_WORKSPACES:
            if (app->current_tab == TAB_WORKSPACES) {
                return;
            }
            app->current_tab = TAB_WORKSPACES;
            gtk_entry_set_text(GTK_ENTRY(app->entry), "");
            filter_workspaces(app, "");
            update_display(app);
            gtk_widget_grab_focus(app->entry);
            break;

        case SHOW_MODE_COMMAND:
            if (app->command_mode.state == CMD_MODE_COMMAND) {
                return;
            }
            app->current_tab = TAB_WINDOWS;
            app->command_target_id = (Window)app->active_window_id;
            enter_command_mode(app);
            break;

        case SHOW_MODE_RUN:
            if (app->command_mode.state == CMD_MODE_RUN) {
                return;
            }
            app->current_tab = TAB_WINDOWS;
            enter_run_mode(app, NULL);
            break;

        default:
            break;
    }
}
