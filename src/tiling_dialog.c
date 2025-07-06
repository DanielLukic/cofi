#include "tiling_dialog.h"
#include "app_data.h"
#include "log.h"
#include "x11_utils.h"
#include "selection.h"
#include "display.h"
#include "monitor_move.h"
#include "workarea.h"
#include "size_hints.h"
#include <string.h>
#include <stdlib.h>
#include <X11/extensions/Xrandr.h>

// Monitor info structure (reusing from monitor_move.c)
typedef struct {
    int x, y;
    int width, height;
} MonitorInfo;

// Global dialog pointer for cleanup
static TilingDialog *g_tiling_dialog = NULL;

// Forward declarations
static void create_tiling_grid(TilingDialog *dialog);
static gboolean on_tiling_dialog_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data);
static gboolean on_tiling_dialog_focus_out(GtkWidget *widget, GdkEventFocus *event, gpointer data);
static void tile_and_close(TilingDialog *dialog, TileOption option);
static gboolean destroy_widget_idle(gpointer widget);

// XRandR helper functions (reused from monitor_move.c)
static int get_monitors_xrandr(Display *display, MonitorInfo **monitors);
static int get_window_monitor_xrandr(Display *display, int win_x, int win_y, int win_width, int win_height);

// Create and show the tiling dialog
void show_tiling_dialog(struct AppData *app) {
    AppData *appdata = (AppData *)app;  // Cast to remove struct
    if (!appdata || appdata->current_tab != TAB_WINDOWS || appdata->filtered_count == 0) {
        return;
    }
    
    // Set dialog active flag to prevent main window from closing on focus loss
    appdata->dialog_active = 1;
    
    // Get selected window using centralized selection management
    WindowInfo *selected_window = get_selected_window(appdata);
    if (!selected_window) {
        log_error("No window selected for tiling dialog");
        return;
    }
    
    log_debug("Creating tiling dialog for window: %s", selected_window->title);
    
    // Create dialog structure
    TilingDialog *dialog = g_new0(TilingDialog, 1);
    dialog->target_window = selected_window;
    dialog->display = appdata->display;
    dialog->app_data = (struct AppData *)appdata;
    dialog->option_selected = FALSE;
    g_tiling_dialog = dialog;
    
    // Create window
    dialog->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(dialog->window), "Tile Window");
    gtk_window_set_default_size(GTK_WINDOW(dialog->window), 500, 400);
    gtk_window_set_position(GTK_WINDOW(dialog->window), GTK_WIN_POS_CENTER);
    
    // Window properties similar to main COFI window
    gtk_window_set_decorated(GTK_WINDOW(dialog->window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog->window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(dialog->window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(dialog->window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(dialog->window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_modal(GTK_WINDOW(dialog->window), TRUE);
    
    // Main container
    dialog->content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->content_box), 20);
    gtk_container_add(GTK_CONTAINER(dialog->window), dialog->content_box);
    
    // Header with window info
    GtkWidget *header_label = gtk_label_new(NULL);
    
    // Escape window title for markup
    gchar *escaped_title = g_markup_escape_text(selected_window->title, -1);
    
    char header_text[1024];
    snprintf(header_text, sizeof(header_text), 
             "<b>Tile Window</b>\n\nWindow: %s",
             escaped_title);
    
    gtk_label_set_markup(GTK_LABEL(header_label), header_text);
    gtk_label_set_line_wrap(GTK_LABEL(header_label), TRUE);
    gtk_box_pack_start(GTK_BOX(dialog->content_box), header_label, FALSE, FALSE, 0);
    
    g_free(escaped_title);
    
    // Separator
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(dialog->content_box), separator, FALSE, FALSE, 0);
    
    // Create tiling options display
    create_tiling_grid(dialog);
    
    // Instructions (dynamic based on tile_columns)
    char instructions_text[256];
    int max_grid_num = appdata->config.tile_columns * 2;
    snprintf(instructions_text, sizeof(instructions_text), 
             "[Press L/R/T/B for halves, 1-%d for grid, F for fullscreen, C for center, Esc to cancel]", 
             max_grid_num);
    
    GtkWidget *instructions = gtk_label_new(instructions_text);
    gtk_widget_set_halign(instructions, GTK_ALIGN_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(instructions), TRUE);
    gtk_box_pack_end(GTK_BOX(dialog->content_box), instructions, FALSE, FALSE, 0);
    
    // Connect signals
    g_signal_connect(dialog->window, "key-press-event", G_CALLBACK(on_tiling_dialog_key_press), dialog);
    g_signal_connect(dialog->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(dialog->window, "focus-out-event", G_CALLBACK(on_tiling_dialog_focus_out), dialog);
    
    // Show all widgets
    gtk_widget_show_all(dialog->window);
    
    // Ensure the dialog window is realized before trying to grab focus
    gtk_widget_realize(dialog->window);
    
    // Present the window to ensure it's on top
    gtk_window_present(GTK_WINDOW(dialog->window));
    
    // Force focus to the dialog window
    gtk_widget_grab_focus(dialog->window);
    gdk_window_focus(gtk_widget_get_window(dialog->window), GDK_CURRENT_TIME);
    
    // Run dialog event loop
    gtk_main();
    
    // Clear dialog active flag
    appdata->dialog_active = 0;
    
    // Cleanup
    g_tiling_dialog = NULL;
    g_free(dialog);
}

// Create the tiling options grid display
static void create_tiling_grid(TilingDialog *dialog) {
    // Create a visual representation of tiling options
    GtkWidget *grid_frame = gtk_frame_new("Tiling Options");
    gtk_box_pack_start(GTK_BOX(dialog->content_box), grid_frame, TRUE, TRUE, 0);
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(grid_frame), main_box);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 15);
    
    // Half-screen options
    GtkWidget *halves_label = gtk_label_new("<b>Half Screen:</b>");
    gtk_label_set_use_markup(GTK_LABEL(halves_label), TRUE);
    gtk_widget_set_halign(halves_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), halves_label, FALSE, FALSE, 0);
    
    GtkWidget *halves_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(main_box), halves_box, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(halves_box), gtk_label_new("L - Left Half"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(halves_box), gtk_label_new("R - Right Half"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(halves_box), gtk_label_new("T - Top Half"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(halves_box), gtk_label_new("B - Bottom Half"), FALSE, FALSE, 0);
    
    // Dynamic Grid (get tile_columns from config)
    AppData *appdata = (AppData *)dialog->app_data;
    int tile_columns = appdata->config.tile_columns;
    
    char grid_label_text[64];
    snprintf(grid_label_text, sizeof(grid_label_text), "<b>%dx2 Grid:</b>", tile_columns);
    GtkWidget *grid_label = gtk_label_new(grid_label_text);
    gtk_label_set_use_markup(GTK_LABEL(grid_label), TRUE);
    gtk_widget_set_halign(grid_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), grid_label, FALSE, FALSE, 0);
    
    // Create dynamic visual grid
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    gtk_box_pack_start(GTK_BOX(main_box), grid, FALSE, FALSE, 0);
    
    // Add grid buttons based on tile_columns
    int max_positions = tile_columns * 2;  // columns * 2 rows
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < tile_columns; col++) {
            int num = row * tile_columns + col + 1;
            if (num <= max_positions) {
                char label_text[16];
                snprintf(label_text, sizeof(label_text), "%d", num);
                
                GtkWidget *button = gtk_button_new_with_label(label_text);
                gtk_widget_set_size_request(button, 40, 30);
                gtk_grid_attach(GTK_GRID(grid), button, col, row, 1, 1);
            }
        }
    }
    
    // Other options
    GtkWidget *other_label = gtk_label_new("<b>Other:</b>");
    gtk_label_set_use_markup(GTK_LABEL(other_label), TRUE);
    gtk_widget_set_halign(other_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), other_label, FALSE, FALSE, 0);
    
    GtkWidget *other_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(main_box), other_box, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(other_box), gtk_label_new("F - Fullscreen"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(other_box), gtk_label_new("C - Center (no resize)"), FALSE, FALSE, 0);
}

