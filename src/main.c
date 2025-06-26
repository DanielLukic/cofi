#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <getopt.h>
#include <ctype.h>
#include "app_data.h"
#include "x11_utils.h"
#include "window_list.h"
#include "workspace_info.h"
#include "history.h"
#include "display.h"
#include "filter.h"
#include "log.h"
#include "x11_events.h"
#include "instance.h"
#include "harpoon.h"
#include "match.h"
#include "constants.h"
#include "utils.h"
#include "cli_args.h"
#include "gtk_window.h"
#include "app_init.h"

#define COFI_VERSION "0.1.0"

// MAX_WINDOWS, MAX_TITLE_LEN, MAX_CLASS_LEN are defined in src/window_info.h
// WindowInfo and AppData types are defined in src/app_data.h

// Forward declarations
static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, AppData *app);
static void on_entry_changed(GtkEntry *entry, AppData *app);
static gboolean on_delete_event(GtkWidget *widget, GdkEvent *event, AppData *app);
static gboolean on_focus_out_event(GtkWidget *widget, GdkEventFocus *event, AppData *app);
static void filter_workspaces(AppData *app, const char *filter);

// Forward declaration for destroy_window function
static void destroy_window(AppData *app);
static gboolean check_focus_loss_delayed(AppData *app);

// Selection management functions
static void reset_selection(AppData *app) {
    if (app->current_tab == TAB_WINDOWS) {
        app->selected_index = 0;
    } else {
        app->selected_workspace_index = 0;
    }
}

// Helper function to switch between tabs
static void switch_to_tab(AppData *app, TabMode target_tab) {
    if (app->current_tab == target_tab) {
        return; // Already on the target tab
    }
    
    app->current_tab = target_tab;
    gtk_entry_set_text(GTK_ENTRY(app->entry), "");
    
    if (target_tab == TAB_WINDOWS) {
        filter_windows(app, "");
    } else {
        filter_workspaces(app, "");
    }
    reset_selection(app);
    
    update_display(app);
    log_debug("Switched to %s tab", target_tab == TAB_WINDOWS ? "Windows" : "Workspaces");
}

// Helper function to get harpoon slot from key event
static int get_harpoon_slot(GdkEventKey *event) {
    if (event->keyval >= GDK_KEY_0 && event->keyval <= GDK_KEY_9) {
        return event->keyval - GDK_KEY_0;
    } else if (event->keyval >= GDK_KEY_KP_0 && event->keyval <= GDK_KEY_KP_9) {
        return event->keyval - GDK_KEY_KP_0;
    } else if (event->keyval >= GDK_KEY_a && event->keyval <= GDK_KEY_z) {
        // Exclude navigation keys (h, j, k, l) and text clearing key (u)
        if (event->keyval != GDK_KEY_h && event->keyval != GDK_KEY_j && 
            event->keyval != GDK_KEY_k && event->keyval != GDK_KEY_l &&
            event->keyval != GDK_KEY_u) {
            return HARPOON_FIRST_LETTER + (event->keyval - GDK_KEY_a);
        }
    }
    return -1;
}

// Move selection up (decrements index in display, moves up visually)
static void move_selection_up(AppData *app) {
    if (app->current_tab == TAB_WINDOWS) {
        if (app->selected_index < app->filtered_count - 1) {
            app->selected_index++;
            update_display(app);
        }
    } else {
        if (app->selected_workspace_index < app->filtered_workspace_count - 1) {
            app->selected_workspace_index++;
            update_display(app);
        }
    }
}

// Move selection down (increments index in display, moves down visually)
static void move_selection_down(AppData *app) {
    if (app->current_tab == TAB_WINDOWS) {
        if (app->selected_index > 0) {
            app->selected_index--;
            update_display(app);
        }
    } else {
        if (app->selected_workspace_index > 0) {
            app->selected_workspace_index--;
            update_display(app);
        }
    }
}

// Handle Ctrl+key for harpoon assignment/unassignment
static gboolean handle_harpoon_assignment(GdkEventKey *event, AppData *app) {
    if (!(event->state & GDK_CONTROL_MASK) || app->current_tab != TAB_WINDOWS) {
        return FALSE;
    }
    
    int slot = get_harpoon_slot(event);
    if (slot < 0 || app->filtered_count == 0 || app->selected_index >= app->filtered_count) {
        return FALSE;
    }
    
    // Get the window that's displayed at the selected position
    WindowInfo *win = &app->filtered[app->selected_index];
    
    // Check if this window is already assigned to this slot
    Window current_window = get_slot_window(&app->harpoon, slot);
    if (current_window == win->id) {
        // Unassign if same window
        unassign_slot(&app->harpoon, slot);
        log_info("Unassigned window '%s' from slot %d", win->title, slot);
    } else {
        // First unassign any other slot this window might have
        int old_slot = get_window_slot(&app->harpoon, win->id);
        if (old_slot >= 0) {
            unassign_slot(&app->harpoon, old_slot);
        }
        // Assign to new slot
        assign_window_to_slot(&app->harpoon, slot, win);
        log_info("Assigned window '%s' to slot %d", win->title, slot);
    }
    
    save_harpoon_config(&app->harpoon);
    update_display(app);
    return TRUE;
}

