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
#include "x11_utils.h"
#include "window_list.h"
#include "app_data.h"
#include "workspace_info.h"
#include "history.h"
#include "display.h"
#include "filter.h"
#include "log.h"
#include "x11_events.h"
#include "instance.h"
#include "harpoon.h"
#include "match.h"

#define COFI_VERSION "0.1.0"

// MAX_WINDOWS, MAX_TITLE_LEN, MAX_CLASS_LEN are defined in src/window_info.h
// WindowInfo and AppData types are defined in src/app_data.h

// Forward declarations
static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, AppData *app);
static void on_entry_changed(GtkEntry *entry, AppData *app);
static void print_usage(const char *prog_name);
static int parse_log_level(const char *level_str);
static gboolean on_delete_event(GtkWidget *widget, GdkEvent *event, AppData *app);
static void on_window_size_allocate(GtkWidget *window, GtkAllocation *allocation, gpointer user_data);
static void filter_workspaces(AppData *app, const char *filter);

// Forward declaration for destroy_window function
static void destroy_window(AppData *app);

// Helper function to switch between tabs
static void switch_to_tab(AppData *app, TabMode target_tab) {
    if (app->current_tab == target_tab) {
        return; // Already on the target tab
    }
    
    app->current_tab = target_tab;
    gtk_entry_set_text(GTK_ENTRY(app->entry), "");
    
    if (target_tab == TAB_WINDOWS) {
        filter_windows(app, "");
        app->selected_index = 0;
    } else {
        filter_workspaces(app, "");
        app->selected_workspace_index = 0;
    }
    
    update_display(app);
    log_debug("Switched to %s tab", target_tab == TAB_WINDOWS ? "Windows" : "Workspaces");
}

