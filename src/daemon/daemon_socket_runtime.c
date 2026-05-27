#include "daemon/daemon_socket_runtime.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <gdk/gdkx.h>
#include <X11/Xatom.h>

#include "ui/cofi_modal.h"
#include "providers/cofi_tab_provider.h"
#include "commands/command_handlers_ui.h"
#include "commands/command_mode.h"
#include "daemon/daemon_socket.h"
#include "ui/display.h"
#include "matching/filter.h"
#include "core/log/log.h"
#include "core/selection/selection.h"
#include "ui/tab_switching.h"
#include "ui/window_lifecycle.h"
#include "x11/x11_utils.h"

static gboolean process_daemon_socket_events(GIOChannel *source, GIOCondition condition,
                                             gpointer data);
static void show_tab_for_opcode(AppData *app, TabMode tab);
static const CofiTabProvider *provider_for_opcode(uint8_t opcode);
static void show_provider_for_opcode(AppData *app, uint8_t opcode);
static void refresh_focus_timestamp(AppData *app);
static void mark_window_user_time(AppData *app, guint32 ts);

#ifndef cofi_get_fresh_focus_timestamp
static guint32 cofi_get_fresh_focus_timestamp(AppData *app) {
    if (!app || !app->window) {
        return 0;
    }

    GdkWindow *gdk_window = gtk_widget_get_window(app->window);
    if (!gdk_window) {
        return 0;
    }

    guint32 ts = gdk_x11_get_server_time(gdk_window);
    return ts == GDK_CURRENT_TIME ? 0 : ts;
}
#endif

int daemon_socket_start_monitor(AppData *app) {
    if (!app || app->daemon_socket_fd < 0) {
        return -1;
    }

    if (daemon_socket_set_nonblocking(app->daemon_socket_fd) != 0) {
        return -1;
    }

    app->daemon_socket_channel = g_io_channel_unix_new(app->daemon_socket_fd);
    app->daemon_socket_watch_id = g_io_add_watch(
        app->daemon_socket_channel,
        (GIOCondition)(G_IO_IN | G_IO_HUP | G_IO_ERR),
        process_daemon_socket_events,
        app);

    return 0;
}

void daemon_socket_stop_monitor(AppData *app) {
    if (!app) {
        return;
    }

    if (app->daemon_socket_watch_id > 0) {
        g_source_remove(app->daemon_socket_watch_id);
        app->daemon_socket_watch_id = 0;
    }

    if (app->daemon_socket_channel) {
        g_io_channel_unref(app->daemon_socket_channel);
        app->daemon_socket_channel = NULL;
    }

    if (app->daemon_socket_fd >= 0) {
        close(app->daemon_socket_fd);
        app->daemon_socket_fd = -1;
    }

    if (app->daemon_socket_path[0] != '\0') {
        unlink(app->daemon_socket_path);
        app->daemon_socket_path[0] = '\0';
    }
}

static void reset_interaction_modes(AppData *app) {
    if (app->command_mode.state == CMD_MODE_COMMAND) {
        exit_command_mode(app);
    } else if (app->command_mode.state == CMD_MODE_MODAL) {
        cofi_exit_modal(app);
    }
}

static void show_tab_for_opcode(AppData *app, TabMode tab) {
    if (!app) {
        return;
    }

    reset_interaction_modes(app);

    app->apps_mode = APPS_MODE_DEFAULT;
    app->current_tab = TAB_WINDOWS;
    show_window(app);

    if (tab != TAB_WINDOWS) {
        surface_tab(app, tab);
    } else {
        gtk_entry_set_text(GTK_ENTRY(app->entry), "");
        reset_selection(app);
        filter_windows(app, "");
        update_display(app);
    }

    if (app->entry) {
        gtk_widget_grab_focus(app->entry);
    }
}

static const CofiTabProvider *provider_for_opcode(uint8_t opcode) {
    const CofiTabProvider *provider = cofi_get_provider_for_delegate_opcode(opcode);
    if (!provider) {
        log_warn("No enabled provider for delegate opcode %u (%s); ignoring delegate",
                 opcode, daemon_socket_opcode_name(opcode));
    }
    return provider;
}

