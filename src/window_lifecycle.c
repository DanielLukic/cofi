#include "window_lifecycle.h"

#include <gdk/gdkx.h>
#include <X11/Xatom.h>

#include "command_mode.h"
#include "config.h"
#include "display.h"
#include "filter.h"
#include "harpoon_config.h"
#include "history.h"
#include "log.h"
#include "named_window.h"
#include "overlay_manager.h"
#include "run_mode.h"
#include "selection.h"
#include "tab_switching.h"
#include "window_highlight.h"
#include "window_list.h"
#include "workspace_utils.h"
#include "x11_utils.h"

static gboolean grab_focus_delayed(gpointer data);

gboolean on_delete_event(GtkWidget *widget, GdkEvent *event, AppData *app) {
    (void)widget;
    (void)event;
    hide_window(app);
    return TRUE;
}

gboolean on_focus_out_event(GtkWidget *widget, GdkEventFocus *event, AppData *app) {
    (void)widget;
    (void)event;

    if (app->pending_hotkey_mode < 0) {
        if (app->command_mode.state == CMD_MODE_COMMAND) {
            log_debug("Resetting command mode due to focus loss");
            exit_command_mode(app);
        } else if (app->command_mode.state == CMD_MODE_RUN) {
            log_debug("Resetting run mode due to focus loss");
            exit_run_mode(app);
        }
    }

    if (!app->config.close_on_focus_loss) {
        return FALSE;
    }

    if (app->focus_loss_timer > 0) {
        g_source_remove(app->focus_loss_timer);
    }
    app->focus_loss_timer = g_timeout_add(100, (GSourceFunc)check_focus_loss_delayed, app);

    return FALSE;
}

gboolean check_focus_loss_delayed(AppData *app) {
    app->focus_loss_timer = 0;

    if (!app->window || !app->window_visible) {
        return FALSE;
    }

    if (gtk_window_has_toplevel_focus(GTK_WINDOW(app->window))) {
        return FALSE;
    }

    log_info("Window lost focus to external application, closing");
    hide_window(app);
    return FALSE;
}

void destroy_window(AppData *app) {
    if (app->window) {
        save_config(&app->config);
        save_harpoon_slots(&app->harpoon);

        gtk_widget_destroy(app->window);
        app->window = NULL;
        app->entry = NULL;
        app->mode_indicator = NULL;
        app->textview = NULL;
        app->scrolled = NULL;
        app->textbuffer = NULL;

        app->command_mode.state = CMD_MODE_NORMAL;
        app->command_mode.showing_help = FALSE;
        app->command_mode.command_buffer[0] = '\0';
        app->command_mode.cursor_pos = 0;
        app->command_mode.history_index = -1;
        app->run_mode.history_index = -1;
        app->run_mode.close_on_exit = FALSE;
        app->run_mode.suppress_entry_change = FALSE;

        reset_selection(app);
        log_debug("Selection reset to 0 in destroy_window");
    }
}

void hide_window(AppData *app) {
    if (!app->window || !app->window_visible) {
        return;
    }

    if (app->no_daemon) {
        gtk_main_quit();
        return;
    }

    log_debug("Hiding window without destroying");

    if (app->entry) {
        gtk_entry_set_text(GTK_ENTRY(app->entry), "");
    }

    app->selection.window_scroll_offset = 0;
    app->selection.workspace_scroll_offset = 0;
    app->selection.harpoon_scroll_offset = 0;

    if (app->command_mode.state == CMD_MODE_COMMAND) {
        exit_command_mode(app);
    } else if (app->command_mode.state == CMD_MODE_RUN) {
        exit_run_mode(app);
    }

    if (app->mode_indicator) {
        gtk_label_set_text(GTK_LABEL(app->mode_indicator), ">");
    }

    app->current_tab = TAB_WINDOWS;

    if (app->overlay_active) {
        hide_overlay(app);
    }

    if (app->focus_loss_timer > 0) {
        g_source_remove(app->focus_loss_timer);
        app->focus_loss_timer = 0;
    }
    if (app->focus_grab_timer > 0) {
        g_source_remove(app->focus_grab_timer);
        app->focus_grab_timer = 0;
    }

    save_config(&app->config);
    save_harpoon_slots(&app->harpoon);

    gtk_widget_hide(app->window);
    app->window_visible = FALSE;

    log_debug("Window hidden, X11 event processing continues");
}

