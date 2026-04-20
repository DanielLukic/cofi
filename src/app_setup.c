#include "app_setup.h"
#include "path_binaries.h"
#include "run_mode.h"

#include <gdk/gdkx.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(G_OS_UNIX)
#include <glib-unix.h>
#endif

#include "app_init.h"
#include "cli_args.h"
#include "command_mode.h"
#include "daemon_socket.h"
#include "daemon_socket_runtime.h"
#include "display.h"
#include "dynamic_display.h"
#include "gtk_window.h"
#include "harpoon_config.h"
#include "history.h"
#include "hotkeys.h"
#include "key_handler.h"
#include "log.h"
#include "overlay_manager.h"
#include "selection.h"
#include "version.h"
#include "window_highlight.h"
#include "window_list.h"
#include "window_lifecycle.h"
#include "workspace_slots.h"
#include "x11_events.h"
#include "x11_utils.h"

static char g_daemon_socket_cleanup_path[COFI_SOCKET_PATH_MAX] = {0};
static int g_daemon_socket_cleanup_armed = 0;
static int g_daemon_socket_cleanup_registered = 0;
static guint g_daemon_sigterm_source_id = 0;
static guint g_daemon_sigint_source_id = 0;

static void cleanup_daemon_socket_on_exit(void) {
    if (!g_daemon_socket_cleanup_armed || g_daemon_socket_cleanup_path[0] == '\0') {
        return;
    }

    unlink(g_daemon_socket_cleanup_path);
    g_daemon_socket_cleanup_path[0] = '\0';
    g_daemon_socket_cleanup_armed = 0;
}

static void arm_daemon_socket_exit_cleanup(const char *socket_path) {
    if (!socket_path || socket_path[0] == '\0') {
        return;
    }

    if (!g_daemon_socket_cleanup_registered) {
        atexit(cleanup_daemon_socket_on_exit);
        g_daemon_socket_cleanup_registered = 1;
    }

    strncpy(g_daemon_socket_cleanup_path, socket_path,
            sizeof(g_daemon_socket_cleanup_path) - 1);
    g_daemon_socket_cleanup_path[sizeof(g_daemon_socket_cleanup_path) - 1] = '\0';
    g_daemon_socket_cleanup_armed = 1;
}

static void disarm_daemon_socket_exit_cleanup(void) {
    g_daemon_socket_cleanup_path[0] = '\0';
    g_daemon_socket_cleanup_armed = 0;
}

