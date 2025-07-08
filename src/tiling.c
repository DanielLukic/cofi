#include "tiling.h"
#include "log.h"
#include "x11_utils.h"
#include "workarea.h"
#include "size_hints.h"
#include "monitor_move.h"
#include <stdlib.h>
#include <string.h>
#include <X11/extensions/Xrandr.h>

// Monitor info structure (reusing from monitor_move.c)
typedef struct {
    int x, y;
    int width, height;
} MonitorInfo;

// XRandR helper functions (reused from monitor_move.c)
static int get_monitors_xrandr(Display *display, MonitorInfo **monitors);
static int get_window_monitor_xrandr(Display *display, int win_x, int win_y, int win_width, int win_height);

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

    XEvent unmaximize_event;
    memset(&unmaximize_event, 0, sizeof(unmaximize_event));
    unmaximize_event.type = ClientMessage;
    unmaximize_event.xclient.window = window_id;
    unmaximize_event.xclient.message_type = net_wm_state;
    unmaximize_event.xclient.format = 32;
    unmaximize_event.xclient.data.l[0] = 0; // _NET_WM_STATE_REMOVE
    unmaximize_event.xclient.data.l[1] = net_wm_state_maximized_horz;
    unmaximize_event.xclient.data.l[2] = net_wm_state_maximized_vert;

    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &unmaximize_event);
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
    if (current_monitor_index >= 0 && current_monitor_index < monitor_count) {
        monitor_x = monitors[current_monitor_index].x;
        monitor_y = monitors[current_monitor_index].y;
        monitor_width = monitors[current_monitor_index].width;
        monitor_height = monitors[current_monitor_index].height;
        log_debug("Using monitor %d: %dx%d at (%d,%d)", current_monitor_index, 
                  monitor_width, monitor_height, monitor_x, monitor_y);
    } else {
        // Fallback to screen dimensions
        Screen *screen = DefaultScreenOfDisplay(display);
        monitor_x = 0;
        monitor_y = 0;
        monitor_width = WidthOfScreen(screen);
        monitor_height = HeightOfScreen(screen);
        log_debug("Using fallback screen dimensions: %dx%d", monitor_width, monitor_height);
    }

    free(monitors);

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

    // Calculate target position and size based on tiling option
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
        case TILE_LEFT_QUARTER:
            x = work_x;
            y = work_y;
            width = work_width / 4;
            height = work_height;
            break;
        case TILE_RIGHT_QUARTER:
            x = work_x + (work_width * 3) / 4;
            y = work_y;
            width = work_width / 4;
            height = work_height;
            break;
        case TILE_TOP_QUARTER:
            x = work_x;
            y = work_y;
            width = work_width;
            height = work_height / 4;
            break;
        case TILE_BOTTOM_QUARTER:
            x = work_x;
            y = work_y + (work_height * 3) / 4;
            width = work_width;
            height = work_height / 4;
            break;
        case TILE_LEFT_TWO_THIRDS:
            x = work_x;
            y = work_y;
            width = (work_width * 2) / 3;
            height = work_height;
            break;
        case TILE_RIGHT_TWO_THIRDS:
            x = work_x + work_width / 3;
            y = work_y;
            width = (work_width * 2) / 3;
            height = work_height;
            break;
        case TILE_TOP_TWO_THIRDS:
            x = work_x;
            y = work_y;
            width = work_width;
            height = (work_height * 2) / 3;
            break;
        case TILE_BOTTOM_TWO_THIRDS:
            x = work_x;
            y = work_y + work_height / 3;
            width = work_width;
            height = (work_height * 2) / 3;
            break;
        case TILE_LEFT_THREE_QUARTERS:
            x = work_x;
            y = work_y;
            width = (work_width * 3) / 4;
            height = work_height;
            break;
        case TILE_RIGHT_THREE_QUARTERS:
            x = work_x + work_width / 4;
            y = work_y;
            width = (work_width * 3) / 4;
            height = work_height;
            break;
        case TILE_TOP_THREE_QUARTERS:
            x = work_x;
            y = work_y;
            width = work_width;
            height = (work_height * 3) / 4;
            break;
        case TILE_BOTTOM_THREE_QUARTERS:
            x = work_x;
            y = work_y + work_height / 4;
            width = work_width;
            height = (work_height * 3) / 4;
            break;
        case TILE_GRID_1:
        case TILE_GRID_2:
        case TILE_GRID_3:
        case TILE_GRID_4:
        case TILE_GRID_5:
        case TILE_GRID_6:
        case TILE_GRID_7:
        case TILE_GRID_8:
        case TILE_GRID_9: {
            // Dynamic grid based on tile_columns
            int grid_position = option - TILE_GRID_1; // 0-based position
            int row = grid_position / tile_columns;
            int col = grid_position % tile_columns;
            
            width = work_width / tile_columns;
            height = work_height / 2; // Always 2 rows
            x = work_x + col * width;
            y = work_y + row * height;
            break;
        }
        case TILE_FULLSCREEN:
            // Toggle fullscreen state
            {
                Atom net_wm_state_fullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
                XEvent fullscreen_event;
                memset(&fullscreen_event, 0, sizeof(fullscreen_event));
                fullscreen_event.type = ClientMessage;
                fullscreen_event.xclient.window = window_id;
                fullscreen_event.xclient.message_type = net_wm_state;
                fullscreen_event.xclient.format = 32;
                fullscreen_event.xclient.data.l[0] = 2; // _NET_WM_STATE_TOGGLE
                fullscreen_event.xclient.data.l[1] = net_wm_state_fullscreen;
                
                XSendEvent(display, DefaultRootWindow(display), False,
                           SubstructureRedirectMask | SubstructureNotifyMask, &fullscreen_event);
                XFlush(display);
                log_info("Toggled fullscreen for window");
                return;
            }
        case TILE_CENTER:
            // Center window without resizing
            x = work_x + (work_width - window_width) / 2;
            y = work_y + (work_height - window_height) / 2;
            width = window_width;
            height = window_height;
            break;
        case TILE_CENTER_THIRD:
            // Center window at 1/3 of screen size
            width = work_width / 3;
            height = work_height / 3;
            x = work_x + (work_width - width) / 2;
            y = work_y + (work_height - height) / 2;
            break;
        case TILE_CENTER_TWO_THIRDS:
            // Center window at 2/3 of screen size
            width = (work_width * 2) / 3;
            height = (work_height * 2) / 3;
            x = work_x + (work_width - width) / 2;
            y = work_y + (work_height - height) / 2;
            break;
        case TILE_CENTER_THREE_QUARTERS:
            // Center window at 3/4 of screen size
            width = (work_width * 3) / 4;
            height = (work_height * 3) / 4;
            x = work_x + (work_width - width) / 2;
            y = work_y + (work_height - height) / 2;
            break;
        case TILE_GRID_1_NARROW:
        case TILE_GRID_2_NARROW:
        case TILE_GRID_3_NARROW:
        case TILE_GRID_4_NARROW:
        case TILE_GRID_5_NARROW:
        case TILE_GRID_6_NARROW:
        case TILE_GRID_7_NARROW:
        case TILE_GRID_8_NARROW:
        case TILE_GRID_9_NARROW: {
            // Grid positions with narrow width (1/3 of tile width)
            int grid_position = option - TILE_GRID_1_NARROW;
            int row = grid_position / tile_columns;
            int col = grid_position % tile_columns;
            
            int tile_width = work_width / tile_columns;
            width = tile_width / 3;
            height = work_height / 2; // Normal height
            x = work_x + col * tile_width;
            y = work_y + row * height;
            break;
        }
        case TILE_GRID_1_WIDE:
        case TILE_GRID_2_WIDE:
        case TILE_GRID_3_WIDE:
        case TILE_GRID_4_WIDE:
        case TILE_GRID_5_WIDE:
        case TILE_GRID_6_WIDE:
        case TILE_GRID_7_WIDE:
        case TILE_GRID_8_WIDE:
        case TILE_GRID_9_WIDE: {
            // Grid positions with wide width (3/2 of tile width)
            int grid_position = option - TILE_GRID_1_WIDE;
            int row = grid_position / tile_columns;
            int col = grid_position % tile_columns;
            
            int tile_width = work_width / tile_columns;
            width = (tile_width * 3) / 2;
            height = work_height / 2; // Normal height
            x = work_x + col * tile_width;
            y = work_y + row * height;
            break;
        }
        case TILE_GRID_1_WIDER:
        case TILE_GRID_2_WIDER:
        case TILE_GRID_3_WIDER:
        case TILE_GRID_4_WIDER:
        case TILE_GRID_5_WIDER:
        case TILE_GRID_6_WIDER:
        case TILE_GRID_7_WIDER:
        case TILE_GRID_8_WIDER:
        case TILE_GRID_9_WIDER: {
            // Grid positions with wider width (4/3 of tile width)
            int grid_position = option - TILE_GRID_1_WIDER;
            int row = grid_position / tile_columns;
            int col = grid_position % tile_columns;
            
            int tile_width = work_width / tile_columns;
            width = (tile_width * 4) / 3;
            height = work_height / 2; // Normal height
            x = work_x + col * tile_width;
            y = work_y + row * height;
            break;
        }
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
    
    switch (option) {
        case TILE_LEFT_HALF:
        case TILE_RIGHT_HALF:
        case TILE_LEFT_QUARTER:
        case TILE_RIGHT_QUARTER:
        case TILE_LEFT_TWO_THIRDS:
        case TILE_RIGHT_TWO_THIRDS:
        case TILE_LEFT_THREE_QUARTERS:
        case TILE_RIGHT_THREE_QUARTERS:
            // Maximize vertically for left/right tiles (like Marco)
            maximize_event.xclient.data.l[1] = net_wm_state_maximized_vert;
            XSendEvent(display, DefaultRootWindow(display), False,
                       SubstructureRedirectMask | SubstructureNotifyMask, &maximize_event);
            log_debug("Applied vertical maximization for left/right tiling");
            break;
            
        case TILE_TOP_HALF:
        case TILE_BOTTOM_HALF:
        case TILE_TOP_QUARTER:
        case TILE_BOTTOM_QUARTER:
        case TILE_TOP_TWO_THIRDS:
        case TILE_BOTTOM_TWO_THIRDS:
        case TILE_TOP_THREE_QUARTERS:
        case TILE_BOTTOM_THREE_QUARTERS:
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

// Get monitor information using XRandR
static int get_monitors_xrandr(Display *display, MonitorInfo **monitors) {
    int monitor_count = 0;
    *monitors = NULL;

    // Check if XRandR extension is available
    int xrandr_event_base, xrandr_error_base;
    if (!XRRQueryExtension(display, &xrandr_event_base, &xrandr_error_base)) {
        log_debug("XRandR extension not available");
        return 0;
    }

    // Get screen resources
    Window root = DefaultRootWindow(display);
    XRRScreenResources *screen_resources = XRRGetScreenResources(display, root);
    if (!screen_resources) {
        log_debug("Failed to get XRandR screen resources");
        return 0;
    }

    // Count active outputs
    for (int i = 0; i < screen_resources->noutput; i++) {
        XRROutputInfo *output_info = XRRGetOutputInfo(display, screen_resources, screen_resources->outputs[i]);
        if (output_info && output_info->connection == RR_Connected && output_info->crtc != None) {
            monitor_count++;
        }
        if (output_info) XRRFreeOutputInfo(output_info);
    }

    if (monitor_count == 0) {
        XRRFreeScreenResources(screen_resources);
        return 0;
    }

    // Allocate monitor array
    *monitors = malloc(monitor_count * sizeof(MonitorInfo));
    if (!*monitors) {
        XRRFreeScreenResources(screen_resources);
        return 0;
    }

    // Fill monitor information
    int monitor_index = 0;
    for (int i = 0; i < screen_resources->noutput && monitor_index < monitor_count; i++) {
        XRROutputInfo *output_info = XRRGetOutputInfo(display, screen_resources, screen_resources->outputs[i]);
        if (output_info && output_info->connection == RR_Connected && output_info->crtc != None) {
            XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(display, screen_resources, output_info->crtc);
            if (crtc_info) {
                (*monitors)[monitor_index].x = crtc_info->x;
                (*monitors)[monitor_index].y = crtc_info->y;
                (*monitors)[monitor_index].width = crtc_info->width;
                (*monitors)[monitor_index].height = crtc_info->height;
                monitor_index++;
                XRRFreeCrtcInfo(crtc_info);
            }
        }
        if (output_info) XRRFreeOutputInfo(output_info);
    }

    XRRFreeScreenResources(screen_resources);
    return monitor_index;
}

// Find which monitor a window is on using XRandR
static int get_window_monitor_xrandr(Display *display, int win_x, int win_y, int win_width, int win_height) {
    MonitorInfo *monitors;
    int monitor_count = get_monitors_xrandr(display, &monitors);

    if (monitor_count == 0) {
        return -1; // No monitors found
    }

    int current_monitor = 0; // Default to first monitor

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