// Handle Alt+key for harpoon/workspace switching
static gboolean handle_harpoon_workspace_switching(GdkEventKey *event, AppData *app) {
    if (!(event->state & GDK_MOD1_MASK)) {  // GDK_MOD1_MASK is Alt
        return FALSE;
    }
    
    int slot = get_harpoon_slot(event);
    if (slot < 0) {
        return FALSE;
    }
    
    if (app->current_tab == TAB_WINDOWS) {
        // Switch to harpooned window
        Window target_window = get_slot_window(&app->harpoon, slot);
        if (target_window != 0) {
            activate_window(target_window);
            destroy_window(app);
            log_info("Switched to harpooned window in slot %d", slot);
            return TRUE;
        }
    } else {
        // Switch to workspace by number
        if (slot < app->workspace_count) {
            switch_to_desktop(app->display, slot);
            destroy_window(app);
            log_info("Switched to workspace %d", slot);
            return TRUE;
        }
    }
    
    return FALSE;
}

// Handle tab switching keys
static gboolean handle_tab_switching(GdkEventKey *event, AppData *app) {
    // Tab key (without Ctrl)
    if (event->keyval == GDK_KEY_Tab && !(event->state & GDK_CONTROL_MASK)) {
        TabMode next_tab = (app->current_tab == TAB_WINDOWS) ? TAB_WORKSPACES : TAB_WINDOWS;
        switch_to_tab(app, next_tab);
        return TRUE;
    }
    
    // Ctrl+H/L for tab switching
    if (event->state & GDK_CONTROL_MASK) {
        if (event->keyval == GDK_KEY_h || event->keyval == GDK_KEY_H) {
            // Ctrl+H: previous tab with wrap-around
            TabMode prev_tab = (app->current_tab == TAB_WINDOWS) ? TAB_WORKSPACES : TAB_WINDOWS;
            switch_to_tab(app, prev_tab);
            return TRUE;
        } else if (event->keyval == GDK_KEY_l || event->keyval == GDK_KEY_L) {
            // Ctrl+L: next tab with wrap-around
            TabMode next_tab = (app->current_tab == TAB_WINDOWS) ? TAB_WORKSPACES : TAB_WINDOWS;
            switch_to_tab(app, next_tab);
            return TRUE;
        }
    }
    
    return FALSE;
}