// Handle key press events in tiling dialog
static gboolean on_tiling_dialog_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget;  // Unused parameter
    TilingDialog *dialog = (TilingDialog *)data;

    // Handle Escape
    if (event->keyval == GDK_KEY_Escape) {
        gtk_widget_destroy(dialog->window);
        return TRUE;
    }

    TileOption option;
    gboolean valid_option = TRUE;

    // Get tile_columns from config for validation
    AppData *appdata = (AppData *)dialog->app_data;
    int tile_columns = appdata->config.tile_columns;
    int max_positions = tile_columns * 2;  // columns * 2 rows

    // Handle tiling options
    switch (event->keyval) {
        case GDK_KEY_l:
        case GDK_KEY_L:
            option = TILE_LEFT_HALF;
            break;
        case GDK_KEY_r:
        case GDK_KEY_R:
            option = TILE_RIGHT_HALF;
            break;
        case GDK_KEY_t:
        case GDK_KEY_T:
            option = TILE_TOP_HALF;
            break;
        case GDK_KEY_b:
        case GDK_KEY_B:
            option = TILE_BOTTOM_HALF;
            break;
        case GDK_KEY_1:
            option = TILE_GRID_1;
            valid_option = (1 <= max_positions);
            break;
        case GDK_KEY_2:
            option = TILE_GRID_2;
            valid_option = (2 <= max_positions);
            break;
        case GDK_KEY_3:
            option = TILE_GRID_3;
            valid_option = (3 <= max_positions);
            break;
        case GDK_KEY_4:
            option = TILE_GRID_4;
            valid_option = (4 <= max_positions);
            break;
        case GDK_KEY_5:
            option = TILE_GRID_5;
            valid_option = (5 <= max_positions);
            break;
        case GDK_KEY_6:
            option = TILE_GRID_6;
            valid_option = (6 <= max_positions);
            break;
        case GDK_KEY_7:
            option = TILE_GRID_7;
            valid_option = (7 <= max_positions);
            break;
        case GDK_KEY_8:
            option = TILE_GRID_8;
            valid_option = (8 <= max_positions);
            break;
        case GDK_KEY_9:
            option = TILE_GRID_9;
            valid_option = (9 <= max_positions);
            break;
        case GDK_KEY_f:
        case GDK_KEY_F:
            option = TILE_FULLSCREEN;
            break;
        case GDK_KEY_c:
        case GDK_KEY_C:
            option = TILE_CENTER;
            break;
        default:
            valid_option = FALSE;
            break;
    }

    if (valid_option) {
        tile_and_close(dialog, option);
        return TRUE;
    }

    return FALSE;
}

