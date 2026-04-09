#include "tiling.h"
#include "log.h"
#include "x11_utils.h"
#include "workarea.h"
#include "size_hints.h"
#include "monitor_move.h"
#include "frame_extents.h"
#include <stdlib.h>
#include <string.h>
#include <X11/extensions/Xrandr.h>

// Monitor info structure (reusing from monitor_move.c)
typedef struct {
    int x, y;
    int width, height;
} MonitorInfo;

// Tiling calculation structure
typedef struct {
    int x, y;
    int width, height;
} TileGeometry;

// XRandR helper functions (reused from monitor_move.c)
static int get_monitors_xrandr(Display *display, MonitorInfo **monitors);
static int get_window_monitor_xrandr(Display *display, int win_x, int win_y, int win_width, int win_height);

// New helper functions
static void unmaximize_window(Display *display, Window window_id);
static void get_target_work_area(Display *display, Window window_id, WorkArea *work_area);
static void calculate_tile_geometry(TileOption option, const WorkArea *work_area, int tile_columns, TileGeometry *geometry);
static void apply_window_position(Display *display, Window window_id, const TileGeometry *geometry, const WindowSizeHints *size_hints);
static void apply_maximization_hints(Display *display, Window window_id, TileOption option);

// Unmaximize window before tiling
static void unmaximize_window(Display *display, Window window_id) {
    log_debug("Unmaximizing window before tiling");
    
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
}

// Get the work area for the monitor containing the window
static void get_target_work_area(Display *display, Window window_id, WorkArea *work_area) {
    // Get current window geometry
    int window_x, window_y, window_width, window_height;
    if (!get_window_geometry(display, window_id, &window_x, &window_y, &window_width, &window_height)) {
        log_error("Failed to get window geometry for tiling");
        // Fallback to screen dimensions
        Screen *screen = DefaultScreenOfDisplay(display);
        work_area->x = 0;
        work_area->y = 0;
        work_area->width = WidthOfScreen(screen);
        work_area->height = HeightOfScreen(screen);
        return;
    }
    
    // Find which monitor the window is on
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
    WorkArea desktop_work_area;
    get_current_work_area(display, &desktop_work_area);
    
    // Intersect work area with monitor bounds
    int work_x = (desktop_work_area.x > monitor_x) ? desktop_work_area.x : monitor_x;
    int work_y = (desktop_work_area.y > monitor_y) ? desktop_work_area.y : monitor_y;
    int work_right = (desktop_work_area.x + desktop_work_area.width < monitor_x + monitor_width) ?
                     desktop_work_area.x + desktop_work_area.width : monitor_x + monitor_width;
    int work_bottom = (desktop_work_area.y + desktop_work_area.height < monitor_y + monitor_height) ?
                      desktop_work_area.y + desktop_work_area.height : monitor_y + monitor_height;
    
    work_area->x = work_x;
    work_area->y = work_y;
    work_area->width = work_right - work_x;
    work_area->height = work_bottom - work_y;
    
    log_debug("Work area on monitor: %dx%d+%d+%d", 
              work_area->width, work_area->height, work_area->x, work_area->y);
}

// Apply window position with size hints
static void apply_window_position(Display *display, Window window_id, 
                                const TileGeometry *geometry, const WindowSizeHints *size_hints) {
    int x = geometry->x;
    int y = geometry->y;
    int width = geometry->width;
    int height = geometry->height;
    
    log_debug("Applying window position: x=%d, y=%d, width=%d, height=%d", x, y, width, height);
    
    // Account for window frame extents (borders and decorations)
    adjust_for_frame_extents(display, window_id, &width, &height);
    log_debug("After frame adjustment: width=%d, height=%d", width, height);
    
    // Enforce size hints
    ensure_size_hints_satisfied(&x, &y, &width, &height, (WindowSizeHints *)size_hints);
    log_debug("After size hints: x=%d, y=%d, width=%d, height=%d", x, y, width, height);
    
    // Move and resize the window
    XMoveResizeWindow(display, window_id, x, y, width, height);
    XFlush(display);
}

// Apply maximization hints for certain tile modes
static void apply_maximization_hints(Display *display, Window window_id, TileOption option) {
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom net_wm_state_maximized_horz = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom net_wm_state_maximized_vert = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    
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
            // Maximize vertically for left/right tiles
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
            
        default:
            // No maximization for other tile modes
            break;
    }
    
    XFlush(display);
}

// Apply tiling to window
void apply_tiling(Display *display, Window window_id, TileOption option, int tile_columns) {
    if (!display || !window_id) {
        log_error("Invalid display or window for tiling");
        return;
    }
    
    // Handle fullscreen toggle separately
    if (option == TILE_FULLSCREEN) {
        Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);
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
    
    // Unmaximize the window first
    unmaximize_window(display, window_id);
    
    // Get the work area for tiling
    WorkArea work_area;
    get_target_work_area(display, window_id, &work_area);
    
    // Get window size hints
    WindowSizeHints size_hints;
    get_window_size_hints(display, window_id, &size_hints);
    
    // Calculate tile geometry
    TileGeometry geometry;
    calculate_tile_geometry(option, &work_area, tile_columns, &geometry);
    
    // Apply the position and size
    apply_window_position(display, window_id, &geometry, &size_hints);
    
    // Apply maximization hints for certain tile modes
    apply_maximization_hints(display, window_id, option);
    
    log_info("Applied tiling option %d to window", option);
}