// Handle navigation keys (arrows, Ctrl+j/k)
static gboolean handle_navigation_keys(GdkEventKey *event, AppData *app) {
    switch (event->keyval) {
        case GDK_KEY_Escape:
            destroy_window(app);
            return TRUE;
            
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            if (app->current_tab == TAB_WINDOWS) {
                if (app->filtered_count > 0 && app->selected_index < app->filtered_count) {
                    WindowInfo *win = &app->filtered[app->selected_index];
                    activate_window(win->id);
                    destroy_window(app);
                }
            } else {
                if (app->filtered_workspace_count > 0 && app->selected_workspace_index < app->filtered_workspace_count) {
                    WorkspaceInfo *ws = &app->filtered_workspaces[app->selected_workspace_index];
                    switch_to_desktop(app->display, ws->id);
                    destroy_window(app);
                    log_info("Switched to workspace %d: %s", ws->id, ws->name);
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

// Handle key press events
static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, AppData *app) {
    (void)widget; // Unused parameter
    
    // Try handlers in order of priority
    if (handle_harpoon_assignment(event, app)) {
        return TRUE;
    }
    
    if (handle_harpoon_workspace_switching(event, app)) {
        return TRUE;
    }
    
    if (handle_tab_switching(event, app)) {
        return TRUE;
    }
    
    if (handle_navigation_keys(event, app)) {
        return TRUE;
    }
    
    return FALSE;
}

// Workspace filtering with fuzzy search
static void filter_workspaces(AppData *app, const char *filter) {
    app->filtered_workspace_count = 0;
    
    if (!filter || !*filter) {
        // No filter - show all workspaces
        for (int i = 0; i < app->workspace_count; i++) {
            app->filtered_workspaces[app->filtered_workspace_count++] = app->workspaces[i];
        }
        return;
    }
    
    // Build searchable string for each workspace: "id name"
    char searchable[512];
    for (int i = 0; i < app->workspace_count; i++) {
        snprintf(searchable, sizeof(searchable), "%d %s", 
                 app->workspaces[i].id, app->workspaces[i].name);
        
        // Use has_match and match functions from filter system
        if (has_match(filter, searchable)) {
            app->filtered_workspaces[app->filtered_workspace_count++] = app->workspaces[i];
        }
    }
}

// Handle entry text changes
static void on_entry_changed(GtkEntry *entry, AppData *app) {
    const char *text = gtk_entry_get_text(entry);
    
    if (app->current_tab == TAB_WINDOWS) {
        filter_windows(app, text);
    } else {
        filter_workspaces(app, text);
    }
    // Ensure selection is always 0 after filtering
    reset_selection(app);
    
    update_display(app);
}

// Handle window delete event (close button)
static gboolean on_delete_event(GtkWidget *widget, GdkEvent *event, AppData *app) {
    (void)widget;
    (void)event;
    destroy_window(app);
    return TRUE; // Prevent default handler
}

// Handle focus out event
static gboolean on_focus_out_event(GtkWidget *widget, GdkEventFocus *event, AppData *app) {
    (void)event;
    
    // Only close if close_on_focus_loss is enabled
    if (!app->close_on_focus_loss) {
        return FALSE;
    }
    
    // Use a small delay to properly detect if focus is really lost
    // This helps distinguish between internal widget focus changes and actual window focus loss
    g_timeout_add(100, (GSourceFunc)check_focus_loss_delayed, app);
    
    return FALSE;
}

// Delayed check for focus loss
static gboolean check_focus_loss_delayed(AppData *app) {
    if (!app->window) {
        return FALSE; // Window already destroyed
    }
    
    // Check if our window still has toplevel focus
    if (gtk_window_has_toplevel_focus(GTK_WINDOW(app->window))) {
        log_debug("Window still has toplevel focus after delay, not closing");
        return FALSE;
    }
    
    log_info("Window lost focus to external application, closing");
    destroy_window(app);
    return FALSE; // Don't repeat the timeout
}

// Destroy window instead of hiding it
static void destroy_window(AppData *app) {
    if (app->window) {
        // Save window position before destroying
        gint x, y;
        gtk_window_get_position(GTK_WINDOW(app->window), &x, &y);
        save_config_with_position(&app->harpoon, 1, x, y);
        log_debug("Saved window position: x=%d, y=%d", x, y);
        
        gtk_widget_destroy(app->window);
        app->window = NULL;
        app->entry = NULL;
        app->textview = NULL;
        app->scrolled = NULL;
        app->textbuffer = NULL;
        // ALWAYS reset selection to 0
        reset_selection(app);
        log_debug("Selection reset to 0 in destroy_window");
    }
}


// Application setup
void setup_application(AppData *app, WindowAlignment alignment) {
    // Store alignment for future window recreations
    app->alignment = alignment;
    
    // Create main window
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "cofi");
    // Set reasonable default size; will be adjusted based on content
    gtk_window_set_default_size(GTK_WINDOW(app->window), 900, 500);
    // Set window position based on saved position or alignment
    if (app->has_saved_position) {
        // Validate saved position is still on-screen
        GdkScreen *screen = gdk_screen_get_default();
        validate_saved_position(app, screen);
    } else {
        // Set window position based on alignment
        apply_window_position(app->window, alignment);
    }
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(app->window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(app->window), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(app->window), FALSE);
    
    // Create main vertical box
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(app->window), vbox);
    
    // Create text view and buffer (bottom-aligned, no scrolling)
    app->textview = gtk_text_view_new();
    app->textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->textview));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app->textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app->textview), FALSE);
    gtk_widget_set_can_focus(app->textview, FALSE);
    
    // Set monospace font
    PangoFontDescription *font_desc = pango_font_description_from_string("monospace 12");
    gtk_widget_override_font(app->textview, font_desc);
    
    // Add margins
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(app->textview), 10);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(app->textview), 10);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(app->textview), 10);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(app->textview), 10);
    
    // Bottom alignment: expand and pin to end
    gtk_widget_set_vexpand(app->textview, TRUE);
    gtk_widget_set_valign(app->textview, GTK_ALIGN_END);
    
    // Create entry
    app->entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter windows...");
    
    // Set monospace font for entry too
    gtk_widget_override_font(app->entry, font_desc);
    
    pango_font_description_free(font_desc);
    
    // Pack widgets (text view on top, entry at bottom - like fzf)
    gtk_box_pack_start(GTK_BOX(vbox), app->textview, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), app->entry, FALSE, FALSE, 0);
    
    // Connect signals
    g_signal_connect(app->window, "delete-event", G_CALLBACK(on_delete_event), app);
    g_signal_connect(app->window, "key-press-event", G_CALLBACK(on_key_press), app);
    g_signal_connect(app->entry, "changed", G_CALLBACK(on_entry_changed), app);
    g_signal_connect(app->window, "focus-out-event", G_CALLBACK(on_focus_out_event), app);
    
    // Make window capture all key events
    gtk_widget_set_can_focus(app->window, TRUE);
    gtk_widget_grab_focus(app->entry);
    
    // Additional window hints for better focus handling
    gtk_window_set_type_hint(GTK_WINDOW(app->window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_focus_on_map(GTK_WINDOW(app->window), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(app->window), TRUE);
    
    // Position window manually for non-center alignments
    if (alignment != ALIGN_CENTER) {
        // Store alignment in app data for use in callback
        app->alignment = alignment;
        // Connect to size-allocate to reposition whenever size changes
        g_signal_connect(app->window, "size-allocate", G_CALLBACK(on_window_size_allocate), app);
    }
}



int main(int argc, char *argv[]) {
    AppData app = {0};
    
    // Default settings
    app.alignment = ALIGN_CENTER;
    int log_enabled = 1;
    char *log_file_path = NULL;
    FILE *log_file = NULL;
    int alignment_specified = 0;
    
    // Set default log level to INFO
    log_set_level(LOG_INFO);
    
    // Parse command line arguments
    int parse_result = parse_command_line(argc, argv, &app, &log_file_path, &log_enabled, &alignment_specified);
    if (parse_result == 2) {
        // Version was printed
        return 0;
    } else if (parse_result == 3) {
        // Help was printed
        return 0;
    } else if (parse_result != 0) {
        return 1;
    }
    
    // Open log file if specified
    if (log_file_path) {
        log_file = fopen(log_file_path, "a");
        if (!log_file) {
            fprintf(stderr, "Failed to open log file: %s\n", log_file_path);
            return 1;
        }
    }
    
    // Initialize logging
    if (log_enabled) {
        if (log_file) {
            log_add_fp(log_file, LOG_DEBUG);
        }
    } else {
        log_set_quiet(true);
    }
    
    log_debug("Starting cofi...");
    
    // Check for existing instance
    InstanceManager *instance_manager = instance_manager_new();
    if (!instance_manager) {
        log_error("Failed to create instance manager");
        if (log_file) fclose(log_file);
        return 1;
    }
    
    if (instance_manager_check_existing(instance_manager)) {
        log_info("Another instance is already running, exiting");
        instance_manager_cleanup(instance_manager);
        if (log_file) fclose(log_file);
        return 0;
    }
    
    // Set program name for WM_CLASS property
    g_set_prgname("cofi");
    
    // Initialize GTK (needs to be called with modified argc/argv after getopt)
    gtk_init(&argc, &argv);
    
    // Initialize application data
    init_app_data(&app);
    
    // Open X11 display
    init_x11_connection(&app);
    
    if (alignment_specified) {
        // If --align was specified, clear any saved position
        load_harpoon_config(&app.harpoon);
        save_config_with_position(&app.harpoon, 0, 0, 0); // Clear saved position
        log_debug("Cleared saved position due to --align argument");
    } else {
        // Load position if no --align was specified
        load_config_with_position(&app.harpoon, &app.has_saved_position, &app.saved_x, &app.saved_y);
        if (app.has_saved_position) {
            log_debug("Loaded saved position: x=%d, y=%d", app.saved_x, app.saved_y);
        }
    }
    
    // Initialize window and workspace lists
    init_window_list(&app);
    init_workspaces(&app);
    
    // Initialize history from windows
    init_history_from_windows(&app);
    
    // Setup GUI
    setup_application(&app, app.alignment);
    
    // Set app data for instance manager and setup signal handler
    // Do this after GUI setup so the window exists
    instance_manager_set_app_data(&app);
    instance_manager_setup_signal_handler();
    
    // Setup X11 event monitoring for dynamic window list updates
    setup_x11_event_monitoring(&app);
    
    // Update display
    update_display(&app);
    
    // ALWAYS reset selection to 0 before showing window
    reset_selection(&app);
    log_debug("Selection reset to 0 before showing window");
    
    // Show window and run
    gtk_widget_show_all(app.window);
    gtk_widget_grab_focus(app.entry);
    
    // Get our own window ID for filtering
    GdkWindow *gdk_window = gtk_widget_get_window(app.window);
    if (gdk_window) {
        app.own_window_id = GDK_WINDOW_XID(gdk_window);
        log_debug("Stored own window ID: 0x%lx", app.own_window_id);
    } else {
        app.own_window_id = 0;
        log_warn("Could not get own window ID");
    }
    
    gtk_main();
    
    // Cleanup
    cleanup_x11_event_monitoring();
    instance_manager_cleanup(instance_manager);
    XCloseDisplay(app.display);
    
    if (log_file) {
        log_debug("Closing log file");
        fclose(log_file);
    }
    
    return 0;
}