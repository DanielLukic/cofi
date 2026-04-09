#include "hotkey_dispatch.h"

#include "command_mode.h"
#include "display.h"
#include "filter.h"
#include "key_handler.h"
#include "log.h"
#include "selection.h"
#include "tab_switching.h"
#include "window_lifecycle.h"


void dispatch_hotkey_mode(AppData *app, ShowMode mode) {
    if (!app->window_visible) {
        switch (mode) {
            case SHOW_MODE_COMMAND:
                app->current_tab = TAB_WINDOWS;
                show_window(app);
                enter_command_mode(app);  // EXPERIMENT: direct call, no timer
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
            enter_command_mode(app);
            break;

        default:
            break;
    }
}