// Handle focus out events
static gboolean on_tiling_dialog_focus_out(GtkWidget *widget, GdkEventFocus *event, gpointer data) {
    (void)widget;  // Unused parameter
    (void)event;   // Unused parameter
    TilingDialog *dialog = (TilingDialog *)data;

    log_debug("Dialog lost focus, closing dialog and main window");

    // Use idle callback to avoid destroying widget during event handling
    g_idle_add(destroy_widget_idle, dialog->window);

    return FALSE;
}

// Apply tiling and close dialog
static void tile_and_close(TilingDialog *dialog, TileOption option) {
    if (!dialog->target_window) {
        return;
    }

    // Mark that an option was selected
    dialog->option_selected = TRUE;

    log_info("USER: Tiling window '%s' with option %d",
             dialog->target_window->title, option);

    // Apply the tiling
    AppData *appdata = (AppData *)dialog->app_data;
    apply_tiling(dialog->display, dialog->target_window->id, option, appdata->config.tile_columns);

    // Store the window ID we need to activate
    Window target_window_id = dialog->target_window->id;

    // Close dialog first
    gtk_widget_destroy(dialog->window);

    // Activate the tiled window
    activate_window(target_window_id);

    // Close the main window - this will trigger proper cleanup
    if (appdata && appdata->window) {
        gtk_widget_destroy(appdata->window);
    }
}

