#include "monitor_move.h"
#include "app_data.h"
#include "window_info.h"
#include "log.h"
#include "display.h"
#include "window_list.h"
#include "filter.h"
#include "selection.h"
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

// Monitor info structure
typedef struct {
    int x, y;
    int width, height;
} MonitorInfo;

// XRandR wrapper to get monitor information
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


// Get window geometry
gboolean get_window_geometry(Display *display, Window window, 
                           int *x, int *y, int *width, int *height) {
    Window root, parent;
    Window *children;
    unsigned int nchildren;
    XWindowAttributes attrs;
    int win_x, win_y;
    unsigned int win_width, win_height, border_width, depth;
    
    // Get the window attributes
    if (XGetWindowAttributes(display, window, &attrs) == 0) {
        log_error("Failed to get window attributes for window 0x%lx", window);
        return FALSE;
    }
    
    // Get the actual geometry including position
    if (XGetGeometry(display, window, &root, &win_x, &win_y, 
                     &win_width, &win_height, &border_width, &depth) == 0) {
        log_error("Failed to get geometry for window 0x%lx", window);
        return FALSE;
    }
    
    // Get the parent window to find the frame
    if (XQueryTree(display, window, &root, &parent, &children, &nchildren) == 0) {
        log_error("Failed to query tree for window 0x%lx", window);
        return FALSE;
    }
    if (children) XFree(children);
    
    // If the parent is not the root, we need to get the frame position
    if (parent != root) {
        XWindowAttributes parent_attrs;
        if (XGetWindowAttributes(display, parent, &parent_attrs) == 0) {
            log_error("Failed to get parent attributes for window 0x%lx", window);
            return FALSE;
        }
        
        // Get the frame position
        Window child;
        if (XTranslateCoordinates(display, parent, root, 0, 0, &win_x, &win_y, &child) == 0) {
            log_error("Failed to translate parent coordinates for window 0x%lx", window);
            return FALSE;
        }
    } else {
        // Window has no frame, use its own position
        Window child;
        if (XTranslateCoordinates(display, window, root, 0, 0, &win_x, &win_y, &child) == 0) {
            log_error("Failed to translate coordinates for window 0x%lx", window);
            return FALSE;
        }
    }
    
    if (x) *x = win_x;
    if (y) *y = win_y;
    if (width) *width = attrs.width;
    if (height) *height = attrs.height;
    
    return TRUE;
}

// Get window state (maximized, etc) and position relative to monitor
gboolean get_window_state_and_position(Display *display, Window window, 
                                      gboolean *is_maximized_vert, gboolean *is_maximized_horz,
                                      double *relative_x, double *relative_y,
                                      int monitor_x, int monitor_width, 
                                      int monitor_y, int monitor_height,
                                      int win_x, int win_y) {
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom net_wm_state_maximized_vert = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    Atom net_wm_state_maximized_horz = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom actual_type;
    int actual_format;
    unsigned long n_items, bytes_after;
    unsigned char *data = NULL;
    
    *is_maximized_vert = FALSE;
    *is_maximized_horz = FALSE;
    
    if (XGetWindowProperty(display, window, net_wm_state,
                          0, 1024, False, XA_ATOM,
                          &actual_type, &actual_format, &n_items, &bytes_after,
                          &data) == Success && data != NULL) {
        if (actual_format == 32) {
            Atom *atoms = (Atom *)data;
            for (unsigned long i = 0; i < n_items; i++) {
                if (atoms[i] == net_wm_state_maximized_vert) {
                    *is_maximized_vert = TRUE;
                }
                if (atoms[i] == net_wm_state_maximized_horz) {
                    *is_maximized_horz = TRUE;
                }
            }
        }
        XFree(data);
    }
    
    // Calculate relative position within monitor
    // This helps preserve tiling position (left vs right, top vs bottom)
    *relative_x = (double)(win_x - monitor_x) / monitor_width;
    *relative_y = (double)(win_y - monitor_y) / monitor_height;
    
    return TRUE;
}