// Handle key press events
static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, AppData *app) {
    (void)widget; // Unused parameter
    
    // Check for Ctrl+number or Ctrl+letter (assign/unassign harpoon) - only in window mode
    if ((event->state & GDK_CONTROL_MASK) && app->current_tab == TAB_WINDOWS) {
        int slot = -1;
        if (event->keyval >= GDK_KEY_0 && event->keyval <= GDK_KEY_9) {
            slot = event->keyval - GDK_KEY_0;
        } else if (event->keyval >= GDK_KEY_KP_0 && event->keyval <= GDK_KEY_KP_9) {
            slot = event->keyval - GDK_KEY_KP_0;
        } else if (event->keyval >= GDK_KEY_a && event->keyval <= GDK_KEY_z) {
            // Exclude navigation keys (h, j, k, l)
            if (event->keyval != GDK_KEY_h && event->keyval != GDK_KEY_j && 
                event->keyval != GDK_KEY_k && event->keyval != GDK_KEY_l) {
                slot = 10 + (event->keyval - GDK_KEY_a);
            }
        }
        
        if (slot >= 0 && app->filtered_count > 0 && app->selected_index < app->filtered_count) {
            // Get the window that's displayed at the selected position
            // The filtered array already has Alt-Tab swap applied, so use selected_index directly
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
    }
    
    // Check for Alt+number or Alt+letter
    if (event->state & GDK_MOD1_MASK) {  // GDK_MOD1_MASK is Alt
        int slot = -1;
        if (event->keyval >= GDK_KEY_0 && event->keyval <= GDK_KEY_9) {
            slot = event->keyval - GDK_KEY_0;
        } else if (event->keyval >= GDK_KEY_KP_0 && event->keyval <= GDK_KEY_KP_9) {
            slot = event->keyval - GDK_KEY_KP_0;
        } else if (event->keyval >= GDK_KEY_a && event->keyval <= GDK_KEY_z) {
            // Exclude navigation keys (h, j, k, l)
            if (event->keyval != GDK_KEY_h && event->keyval != GDK_KEY_j && 
                event->keyval != GDK_KEY_k && event->keyval != GDK_KEY_l) {
                slot = 10 + (event->keyval - GDK_KEY_a);
            }
        }
        
        if (slot >= 0) {
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
        }
    }
    
    // Tab switching: Tab wraps around
    if (event->keyval == GDK_KEY_Tab && !(event->state & GDK_CONTROL_MASK)) {
        // Tab: switch to next tab with wrap-around
        TabMode next_tab = (app->current_tab == TAB_WINDOWS) ? TAB_WORKSPACES : TAB_WINDOWS;
        switch_to_tab(app, next_tab);
        return TRUE;
    }
    
    // Ctrl+H/L for tab switching with wrap-around
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
    
    switch (event->keyval) {
        case GDK_KEY_Escape:
            destroy_window(app);
            return TRUE;
            
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            if (app->current_tab == TAB_WINDOWS) {
                if (app->filtered_count > 0 && app->selected_index < app->filtered_count) {
                    // The swap is already applied in the filtered array, so just use the selected index
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
            return TRUE;
            
        case GDK_KEY_Down:
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
            return TRUE;
        
        case GDK_KEY_k:
            if (event->state & GDK_CONTROL_MASK) {
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
                return TRUE;
            }
            break;
            
        case GDK_KEY_j:
            if (event->state & GDK_CONTROL_MASK) {
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
                return TRUE;
            }
            break;
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
        // Ensure selection is always 0 after filtering
        app->selected_index = 0;
    } else {
        filter_workspaces(app, text);
        // Ensure selection is always 0 after filtering
        app->selected_workspace_index = 0;
    }
    
    update_display(app);
}

// Handle window delete event (close button)
static gboolean on_delete_event(GtkWidget *widget, GdkEvent *event, AppData *app) {
    (void)widget;
    (void)event;
    destroy_window(app);
    return TRUE; // Prevent default handler
}

// Destroy window instead of hiding it
static void destroy_window(AppData *app) {
    if (app->window) {
        gtk_widget_destroy(app->window);
        app->window = NULL;
        app->entry = NULL;
        app->textview = NULL;
        app->scrolled = NULL;
        app->textbuffer = NULL;
        // ALWAYS reset selection to 0
        app->selected_index = 0;
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
    // Set window position based on alignment
    switch (alignment) {
        case ALIGN_CENTER:
            gtk_window_set_position(GTK_WINDOW(app->window), GTK_WIN_POS_CENTER);
            break;
        case ALIGN_TOP:
            gtk_window_set_position(GTK_WINDOW(app->window), GTK_WIN_POS_NONE);
            // Position will be set after window is realized
            break;
        case ALIGN_TOP_LEFT:
            gtk_window_set_position(GTK_WINDOW(app->window), GTK_WIN_POS_NONE);
            break;
        case ALIGN_TOP_RIGHT:
            gtk_window_set_position(GTK_WINDOW(app->window), GTK_WIN_POS_NONE);
            break;
        case ALIGN_LEFT:
            gtk_window_set_position(GTK_WINDOW(app->window), GTK_WIN_POS_NONE);
            break;
        case ALIGN_RIGHT:
            gtk_window_set_position(GTK_WINDOW(app->window), GTK_WIN_POS_NONE);
            break;
        case ALIGN_BOTTOM:
            gtk_window_set_position(GTK_WINDOW(app->window), GTK_WIN_POS_NONE);
            break;
        case ALIGN_BOTTOM_LEFT:
            gtk_window_set_position(GTK_WINDOW(app->window), GTK_WIN_POS_NONE);
            break;
        case ALIGN_BOTTOM_RIGHT:
            gtk_window_set_position(GTK_WINDOW(app->window), GTK_WIN_POS_NONE);
            break;
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

static void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  --log-level LEVEL    Set log level (trace, debug, info, warn, error, fatal)\n");
    printf("  --log-file FILE      Write logs to FILE\n");
    printf("  --no-log             Disable logging\n");
    printf("  --align ALIGNMENT    Set window alignment (center, top, top_left, top_right,\n");
    printf("                       left, right, bottom, bottom_left, bottom_right)\n");
    printf("  --kill               Kill running cofi instance\n");
    printf("  --version            Show version information\n");
    printf("  --help               Show this help message\n");
}

static int parse_log_level(const char *level_str) {
    if (strcasecmp(level_str, "trace") == 0) return LOG_TRACE;
    if (strcasecmp(level_str, "debug") == 0) return LOG_DEBUG;
    if (strcasecmp(level_str, "info") == 0) return LOG_INFO;
    if (strcasecmp(level_str, "warn") == 0) return LOG_WARN;
    if (strcasecmp(level_str, "error") == 0) return LOG_ERROR;
    if (strcasecmp(level_str, "fatal") == 0) return LOG_FATAL;
    return -1;
}

static WindowAlignment parse_alignment(const char *align_str) {
    if (strcasecmp(align_str, "center") == 0) return ALIGN_CENTER;
    if (strcasecmp(align_str, "top") == 0) return ALIGN_TOP;
    if (strcasecmp(align_str, "top_left") == 0) return ALIGN_TOP_LEFT;
    if (strcasecmp(align_str, "top_right") == 0) return ALIGN_TOP_RIGHT;
    if (strcasecmp(align_str, "left") == 0) return ALIGN_LEFT;
    if (strcasecmp(align_str, "right") == 0) return ALIGN_RIGHT;
    if (strcasecmp(align_str, "bottom") == 0) return ALIGN_BOTTOM;
    if (strcasecmp(align_str, "bottom_left") == 0) return ALIGN_BOTTOM_LEFT;
    if (strcasecmp(align_str, "bottom_right") == 0) return ALIGN_BOTTOM_RIGHT;
    return ALIGN_CENTER; // Default fallback
}

// Callback to reposition window whenever size changes
static void on_window_size_allocate(GtkWidget *window, GtkAllocation *allocation, gpointer user_data) {
    AppData *app = (AppData *)user_data;
    WindowAlignment alignment = app->alignment;
    
    // Only reposition if we have a valid size
    if (allocation->width <= 1 || allocation->height <= 1) {
        return;
    }
    GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(window));
    GdkDisplay *display = gdk_screen_get_display(screen);
    
    // Get mouse pointer position to determine current monitor
    gint mouse_x, mouse_y;
    gdk_display_get_pointer(display, NULL, &mouse_x, &mouse_y, NULL);
    
    // Get the monitor at mouse position
    gint monitor_num = gdk_screen_get_monitor_at_point(screen, mouse_x, mouse_y);
    GdkRectangle monitor_geometry;
    gdk_screen_get_monitor_geometry(screen, monitor_num, &monitor_geometry);
    
    // Use the allocation size
    gint window_width = allocation->width;
    gint window_height = allocation->height;
    
    log_debug("Repositioning window on size change: alignment=%d, size=%dx%d", 
              alignment, window_width, window_height);
    
    gint x = 0, y = 0;
    
    switch (alignment) {
        case ALIGN_TOP:
            x = monitor_geometry.x + (monitor_geometry.width - window_width) / 2;
            y = monitor_geometry.y;
            break;
        case ALIGN_TOP_LEFT:
            x = monitor_geometry.x;
            y = monitor_geometry.y;
            break;
        case ALIGN_TOP_RIGHT:
            x = monitor_geometry.x + monitor_geometry.width - window_width;
            y = monitor_geometry.y;
            break;
        case ALIGN_LEFT:
            x = monitor_geometry.x;
            y = monitor_geometry.y + (monitor_geometry.height - window_height) / 2;
            break;
        case ALIGN_RIGHT:
            x = monitor_geometry.x + monitor_geometry.width - window_width;
            y = monitor_geometry.y + (monitor_geometry.height - window_height) / 2;
            break;
        case ALIGN_BOTTOM:
            x = monitor_geometry.x + (monitor_geometry.width - window_width) / 2;
            y = monitor_geometry.y + monitor_geometry.height - window_height;
            break;
        case ALIGN_BOTTOM_LEFT:
            x = monitor_geometry.x;
            y = monitor_geometry.y + monitor_geometry.height - window_height;
            break;
        case ALIGN_BOTTOM_RIGHT:
            x = monitor_geometry.x + monitor_geometry.width - window_width;
            y = monitor_geometry.y + monitor_geometry.height - window_height;
            break;
        case ALIGN_CENTER:
        default:
            // This shouldn't happen for non-center alignments
            x = monitor_geometry.x + (monitor_geometry.width - window_width) / 2;
            y = monitor_geometry.y + (monitor_geometry.height - window_height) / 2;
            break;
    }
    
    log_debug("Monitor geometry: x=%d, y=%d, width=%d, height=%d",
              monitor_geometry.x, monitor_geometry.y, 
              monitor_geometry.width, monitor_geometry.height);
    log_debug("Calculated position: x=%d, y=%d", x, y);
    
    // Set gravity hint before moving
    gtk_window_set_gravity(GTK_WINDOW(window), GDK_GRAVITY_STATIC);
    gtk_window_move(GTK_WINDOW(window), x, y);
}

int main(int argc, char *argv[]) {
    AppData app = {0};
    
    // Command line options
    static struct option long_options[] = {
        {"log-level", required_argument, 0, 'l'},
        {"log-file", required_argument, 0, 'f'},
        {"no-log", no_argument, 0, 'n'},
        {"align", required_argument, 0, 'a'},
        {"kill", no_argument, 0, 'k'},
        {"version", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    // Default settings
    int log_level = LOG_INFO;
    bool logging_enabled = true;
    FILE *log_file = NULL;
    WindowAlignment alignment = ALIGN_CENTER;
    
    // Parse command line arguments
    int option_index = 0;
    int c;
    while ((c = getopt_long(argc, argv, "l:f:na:kvh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'l':
                log_level = parse_log_level(optarg);
                if (log_level == -1) {
                    fprintf(stderr, "Invalid log level: %s\n", optarg);
                    fprintf(stderr, "Valid levels are: trace, debug, info, warn, error, fatal\n");
                    return 1;
                }
                break;
            case 'f':
                log_file = fopen(optarg, "a");
                if (!log_file) {
                    fprintf(stderr, "Failed to open log file: %s\n", optarg);
                    return 1;
                }
                break;
            case 'n':
                logging_enabled = false;
                break;
            case 'a':
                // Validate alignment argument first
                if (strcasecmp(optarg, "center") != 0 &&
                    strcasecmp(optarg, "top") != 0 &&
                    strcasecmp(optarg, "top_left") != 0 &&
                    strcasecmp(optarg, "top_right") != 0 &&
                    strcasecmp(optarg, "left") != 0 &&
                    strcasecmp(optarg, "right") != 0 &&
                    strcasecmp(optarg, "bottom") != 0 &&
                    strcasecmp(optarg, "bottom_left") != 0 &&
                    strcasecmp(optarg, "bottom_right") != 0) {
                    fprintf(stderr, "Invalid alignment: %s\n", optarg);
                    fprintf(stderr, "Valid alignments: center, top, top_left, top_right, left, right, bottom, bottom_left, bottom_right\n");
                    return 1;
                }
                alignment = parse_alignment(optarg);
                break;
            case 'k':
                // TODO: Implement kill functionality
                printf("Kill functionality not yet implemented\n");
                return 0;
            case 'v':
                printf("cofi version %s\n", COFI_VERSION);
                return 0;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Initialize logging
    if (logging_enabled) {
        log_set_level(log_level);
        if (log_file) {
            log_add_fp(log_file, log_level);
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
    
    // Open X11 display
    app.display = XOpenDisplay(NULL);
    if (!app.display) {
        log_error("Cannot open X11 display");
        if (log_file) fclose(log_file);
        return 1;
    }
    
    log_debug("X11 display opened successfully");
    
    // Initialize history and active window tracking
    app.history_count = 0;
    app.active_window_id = -1; // Use -1 to force initial active window to be moved to front
    
    // Initialize tab mode
    app.current_tab = TAB_WINDOWS;
    app.selected_workspace_index = 0;
    
    // Initialize harpoon manager
    init_harpoon_manager(&app.harpoon);
    load_harpoon_config(&app.harpoon);
    
    // Get window list
    get_window_list(&app);
    
    // Get workspace list
    int num_desktops = get_number_of_desktops(app.display);
    int current_desktop = get_current_desktop(app.display);
    int desktop_count = 0;
    char** desktop_names = get_desktop_names(app.display, &desktop_count);
    
    app.workspace_count = (num_desktops < MAX_WORKSPACES) ? num_desktops : MAX_WORKSPACES;
    for (int i = 0; i < app.workspace_count; i++) {
        app.workspaces[i].id = i;
        strncpy(app.workspaces[i].name, desktop_names[i], MAX_WORKSPACE_NAME_LEN - 1);
        app.workspaces[i].name[MAX_WORKSPACE_NAME_LEN - 1] = '\0';
        app.workspaces[i].is_current = (i == current_desktop);
        app.filtered_workspaces[i] = app.workspaces[i];
    }
    app.filtered_workspace_count = app.workspace_count;
    
    // Free desktop names
    for (int i = 0; i < desktop_count; i++) {
        free(desktop_names[i]);
    }
    free(desktop_names);
    
    log_debug("Found %d workspaces, current workspace: %d", app.workspace_count, current_desktop);
    
    // Check for automatic reassignments after loading config and getting window list
    check_and_reassign_windows(&app.harpoon, app.windows, app.window_count);
    
    // Initialize history with current windows
    for (int i = 0; i < app.window_count && i < MAX_WINDOWS; i++) {
        app.history[i] = app.windows[i];
    }
    app.history_count = app.window_count;
    
    // Initialize filtered list with all windows (this will process history)
    filter_windows(&app, "");
    
    log_trace("First 3 windows in history after filter:");
    for (int i = 0; i < 3 && i < app.history_count; i++) {
        log_trace("  [%d] %s (0x%lx)", i, app.history[i].title, app.history[i].id);
    }
    
    // Setup GUI
    setup_application(&app, alignment);
    
    // Set app data for instance manager and setup signal handler
    // Do this after GUI setup so the window exists
    instance_manager_set_app_data(&app);
    instance_manager_setup_signal_handler();
    
    // Setup X11 event monitoring for dynamic window list updates
    setup_x11_event_monitoring(&app);
    
    // Update display
    update_display(&app);
    
    // ALWAYS reset selection to 0 before showing window
    app.selected_index = 0;
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