static gboolean on_daemon_shutdown_signal(gpointer user_data) {
    AppData *app = (AppData *)user_data;

    log_info("Received shutdown signal; stopping daemon socket monitor");
    daemon_socket_stop_monitor(app);
    disarm_daemon_socket_exit_cleanup();
    path_binaries_shutdown();

    g_daemon_sigterm_source_id = 0;
    g_daemon_sigint_source_id = 0;

    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

static void register_daemon_signal_handlers(AppData *app) {
#if defined(G_OS_UNIX) && GLIB_CHECK_VERSION(2, 30, 0)
    if (g_daemon_sigterm_source_id == 0) {
        g_daemon_sigterm_source_id = g_unix_signal_add(SIGTERM, on_daemon_shutdown_signal, app);
        if (g_daemon_sigterm_source_id == 0) {
            log_warn("Failed to register SIGTERM handler for daemon socket cleanup");
        }
    }

    if (g_daemon_sigint_source_id == 0) {
        g_daemon_sigint_source_id = g_unix_signal_add(SIGINT, on_daemon_shutdown_signal, app);
        if (g_daemon_sigint_source_id == 0) {
            log_warn("Failed to register SIGINT handler for daemon socket cleanup");
        }
    }
#else
    (void)app;
    log_warn("g_unix_signal_add unavailable; daemon shutdown signal cleanup disabled");
#endif
}

static void unregister_daemon_signal_handlers(void) {
    if (g_daemon_sigterm_source_id > 0) {
        g_source_remove(g_daemon_sigterm_source_id);
        g_daemon_sigterm_source_id = 0;
    }

    if (g_daemon_sigint_source_id > 0) {
        g_source_remove(g_daemon_sigint_source_id);
        g_daemon_sigint_source_id = 0;
    }
}

void setup_application(AppData *app, WindowAlignment alignment) {
    app->config.alignment = alignment;

    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "cofi");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 900, 500);
    apply_window_position(app->window, alignment);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(app->window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(app->window), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(app->window), FALSE);

    app->main_overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(app->window), app->main_overlay);

    app->main_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(app->main_overlay), app->main_content);

    app->textview = gtk_text_view_new();
    app->textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->textview));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app->textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app->textview), FALSE);
    gtk_widget_set_can_focus(app->textview, FALSE);

    GtkCssProvider *css_provider = gtk_css_provider_new();
    const char *css =
        "textview { font-family: monospace; font-size: 12pt; }\n"
        "entry { font-family: monospace; font-size: 12pt; }\n"
        "#mode-indicator { font-family: monospace; font-size: 12pt; padding-left: 10px; padding-right: 5px; }\n"
        "#modal-background { background-color: rgba(0, 0, 0, 0.7); }\n"
        "#dialog-overlay { background-color: @theme_bg_color; border: 2px solid @theme_border_color; border-radius: 8px; box-shadow: 0 8px 32px rgba(0, 0, 0, 0.5); padding: 20px; margin: 20px; }\n"
        ".grid-cell { border: 1px solid @theme_border_color; background-color: @theme_base_color; border-radius: 3px; margin: 2px; }";
    gtk_css_provider_load_from_data(css_provider, css, -1, NULL);

    GtkStyleContext *textview_context = gtk_widget_get_style_context(app->textview);
    gtk_style_context_add_provider(textview_context,
                                   GTK_STYLE_PROVIDER(css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(app->textview), 10);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(app->textview), 10);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(app->textview), 10);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(app->textview), 10);

    gtk_widget_set_vexpand(app->textview, TRUE);
    gtk_widget_set_valign(app->textview, GTK_ALIGN_END);

    GtkWidget *entry_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    app->mode_indicator = gtk_label_new(">");
    gtk_widget_set_name(app->mode_indicator, "mode-indicator");
    gtk_label_set_width_chars(GTK_LABEL(app->mode_indicator), 2);
    gtk_widget_set_halign(app->mode_indicator, GTK_ALIGN_CENTER);

    app->entry = gtk_entry_new();
    gtk_widget_set_hexpand(app->entry, TRUE);

    if (app->current_tab == TAB_WORKSPACES) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter workspaces...");
    } else if (app->current_tab == TAB_HARPOON) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter harpoon slots...");
    } else {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter windows...");
    }

    GtkStyleContext *entry_context = gtk_widget_get_style_context(app->entry);
    gtk_style_context_add_provider(entry_context,
                                   GTK_STYLE_PROVIDER(css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkStyleContext *indicator_context = gtk_widget_get_style_context(app->mode_indicator);
    gtk_style_context_add_provider(indicator_context,
                                   GTK_STYLE_PROVIDER(css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_unref(css_provider);

    gtk_box_pack_start(GTK_BOX(entry_box), app->mode_indicator, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(entry_box), app->entry, TRUE, TRUE, 0);

    gtk_widget_set_margin_start(entry_box, 10);
    gtk_widget_set_margin_end(entry_box, 10);
    gtk_widget_set_margin_bottom(entry_box, 10);

    gtk_box_pack_start(GTK_BOX(app->main_content), app->textview, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(app->main_content), entry_box, FALSE, FALSE, 0);

    g_signal_connect(app->window, "delete-event", G_CALLBACK(on_delete_event), app);
    g_signal_connect(app->window, "key-press-event", G_CALLBACK(on_key_press), app);
    g_signal_connect(app->entry, "changed", G_CALLBACK(on_entry_changed), app);
    g_signal_connect(app->window, "focus-out-event", G_CALLBACK(on_focus_out_event), app);

    gtk_widget_set_can_focus(app->window, TRUE);
    gtk_widget_grab_focus(app->entry);

    gtk_window_set_type_hint(GTK_WINDOW(app->window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_focus_on_map(GTK_WINDOW(app->window), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(app->window), TRUE);

    if (alignment != ALIGN_CENTER) {
        app->config.alignment = alignment;
        g_signal_connect(app->window, "size-allocate", G_CALLBACK(on_window_size_allocate), app);
    }

    init_overlay_system(app);

    app->fixed_size_allocate_handler_id =
        g_signal_connect(app->textview, "size-allocate",
                         G_CALLBACK(on_textview_size_allocate_for_fixed_init), app);
}

void on_textview_size_allocate_for_fixed_init(GtkWidget *widget,
                                              GtkAllocation *allocation,
                                              gpointer user_data) {
    AppData *app = (AppData *)user_data;
    if (!app || allocation->width <= 1 || allocation->height <= 1) {
        return;
    }

    if (app->fixed_size_allocate_handler_id > 0) {
        g_signal_handler_disconnect(widget, app->fixed_size_allocate_handler_id);
        app->fixed_size_allocate_handler_id = 0;
    }

    init_fixed_window_size(app);

    if (app->pending_initial_render) {
        update_display(app);
        app->pending_initial_render = FALSE;
    }
}

int run_cofi(int argc, char *argv[]) {
    AppData app = {0};

    init_config_defaults(&app.config);
    int log_enabled = 1;
    char *log_file_path = NULL;
    FILE *log_file = NULL;
    int alignment_specified = 0;
    int close_on_focus_loss_specified = 0;

    log_set_level(LOG_INFO);
    int log_level_from_cli = 0;

    int parse_result = parse_command_line(argc, argv, &app, &log_file_path, &log_enabled,
                                          &alignment_specified, &close_on_focus_loss_specified,
                                          &log_level_from_cli);
    if (parse_result == 2 || parse_result == 3 || parse_result == 4) {
        return 0;
    } else if (parse_result != 0) {
        return 1;
    }

    if (log_file_path) {
        log_file = fopen(log_file_path, "a");
        if (!log_file) {
            fprintf(stderr, "Failed to open log file: %s\n", log_file_path);
            return 1;
        }
    }

    if (log_enabled) {
        if (log_file) {
            log_add_fp(log_file, LOG_DEBUG);
        }
    } else {
        log_set_quiet(true);
    }

    log_debug("Starting cofi...");

    if (!app.assign_slots_and_exit) {
        char socket_path[COFI_SOCKET_PATH_MAX] = {0};
        if (daemon_socket_get_path(socket_path, sizeof(socket_path)) != 0) {
            fprintf(stderr, "Failed to derive daemon socket path\n");
            return 1;
        }

        int existing_fd = daemon_socket_connect(socket_path);
        if (existing_fd >= 0) {
            if (app.startup_delegate_opcode != COFI_OPCODE_RESERVED) {
                int send_rc = daemon_socket_send_opcode(existing_fd, app.startup_delegate_opcode);
                int saved_errno = errno;
                close(existing_fd);
                if (send_rc != 0) {
                    fprintf(stderr, "Failed to delegate mode to running daemon: %s\n",
                            strerror(saved_errno));
                    return 1;
                }
                return 0;
            }

            close(existing_fd);
            fprintf(stderr, "cofi: already running\n");
            return 1;
        }

        int listener_fd = daemon_socket_bind_listener(socket_path);
        if (listener_fd < 0) {
            fprintf(stderr, "cofi: failed to bind daemon socket: %s\n", strerror(errno));
            return 1;
        }

        app.daemon_socket_fd = listener_fd;
        strncpy(app.daemon_socket_path, socket_path, sizeof(app.daemon_socket_path) - 1);
        app.daemon_socket_path[sizeof(app.daemon_socket_path) - 1] = '\0';
        arm_daemon_socket_exit_cleanup(app.daemon_socket_path);
        register_daemon_signal_handlers(&app);
    }

    gint64 start_time = g_get_monotonic_time();
    g_set_prgname("cofi");
    gtk_init(&argc, &argv);

    init_app_data(&app);
    init_x11_connection(&app);

    load_config(&app.config);
    load_harpoon_slots(&app.harpoon);

    if (!log_level_from_cli && app.config.log_level[0]) {
        int level = parse_log_level(app.config.log_level);
        if (level >= 0) {
            log_set_level(level);
            log_debug("Log level set from config: %s", app.config.log_level);
        }
    }

    log_debug("close_on_focus_loss = %d (cmdline_specified=%d)",
              app.config.close_on_focus_loss, close_on_focus_loss_specified);

    if (alignment_specified) {
        save_config(&app.config);
        log_debug("Using command line alignment: %d", app.config.alignment);
    } else {
        log_debug("Using config alignment: %d", app.config.alignment);
    }

    gint64 window_enum_start = g_get_monotonic_time();
    init_window_list(&app);
    init_workspaces(&app);
    gint64 window_enum_end = g_get_monotonic_time();
    log_info("Window enumeration completed in %.2fms (%d windows)",
             (window_enum_end - window_enum_start) / 1000.0, app.window_count);

    if (app.assign_slots_and_exit) {
        assign_workspace_slots(&app);
        log_info("Assigned workspace slots and exiting");
        if (log_file) fclose(log_file);
        return 0;
    }

    init_history_from_windows(&app);
    init_selection(&app);

    setup_application(&app, app.config.alignment);
    setup_x11_event_monitoring(&app);

    gtk_widget_realize(app.window);
    GdkWindow *gdk_window = gtk_widget_get_window(app.window);
    if (gdk_window) {
        app.own_window_id = GDK_WINDOW_XID(gdk_window);
        log_debug("Own window ID: 0x%lx", app.own_window_id);
    } else {
        app.own_window_id = 0;
        log_warn("Could not get own window ID");
    }

    setup_hotkeys(&app);

    if (!app.assign_slots_and_exit) {
        if (daemon_socket_start_monitor(&app) != 0) {
            fprintf(stderr, "cofi: failed to start daemon socket watcher\n");
            cleanup_hotkeys(&app);
            daemon_socket_stop_monitor(&app);
            disarm_daemon_socket_exit_cleanup();
            unregister_daemon_signal_handlers();
            cleanup_window_highlight(&app);
            cleanup_x11_event_monitoring();
            XCloseDisplay(app.display);
            if (log_file) {
                fclose(log_file);
            }
            return 1;
        }
    }

    if (app.startup_delegate_opcode != COFI_OPCODE_RESERVED) {
        daemon_socket_dispatch_opcode(&app, app.startup_delegate_opcode);
        log_info("Started daemon with delegated startup mode: %s",
                 daemon_socket_opcode_name(app.startup_delegate_opcode));
    } else {
        log_info("Daemon started, waiting for hotkeys");
    }

    gint64 window_show_time = g_get_monotonic_time();
    log_debug("Total startup time: %.2fms", (window_show_time - start_time) / 1000.0);

    gtk_main();

    cleanup_hotkeys(&app);
    daemon_socket_stop_monitor(&app);
    disarm_daemon_socket_exit_cleanup();
    unregister_daemon_signal_handlers();
    cleanup_window_highlight(&app);
    cleanup_x11_event_monitoring();
    XCloseDisplay(app.display);

    if (log_file) {
        log_debug("Closing log file");
        fclose(log_file);
    }

    return 0;
}