// Move window to specific position
void move_window_to_position(Display *display, Window window, int x, int y,
                           gboolean restore_maximized_vert, gboolean restore_maximized_horz) {
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom net_wm_state_maximized_vert = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    Atom net_wm_state_maximized_horz = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    XEvent event;
    
    // First, remove maximized state if present
    memset(&event, 0, sizeof(event));
    event.type = ClientMessage;
    event.xclient.type = ClientMessage;
    event.xclient.send_event = True;
    event.xclient.display = display;
    event.xclient.window = window;
    event.xclient.message_type = net_wm_state;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 0; // _NET_WM_STATE_REMOVE
    event.xclient.data.l[1] = net_wm_state_maximized_vert;
    event.xclient.data.l[2] = net_wm_state_maximized_horz;
    event.xclient.data.l[3] = 1; // Source indication
    
    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display);
    
    // Small delay to let the window manager process the unmaximize
    usleep(50000); // 50ms
    
    // Move the window
    XMoveWindow(display, window, x, y);
    XFlush(display);
    
    // Restore maximized state if needed
    if (restore_maximized_vert || restore_maximized_horz) {
        usleep(50000); // Give WM time to process the move
        
        memset(&event, 0, sizeof(event));
        event.type = ClientMessage;
        event.xclient.type = ClientMessage;
        event.xclient.send_event = True;
        event.xclient.display = display;
        event.xclient.window = window;
        event.xclient.message_type = net_wm_state;
        event.xclient.format = 32;
        event.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
        event.xclient.data.l[1] = restore_maximized_vert ? net_wm_state_maximized_vert : 0;
        event.xclient.data.l[2] = restore_maximized_horz ? net_wm_state_maximized_horz : 0;
        event.xclient.data.l[3] = 1; // Source indication
        
        XSendEvent(display, DefaultRootWindow(display), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &event);
        XFlush(display);
    }
    
    log_debug("Moved window 0x%lx to position (%d, %d), maximized state: vert=%d, horz=%d", 
              window, x, y, restore_maximized_vert, restore_maximized_horz);
}

// Find which monitor a window center is on using XRandR
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