static gboolean grab_focus_delayed(gpointer data) {
    AppData *app = (AppData *)data;

    app->focus_grab_timer = 0;

    if (app->entry && app->window && app->window_visible) {
        GtkWindow *window = GTK_WINDOW(app->window);

        gtk_window_set_urgency_hint(window, FALSE);

        GdkWindow *gdk_window = gtk_widget_get_window(app->window);
        if (gdk_window) {
            Display *display = GDK_WINDOW_XDISPLAY(gdk_window);
            Window xwindow = GDK_WINDOW_XID(gdk_window);
            guint32 ts = app->focus_timestamp ? app->focus_timestamp : CurrentTime;

            XRaiseWindow(display, xwindow);
            gdk_window_focus(gdk_window, ts);
            XFlush(display);

            gdk_error_trap_push();
            XSetInputFocus(display, xwindow, RevertToParent, ts);
            XSync(display, False);
            int err = gdk_error_trap_pop();
            if (err != 0) {
                log_debug("grab_focus_delayed: XSetInputFocus failed (err=%d), retrying", err);
                app->focus_grab_timer = g_timeout_add(20, grab_focus_delayed, app);
                return FALSE;
            }
        }

        gtk_widget_grab_focus(app->entry);
    }

    return FALSE;
}

void ensure_cofi_on_current_workspace(AppData *app) {
    if (!app || !app->display || app->own_window_id == 0) {
        return;
    }

    int current_desktop = get_current_desktop(app->display);
    if (current_desktop < 0) {
        return;
    }

    int actual_format = 0;
    unsigned long n_items = 0;
    unsigned char *prop = NULL;

    if (get_x11_property(app->display, app->own_window_id, app->atoms.net_wm_desktop,
                         XA_CARDINAL, 1, NULL, &actual_format, &n_items, &prop) != COFI_SUCCESS) {
        return;
    }

    if (actual_format != 32 || n_items < 1 || !prop) {
        if (prop) XFree(prop);
        return;
    }

    long own_desktop = *(long *)prop;
    XFree(prop);

    if (own_desktop < 0 || own_desktop == 0xFFFFFFFF) {
        return;
    }

    if ((int)own_desktop != current_desktop) {
        move_window_to_desktop(app->display, app->own_window_id, current_desktop);
        log_debug("Moved cofi window to current desktop %d", current_desktop + 1);
    }
}

void show_window(AppData *app) {
    if (!app->window) {
        log_error("Cannot show window - window not created");
        return;
    }

    if (app->window_visible) {
        ensure_cofi_on_current_workspace(app);
        gtk_window_present(GTK_WINDOW(app->window));
        return;
    }

    log_debug("Showing window and refreshing state");

    if (app->mode_indicator) {
        const char *indicator = ">";
        if (app->command_mode.state == CMD_MODE_COMMAND) {
            indicator = ":";
        } else if (app->command_mode.state == CMD_MODE_RUN) {
            indicator = "!";
        }
        gtk_label_set_text(GTK_LABEL(app->mode_indicator), indicator);
    }

    get_window_list(app);
    check_and_reassign_windows(&app->harpoon, app->windows, app->window_count);

    if (app->current_tab == TAB_WINDOWS) {
        reset_selection(app);
        filter_windows(app, "");
    } else if (app->current_tab == TAB_WORKSPACES) {
        filter_workspaces(app, "");
    } else if (app->current_tab == TAB_HARPOON) {
        filter_harpoon(app, "");
    }

    gtk_widget_show_all(app->window);
    app->window_visible = TRUE;
    ensure_cofi_on_current_workspace(app);

    if (app->fixed_cols > 0 && app->fixed_rows > 0) {
        update_display(app);
    } else {
        app->pending_initial_render = TRUE;
    }

    GtkWindow *window = GTK_WINDOW(app->window);
    guint32 ts = app->focus_timestamp ? app->focus_timestamp : GDK_CURRENT_TIME;
    gtk_window_present_with_time(window, ts);
    gtk_window_set_urgency_hint(window, TRUE);
    gtk_widget_grab_focus(app->entry);

    if (app->focus_grab_timer > 0) {
        g_source_remove(app->focus_grab_timer);
    }
    app->focus_grab_timer = g_idle_add(grab_focus_delayed, app);

    log_debug("Window shown with multi-method focus grab");
}