// Idle callback to destroy widget safely
static gboolean destroy_widget_idle(gpointer widget) {
    if (GTK_IS_WIDGET(widget)) {
        gtk_widget_destroy(GTK_WIDGET(widget));
    }
    return FALSE; // Remove from idle queue
}

// Apply tiling to window
void apply_tiling(Display *display, Window window_id, TileOption option, int tile_columns) {
    if (!display || !window_id) {
        log_error("Invalid display or window for tiling");
        return;
    }

    // First, unmaximize the window if it's maximized
    // This is crucial - maximized windows won't move/resize properly
    log_debug("Unmaximizing window before tiling");

    // Remove all maximize states
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom net_wm_state_maximized_horz = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom net_wm_state_maximized_vert = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);

    // Send client message to remove maximize states
    XEvent event;
    memset(&event, 0, sizeof(event));
    event.type = ClientMessage;
    event.xclient.window = window_id;
    event.xclient.message_type = net_wm_state;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 0; // _NET_WM_STATE_REMOVE
    event.xclient.data.l[1] = net_wm_state_maximized_horz;
    event.xclient.data.l[2] = net_wm_state_maximized_vert;
    event.xclient.data.l[3] = 1; // Source indication (application)

    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display);

    // Get the current window geometry using existing function
    int window_x, window_y, window_width, window_height;
    if (!get_window_geometry(display, window_id, &window_x, &window_y, &window_width, &window_height)) {
        log_error("Failed to get window geometry for tiling");
        return;
    }

    // Find which monitor the window is currently on using XRandR
    int current_monitor_index = get_window_monitor_xrandr(display, window_x, window_y, window_width, window_height);

    // Get monitor information
    MonitorInfo *monitors;
    int monitor_count = get_monitors_xrandr(display, &monitors);

    int monitor_x, monitor_y, monitor_width, monitor_height;

    if (monitor_count > 0 && monitors && current_monitor_index >= 0 && current_monitor_index < monitor_count) {
        // Use the specific monitor the window is on
        MonitorInfo current_monitor = monitors[current_monitor_index];
        monitor_x = current_monitor.x;
        monitor_y = current_monitor.y;
        monitor_width = current_monitor.width;
        monitor_height = current_monitor.height;

        log_debug("Tiling on monitor %d: %dx%d at (%d,%d)", current_monitor_index,
                  monitor_width, monitor_height, monitor_x, monitor_y);
    } else {
        // Fallback to default screen if XRandR fails
        log_warn("Could not detect monitor via XRandR, using default screen");
        Screen *screen = DefaultScreenOfDisplay(display);
        monitor_x = 0;
        monitor_y = 0;
        monitor_width = WidthOfScreen(screen);
        monitor_height = HeightOfScreen(screen);
    }

    // Clean up monitor list
    if (monitors) {
        free(monitors);
    }

    // Get the actual work area (excluding panels/docks)
    WorkArea work_area;
    get_current_work_area(display, &work_area);
    
    // Intersect work area with monitor bounds
    int work_x = (work_area.x > monitor_x) ? work_area.x : monitor_x;
    int work_y = (work_area.y > monitor_y) ? work_area.y : monitor_y;
    int work_right = (work_area.x + work_area.width < monitor_x + monitor_width) ? 
                     work_area.x + work_area.width : monitor_x + monitor_width;
    int work_bottom = (work_area.y + work_area.height < monitor_y + monitor_height) ?
                      work_area.y + work_area.height : monitor_y + monitor_height;
    int work_width = work_right - work_x;
    int work_height = work_bottom - work_y;
    
    log_debug("Work area on monitor: %dx%d+%d+%d", work_width, work_height, work_x, work_y);
    
    // Get window size hints
    WindowSizeHints size_hints;
    get_window_size_hints(display, window_id, &size_hints);

    int x, y, width, height;

    switch (option) {
        case TILE_LEFT_HALF:
            x = work_x;
            y = work_y;
            width = work_width / 2;
            height = work_height;
            break;

        case TILE_RIGHT_HALF:
            x = work_x + work_width / 2;
            y = work_y;
            width = work_width / 2;
            height = work_height;
            break;

        case TILE_TOP_HALF:
            x = work_x;
            y = work_y;
            width = work_width;
            height = work_height / 2;
            break;

        case TILE_BOTTOM_HALF:
            x = work_x;
            y = work_y + work_height / 2;
            width = work_width;
            height = work_height / 2;
            break;

        // Dynamic Grid positions (configurable columns)
        case TILE_GRID_1:
        case TILE_GRID_2:
        case TILE_GRID_3:
        case TILE_GRID_4:
        case TILE_GRID_5:
        case TILE_GRID_6:
        case TILE_GRID_7:
        case TILE_GRID_8:
        case TILE_GRID_9:
            {
                // Calculate position based on tile_columns
                int position = option - TILE_GRID_1;  // 0-based position
                int max_positions = tile_columns * 2;  // Total grid positions (columns * 2 rows)
                
                // Validate position is within bounds
                if (position >= max_positions) {
                    log_error("Invalid grid position %d for %d columns", position + 1, tile_columns);
                    return;
                }
                
                int col = position % tile_columns;     // Column (0 to tile_columns-1)
                int row = position / tile_columns;     // Row (0 to 1)
                
                x = work_x + (col * work_width) / tile_columns;
                y = work_y + (row * work_height) / 2;
                width = work_width / tile_columns;
                height = work_height / 2;
                
                log_debug("Grid position %d: col=%d, row=%d, %dx%d+%d+%d", 
                         position + 1, col, row, width, height, x, y);
            }
            break;

        case TILE_FULLSCREEN:
            // Toggle fullscreen state
            toggle_window_state(display, window_id, "_NET_WM_STATE_FULLSCREEN");
            return; // Don't do XMoveResizeWindow for fullscreen

        case TILE_CENTER:
            // Center window without resizing (on the monitor it's currently on)
            {
                // Get current window size
                Window root;
                int current_x, current_y;
                unsigned int current_width, current_height, border_width, depth;
                if (XGetGeometry(display, window_id, &root, &current_x, &current_y,
                                &current_width, &current_height, &border_width, &depth) == 0) {
                    log_error("Failed to get window geometry for centering");
                    return;
                }

                // Center the window in the work area
                x = work_x + (work_width - current_width) / 2;
                y = work_y + (work_height - current_height) / 2;
                width = current_width;
                height = current_height;
            }
            break;

        default:
            log_error("Unknown tiling option: %d", option);
            return;
    }

    log_debug("Tiling window to position: x=%d, y=%d, width=%d, height=%d", x, y, width, height);
    
    // Enforce size hints
    ensure_size_hints_satisfied(&x, &y, &width, &height, &size_hints);
    log_debug("After size hints: x=%d, y=%d, width=%d, height=%d", x, y, width, height);

    // First, move and resize the window
    XMoveResizeWindow(display, window_id, x, y, width, height);
    XFlush(display);
    
    // Then apply maximization for certain tile modes (like Marco does)
    // This helps fill gaps caused by size increment constraints
    // (reuse atoms already defined above)
    
    XEvent maximize_event;
    memset(&maximize_event, 0, sizeof(maximize_event));
    maximize_event.type = ClientMessage;
    maximize_event.xclient.window = window_id;
    maximize_event.xclient.message_type = net_wm_state;
    maximize_event.xclient.format = 32;
    maximize_event.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
    maximize_event.xclient.data.l[3] = 1; // Source indication
    
    switch (option) {
        case TILE_LEFT_HALF:
        case TILE_RIGHT_HALF:
            // Maximize vertically for left/right tiles (like Marco)
            maximize_event.xclient.data.l[1] = net_wm_state_maximized_vert;
            XSendEvent(display, DefaultRootWindow(display), False,
                       SubstructureRedirectMask | SubstructureNotifyMask, &maximize_event);
            log_debug("Applied vertical maximization for left/right tiling");
            break;
            
        case TILE_TOP_HALF:
        case TILE_BOTTOM_HALF:
            // Maximize horizontally for top/bottom tiles
            maximize_event.xclient.data.l[1] = net_wm_state_maximized_horz;
            XSendEvent(display, DefaultRootWindow(display), False,
                       SubstructureRedirectMask | SubstructureNotifyMask, &maximize_event);
            log_debug("Applied horizontal maximization for top/bottom tiling");
            break;
            
        // Note: For 3x3 grid tiles, we don't maximize since they should remain
        // exactly 1/3 of the screen size
            
        default:
            // No maximization for other tile modes
            break;
    }
    
    XFlush(display);
    log_info("Applied tiling option %d to window", option);
}

