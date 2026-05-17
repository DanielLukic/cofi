#include "hotkey_dispatch.h"

#include "command_mode.h"
#include "display.h"
#include "filter.h"
#include "key_handler.h"
#include "log.h"
#include "cofi_modal.h"
#include "cofi_tab_provider.h"
#include "selection.h"
#include "tab_switching.h"
#include "window_lifecycle.h"
#include "x11_utils.h"

static gboolean provider_tab_for_hotkey(const char *provider_id, TabMode *tab_out) {
    if (!provider_id || !tab_out) return FALSE;

    int id = cofi_get_provider_id(provider_id);
    if (id < 0 || !cofi_provider_is_enabled(id)) {
        log_warn("Provider '%s' is disabled; ignoring hotkey", provider_id);
        return FALSE;
    }

    const CofiTabProvider *provider = cofi_get_provider(id);
    if (!provider) {
        log_warn("Provider '%s' is unavailable; ignoring hotkey", provider_id);
        return FALSE;
    }

    *tab_out = (TabMode)provider->tab_mode;
    return TRUE;
}

static const CofiTabProvider *run_provider_for_hotkey(void) {
    const CofiTabProvider *provider = cofi_get_provider_for_prefix('!');
    if (!provider) {
        log_warn("Run provider is disabled; ignoring run hotkey");
    }
    return provider;
}

void dispatch_hotkey_mode(AppData *app, ShowMode mode) {
    if (!app->window_visible) {
        switch (mode) {
            case SHOW_MODE_COMMAND:
                app->current_tab = TAB_WINDOWS;
                app->command_target_id = (Window)get_active_window_id(app->display);
                show_window(app);
                enter_command_mode(app);
                break;
            case SHOW_MODE_RUN: {
                const CofiTabProvider *provider = run_provider_for_hotkey();
                if (!provider) {
                    return;
                }
                app->current_tab = TAB_WINDOWS;
                show_window(app);
                cofi_enter_modal(app, provider);
                break;
            }
            case SHOW_MODE_WORKSPACES: {
                TabMode workspaces_tab;
                if (!provider_tab_for_hotkey("workspaces", &workspaces_tab)) {
                    return;
                }
                app->current_tab = TAB_WINDOWS;
                show_window(app);
                surface_tab(app, workspaces_tab);
                break;
            }
            default:
                app->current_tab = TAB_WINDOWS;
                show_window(app);
                break;
        }
        return;
    }

    TabMode workspaces_tab = TAB_WINDOWS;
    const CofiTabProvider *run_provider = NULL;
    if (mode == SHOW_MODE_WORKSPACES &&
        !provider_tab_for_hotkey("workspaces", &workspaces_tab)) {
        return;
    }
    if (mode == SHOW_MODE_RUN) {
        run_provider = run_provider_for_hotkey();
        if (!run_provider) {
            return;
        }
    }

    if (app->focus_loss_timer > 0) {
        g_source_remove(app->focus_loss_timer);
        app->focus_loss_timer = 0;
    }

    ensure_cofi_on_current_workspace(app);

    if (app->command_mode.state == CMD_MODE_COMMAND) {
        exit_command_mode(app);
    } else if (app->command_mode.state == CMD_MODE_MODAL) {
        cofi_exit_modal(app);
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

        case SHOW_MODE_WORKSPACES: {
            if (app->current_tab == workspaces_tab) {
                return;
            }
            surface_tab(app, workspaces_tab);
            gtk_widget_grab_focus(app->entry);
            break;
        }

        case SHOW_MODE_COMMAND:
            if (app->command_mode.state == CMD_MODE_COMMAND) {
                return;
            }
            app->current_tab = TAB_WINDOWS;
            app->command_target_id = (Window)app->active_window_id;
            enter_command_mode(app);
            break;

        case SHOW_MODE_RUN: {
            if (app->command_mode.state == CMD_MODE_MODAL) {
                return;
            }
            app->current_tab = TAB_WINDOWS;
            cofi_enter_modal(app, run_provider);
            break;
        }

        default:
            break;
    }
}