// Calculate tile geometry based on tiling option
static void calculate_tile_geometry(TileOption option, const WorkArea *work_area, 
                                  int tile_columns, TileGeometry *geometry) {
    int work_x = work_area->x;
    int work_y = work_area->y;
    int work_width = work_area->width;
    int work_height = work_area->height;

    switch (option) {
        case TILE_LEFT_HALF:
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = work_width / 2;
            geometry->height = work_height;
            break;
        case TILE_RIGHT_HALF:
            geometry->x = work_x + work_width / 2;
            geometry->y = work_y;
            geometry->width = work_width / 2;
            geometry->height = work_height;
            break;
        case TILE_TOP_HALF:
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = work_width;
            geometry->height = work_height / 2;
            break;
        case TILE_BOTTOM_HALF:
            geometry->x = work_x;
            geometry->y = work_y + work_height / 2;
            geometry->width = work_width;
            geometry->height = work_height / 2;
            break;
        case TILE_LEFT_QUARTER:
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = work_width / 4;
            geometry->height = work_height;
            break;
        case TILE_RIGHT_QUARTER:
            geometry->x = work_x + (work_width * 3) / 4;
            geometry->y = work_y;
            geometry->width = work_width / 4;
            geometry->height = work_height;
            break;
        case TILE_TOP_QUARTER:
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = work_width;
            geometry->height = work_height / 4;
            break;
        case TILE_BOTTOM_QUARTER:
            geometry->x = work_x;
            geometry->y = work_y + (work_height * 3) / 4;
            geometry->width = work_width;
            geometry->height = work_height / 4;
            break;
        case TILE_LEFT_TWO_THIRDS:
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = (work_width * 2) / 3;
            geometry->height = work_height;
            break;
        case TILE_RIGHT_TWO_THIRDS:
            geometry->x = work_x + work_width / 3;
            geometry->y = work_y;
            geometry->width = (work_width * 2) / 3;
            geometry->height = work_height;
            break;
        case TILE_TOP_TWO_THIRDS:
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = work_width;
            geometry->height = (work_height * 2) / 3;
            break;
        case TILE_BOTTOM_TWO_THIRDS:
            geometry->x = work_x;
            geometry->y = work_y + work_height / 3;
            geometry->width = work_width;
            geometry->height = (work_height * 2) / 3;
            break;
        case TILE_LEFT_THREE_QUARTERS:
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = (work_width * 3) / 4;
            geometry->height = work_height;
            break;
        case TILE_RIGHT_THREE_QUARTERS:
            geometry->x = work_x + work_width / 4;
            geometry->y = work_y;
            geometry->width = (work_width * 3) / 4;
            geometry->height = work_height;
            break;
        case TILE_TOP_THREE_QUARTERS:
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = work_width;
            geometry->height = (work_height * 3) / 4;
            break;
        case TILE_BOTTOM_THREE_QUARTERS:
            geometry->x = work_x;
            geometry->y = work_y + work_height / 4;
            geometry->width = work_width;
            geometry->height = (work_height * 3) / 4;
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
            int grid_position = option - TILE_GRID_1;
            int row = grid_position / tile_columns;
            int col = grid_position % tile_columns;
            
            geometry->width = work_width / tile_columns;
            geometry->height = work_height / 2;
            geometry->x = work_x + col * geometry->width;
            geometry->y = work_y + row * geometry->height;
            break;
        }
        case TILE_CENTER:
            // Center window at 50% size
            geometry->width = work_width / 2;
            geometry->height = work_height / 2;
            geometry->x = work_x + (work_width - geometry->width) / 2;
            geometry->y = work_y + (work_height - geometry->height) / 2;
            break;
        case TILE_CENTER_THIRD:
            geometry->width = work_width / 3;
            geometry->height = work_height / 3;
            geometry->x = work_x + (work_width - geometry->width) / 2;
            geometry->y = work_y + (work_height - geometry->height) / 2;
            break;
        case TILE_CENTER_TWO_THIRDS:
            geometry->width = (work_width * 2) / 3;
            geometry->height = (work_height * 2) / 3;
            geometry->x = work_x + (work_width - geometry->width) / 2;
            geometry->y = work_y + (work_height - geometry->height) / 2;
            break;
        case TILE_CENTER_THREE_QUARTERS:
            geometry->width = (work_width * 3) / 4;
            geometry->height = (work_height * 3) / 4;
            geometry->x = work_x + (work_width - geometry->width) / 2;
            geometry->y = work_y + (work_height - geometry->height) / 2;
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
            int grid_position = option - TILE_GRID_1_NARROW;
            int row = grid_position / tile_columns;
            int col = grid_position % tile_columns;
            
            int tile_width = work_width / tile_columns;
            geometry->width = tile_width / 3;
            geometry->height = work_height / 2;
            geometry->x = work_x + col * tile_width;
            geometry->y = work_y + row * geometry->height;
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
            int grid_position = option - TILE_GRID_1_WIDE;
            int row = grid_position / tile_columns;
            int col = grid_position % tile_columns;
            
            int tile_width = work_width / tile_columns;
            geometry->width = (tile_width * 3) / 2;
            geometry->height = work_height / 2;
            geometry->x = work_x + col * tile_width;
            geometry->y = work_y + row * geometry->height;
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
            int grid_position = option - TILE_GRID_1_WIDER;
            int row = grid_position / tile_columns;
            int col = grid_position % tile_columns;
            
            int tile_width = work_width / tile_columns;
            geometry->width = (tile_width * 4) / 3;
            geometry->height = work_height / 2;
            geometry->x = work_x + col * tile_width;
            geometry->y = work_y + row * geometry->height;
            break;
        }
        default:
            log_error("Unknown tiling option: %d", option);
            // Set safe defaults
            geometry->x = work_x;
            geometry->y = work_y;
            geometry->width = work_width / 2;
            geometry->height = work_height / 2;
            break;
    }
    
    log_debug("Calculated tile geometry: x=%d, y=%d, width=%d, height=%d", 
              geometry->x, geometry->y, geometry->width, geometry->height);
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