// XRandR wrapper to get monitor information (reused from monitor_move.c)
static int get_monitors_xrandr(Display *display, MonitorInfo **monitors) {
    Window root = DefaultRootWindow(display);
    XRRScreenResources *screen_resources;
    int monitor_count = 0;
    MonitorInfo *monitor_list = NULL;

    // Check if XRandR extension is available
    int xrandr_event_base, xrandr_error_base;
    if (!XRRQueryExtension(display, &xrandr_event_base, &xrandr_error_base)) {
        log_error("XRandR extension not available");
        *monitors = NULL;
        return 0;
    }

    screen_resources = XRRGetScreenResources(display, root);
    if (!screen_resources) {
        log_error("Failed to get XRandR screen resources");
        *monitors = NULL;
        return 0;
    }

    // Count active CRTCs (connected monitors)
    for (int i = 0; i < screen_resources->ncrtc; i++) {
        XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(display, screen_resources, screen_resources->crtcs[i]);
        if (crtc_info && crtc_info->noutput > 0 && crtc_info->mode != None) {
            monitor_count++;
        }
        if (crtc_info) XRRFreeCrtcInfo(crtc_info);
    }

    if (monitor_count == 0) {
        log_warn("No active monitors found via XRandR");
        XRRFreeScreenResources(screen_resources);
        *monitors = NULL;
        return 0;
    }

    // Allocate memory for monitor list
    monitor_list = malloc(monitor_count * sizeof(MonitorInfo));
    if (!monitor_list) {
        log_error("Failed to allocate memory for monitor list");
        XRRFreeScreenResources(screen_resources);
        *monitors = NULL;
        return 0;
    }

    // Fill monitor information
    int monitor_index = 0;
    for (int i = 0; i < screen_resources->ncrtc && monitor_index < monitor_count; i++) {
        XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(display, screen_resources, screen_resources->crtcs[i]);
        if (crtc_info && crtc_info->noutput > 0 && crtc_info->mode != None) {
            monitor_list[monitor_index].x = crtc_info->x;
            monitor_list[monitor_index].y = crtc_info->y;
            monitor_list[monitor_index].width = crtc_info->width;
            monitor_list[monitor_index].height = crtc_info->height;

            log_debug("Monitor %d: %dx%d at (%d,%d)", monitor_index,
                     monitor_list[monitor_index].width, monitor_list[monitor_index].height,
                     monitor_list[monitor_index].x, monitor_list[monitor_index].y);

            monitor_index++;
        }
        if (crtc_info) XRRFreeCrtcInfo(crtc_info);
    }

    XRRFreeScreenResources(screen_resources);
    *monitors = monitor_list;
    return monitor_count;
}

// Find which monitor a window center is on using XRandR (reused from monitor_move.c)
static int get_window_monitor_xrandr(Display *display, int win_x, int win_y, int win_width, int win_height) {
    MonitorInfo *monitors;
    int monitor_count = get_monitors_xrandr(display, &monitors);
    int current_monitor = -1;

    if (monitor_count == 0 || !monitors) {
        return -1;
    }

    // Check if window center is on any monitor
    int win_center_x = win_x + win_width / 2;
    int win_center_y = win_y + win_height / 2;

    log_debug("Window center: (%d, %d)", win_center_x, win_center_y);

    for (int i = 0; i < monitor_count; i++) {
        if (win_center_x >= monitors[i].x && win_center_x < monitors[i].x + monitors[i].width &&
            win_center_y >= monitors[i].y && win_center_y < monitors[i].y + monitors[i].height) {
            current_monitor = i;
            log_debug("Window is on monitor %d", i);
            break;
        }
    }

    free(monitors);
    return current_monitor;
}