static void show_provider_for_opcode(AppData *app, uint8_t opcode) {
    if (!app) {
        return;
    }

    const CofiTabProvider *provider = provider_for_opcode(opcode);
    if (!provider) {
        return;
    }

    show_tab_for_opcode(app, (TabMode)provider->tab_mode);
}

static void mark_window_user_time(AppData *app, guint32 ts) {
    if (!app || !app->display || !app->own_window_id || ts == 0) {
        return;
    }

    Atom prop = XInternAtom(app->display, "_NET_WM_USER_TIME", False);
    XChangeProperty(app->display, app->own_window_id, prop, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&ts, 1);
    XFlush(app->display);
}

static void refresh_focus_timestamp(AppData *app) {
    guint32 ts = cofi_get_fresh_focus_timestamp(app);
    if (ts != 0) {
        app->focus_timestamp = ts;
        mark_window_user_time(app, ts);
    }
}

void daemon_socket_dispatch_opcode(AppData *app, uint8_t opcode) {
    if (!daemon_socket_is_valid_opcode(opcode) || !app) {
        return;
    }

    refresh_focus_timestamp(app);

    switch (opcode) {
        case COFI_OPCODE_WINDOWS:
            show_tab_for_opcode(app, TAB_WINDOWS);
            break;
        case COFI_OPCODE_WORKSPACES:
            show_provider_for_opcode(app, opcode);
            break;
        case COFI_OPCODE_HARPOON:
            show_provider_for_opcode(app, opcode);
            break;
        case COFI_OPCODE_MATCHING:
            show_provider_for_opcode(app, opcode);
            break;
        case COFI_OPCODE_APPLICATIONS:
            show_provider_for_opcode(app, opcode);
            break;
        case COFI_OPCODE_COMMAND:
            app->current_tab = TAB_WINDOWS;
            app->command_target_id = (Window)get_active_window_id(app->display);
            show_window(app);
            enter_command_mode(app);
            break;
        case COFI_OPCODE_RUN: {
            const CofiTabProvider *provider = provider_for_opcode(opcode);
            if (!provider) {
                break;
            }
            app->current_tab = TAB_WINDOWS;
            show_window(app);
            if (app->command_mode.state == CMD_MODE_COMMAND)
                exit_command_mode(app);
            app->prefix_origin_tab = app->current_tab;
            app->active_prefix_claim = '!';
            cofi_enter_modal(app, provider);
            break;
        }
        default:
            break;
    }

    log_info("Delegated opcode handled: %u (%s)",
             opcode, daemon_socket_opcode_name(opcode));
}

void daemon_socket_dispatch_show_tab(AppData *app, const char *name) {
    if (!app || !name || name[0] == '\0') {
        return;
    }

    refresh_focus_timestamp(app);
    reset_interaction_modes(app);
    app->apps_mode = APPS_MODE_DEFAULT;
    app->current_tab = TAB_WINDOWS;
    show_window(app);
    cofi_surface_provider_command(app, name);

    if (app->entry) {
        gtk_widget_grab_focus(app->entry);
    }

    log_info("Delegated show-tab handled: %s", name);
}

static gboolean process_daemon_socket_events(GIOChannel *source, GIOCondition condition,
                                             gpointer data) {
    (void)source;

    AppData *app = (AppData *)data;
    if (!app) {
        return FALSE;
    }

    if (condition & (G_IO_HUP | G_IO_ERR)) {
        log_error("Daemon socket watcher hit error/hup");
        return TRUE;
    }

    while (1) {
        uint8_t opcode = 0;
        if (daemon_socket_accept_opcode(app->daemon_socket_fd, &opcode) == 0) {
            if (opcode == COFI_OPCODE_SHOW_TAB) {
                char tab_name[64] = {0};
                if (daemon_socket_accept_tab_name(app->daemon_socket_fd, tab_name, sizeof(tab_name)) == 0) {
                    daemon_socket_dispatch_show_tab(app, tab_name);
                    continue;
                }
                log_warn("Failed to read delegated show-tab payload: %s", strerror(errno));
                continue;
            }
            daemon_socket_dispatch_opcode(app, opcode);
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }

        log_warn("Failed to read delegated opcode: %s", strerror(errno));
        break;
    }

    return TRUE;
}