// Move window to next monitor using XRandR
void move_window_to_next_monitor_with_screen(Display *display, Window window, GdkScreen *screen) {
    int win_x, win_y, win_width, win_height;
    MonitorInfo *monitors;
    int monitor_count;
    gboolean is_maximized_vert, is_maximized_horz;
    
    // Variables for state preservation
    double relative_x = 0.0, relative_y = 0.0;
    
    // Get current window geometry
    if (!get_window_geometry(display, window, &win_x, &win_y, &win_width, &win_height)) {
        log_error("Failed to get geometry for window 0x%lx", window);
        return;
    }
    
    log_debug("Current window position: (%d, %d), size: %dx%d, maximized: vert=%d, horz=%d", 
              win_x, win_y, win_width, win_height, is_maximized_vert, is_maximized_horz);
    
    // Get monitor information via XRandR
    monitor_count = get_monitors_xrandr(display, &monitors);
    if (monitor_count <= 1 || !monitors) {
        log_info("Only %d monitor(s) detected, cannot move window", monitor_count);
        if (monitors) free(monitors);
        return;
    }
    
    // Find which monitor the window is currently on
    int current_monitor = get_window_monitor_xrandr(display, win_x, win_y, win_width, win_height);
    
    // Fallback: if not found, assume first monitor
    if (current_monitor == -1) {
        current_monitor = 0;
        log_debug("Window not clearly on any monitor, using monitor 0");
    }
    
    // Calculate next monitor (wrap around)
    int next_monitor = (current_monitor + 1) % monitor_count;
    
    log_debug("Moving from monitor %d to monitor %d", current_monitor, next_monitor);
    
    // Get monitor geometries
    MonitorInfo current_geometry = monitors[current_monitor];
    MonitorInfo next_geometry = monitors[next_monitor];
    
    // Get current window state and position info
    get_window_state_and_position(display, window, &is_maximized_vert, &is_maximized_horz,
                                 &relative_x, &relative_y,
                                 current_geometry.x, current_geometry.width,
                                 current_geometry.y, current_geometry.height,
                                 win_x, win_y);
    
    int new_x, new_y;
    
    if (is_maximized_vert || is_maximized_horz) {
        // For maximized/tiled windows, preserve the relative position
        // This ensures right-tiled windows stay on right, etc.
        new_x = next_geometry.x + (int)(relative_x * next_geometry.width);
        new_y = next_geometry.y + (int)(relative_y * next_geometry.height);
        
        // For tiled windows, ensure they're flush with the monitor edge
        if (relative_x < 0.01) {
            new_x = next_geometry.x;  // Left edge
        } else if (relative_x > 0.99) {
            new_x = next_geometry.x + next_geometry.width - win_width;  // Right edge
        }
        
        log_debug("Tiled window: placing at relative position %.2f, %.2f", relative_x, relative_y);
    } else {
        // For normal windows, maintain relative position
        double rel_x = (double)(win_x - current_geometry.x) / current_geometry.width;
        double rel_y = (double)(win_y - current_geometry.y) / current_geometry.height;
        
        // Clamp to reasonable bounds
        rel_x = MAX(0.0, MIN(1.0, rel_x));
        rel_y = MAX(0.0, MIN(1.0, rel_y));
        
        log_debug("Relative position: %.2f, %.2f", rel_x, rel_y);
        
        // Calculate new position on next monitor
        new_x = next_geometry.x + (int)(rel_x * next_geometry.width);
        new_y = next_geometry.y + (int)(rel_y * next_geometry.height);
        
        // Ensure window stays within monitor bounds
        if (new_x + win_width > next_geometry.x + next_geometry.width) {
            new_x = next_geometry.x + next_geometry.width - win_width;
        }
        if (new_y + win_height > next_geometry.y + next_geometry.height) {
            new_y = next_geometry.y + next_geometry.height - win_height;
        }
        
        // Ensure window is not off screen
        new_x = MAX(new_x, next_geometry.x);
        new_y = MAX(new_y, next_geometry.y);
    }
    
    log_debug("Moving window to: (%d, %d)", new_x, new_y);
    
    // Move the window and restore its state
    move_window_to_position(display, window, new_x, new_y, is_maximized_vert, is_maximized_horz);
    
    log_info("Moved window 0x%lx from monitor %d to monitor %d (position: %d,%d -> %d,%d)",
             window, current_monitor, next_monitor, win_x, win_y, new_x, new_y);
    
    // Clean up
    free(monitors);
}

// Move window to next monitor (compatibility function)
void move_window_to_next_monitor_by_id(Display *display, Window window) {
    move_window_to_next_monitor_with_screen(display, window, NULL);
}

// Move the selected window to the next monitor
void move_window_to_next_monitor(AppData *app) {
    if (!app || app->filtered_count == 0) {
        log_warn("No window selected to move");
        return;
    }
    
    // Get the selected window using centralized selection management
    WindowInfo *selected_window = get_selected_window(app);
    if (!selected_window) {
        log_error("No window selected for monitor move");
        return;
    }

    log_debug("Moving window '%s' (ID: 0x%lx) to next monitor",
              selected_window->title, selected_window->id);
    
    // Get the GDK screen from the app window
    GdkScreen *screen = NULL;
    if (app->window) {
        screen = gtk_window_get_screen(GTK_WINDOW(app->window));
    }
    
    // Store the window ID to activate after moving
    Window window_to_activate = selected_window->id;
    
    // Move the window
    move_window_to_next_monitor_with_screen(app->display, window_to_activate, screen);
    
    // Activate the moved window and close cofi
    log_info("Activating moved window and closing cofi");
    activate_window(window_to_activate);
    gtk_main_quit();
}