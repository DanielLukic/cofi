#include "x11/monitor_move.h"
#include "core/app/app_data.h"
#include "x11/frame_extents.h"
#include "x11/window_info.h"
#include "core/log/log.h"
#include "ui/display.h"
#include "x11/window_list.h"
#include "ui/window_filter.h"
#include "core/selection/selection.h"
#include "x11/x11_utils.h"
#include "x11/xrandr_helpers.h"
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

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

    // Remove maximized state only if currently set
    if (get_window_state(display, window, "_NET_WM_STATE_MAXIMIZED_VERT") ||
        get_window_state(display, window, "_NET_WM_STATE_MAXIMIZED_HORZ")) {
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
        usleep(50000);
    }

    // Move the window; x,y are frame-space (from monitor geometry)
    int cur_width = 0, cur_height = 0;
    {
        int cx, cy;
        get_window_geometry(display, window, &cx, &cy, &cur_width, &cur_height);
    }
    xmove_resize_frame_aware(display, window, x, y, cur_width, cur_height);
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

static gboolean move_window_to_monitor_with_screen(Display *display, Window window,
                                                   GdkScreen *screen __attribute__((unused)),
                                                   int target_monitor) {
    int win_x, win_y, win_width, win_height;
    MonitorInfo *monitors;
    int monitor_count;
    gboolean is_maximized_vert, is_maximized_horz;

    // Variables for state preservation
    double relative_x = 0.0, relative_y = 0.0;

    // Get current window geometry
    if (!get_window_geometry(display, window, &win_x, &win_y, &win_width, &win_height)) {
        log_error("Failed to get geometry for window 0x%lx", window);
        return FALSE;
    }

    // Get monitor information via XRandR
    monitor_count = get_monitors_xrandr(display, &monitors);
    if (monitor_count <= 0 || !monitors) {
        log_info("No monitors detected, cannot move window");
        if (monitors) free(monitors);
        return FALSE;
    }

    if (target_monitor < 0 || target_monitor >= monitor_count) {
        log_warn("Monitor index %d out of range (monitor count: %d)", target_monitor, monitor_count);
        free(monitors);
        return FALSE;
    }

    // Find which monitor the window is currently on
    int current_monitor = get_window_monitor_xrandr(display, win_x, win_y, win_width, win_height);

    // Fallback: if not found, assume first monitor
    if (current_monitor == -1) {
        current_monitor = 0;
        log_debug("Window not clearly on any monitor, using monitor 0");
    }

    log_debug("Moving from monitor %d to monitor %d", current_monitor, target_monitor);

    if (current_monitor == target_monitor) {
        log_debug("Window already on monitor %d; skipping move", target_monitor);
        free(monitors);
        return TRUE;
    }

    // Get monitor geometries
    MonitorInfo current_geometry = monitors[current_monitor];
    MonitorInfo next_geometry = monitors[target_monitor];

    // Get current window state and position info
    get_window_state_and_position(display, window, &is_maximized_vert, &is_maximized_horz,
                                 &relative_x, &relative_y,
                                 current_geometry.x, current_geometry.width,
                                 current_geometry.y, current_geometry.height,
                                 win_x, win_y);

    int new_x, new_y;

    if (is_maximized_vert || is_maximized_horz) {
        // For maximized/tiled windows, preserve the relative position.
        // This ensures right-tiled windows stay on right, etc.
        new_x = next_geometry.x + (int)(relative_x * next_geometry.width);
        new_y = next_geometry.y + (int)(relative_y * next_geometry.height);

        // For tiled windows, ensure they're flush with the monitor edge.
        if (relative_x < 0.01) {
            new_x = next_geometry.x;
        } else if (relative_x > 0.99) {
            new_x = next_geometry.x + next_geometry.width - win_width;
        }

        log_debug("Tiled window: placing at relative position %.2f, %.2f", relative_x, relative_y);
    } else {
        // For normal windows, maintain relative position.
        double rel_x = (double)(win_x - current_geometry.x) / current_geometry.width;
        double rel_y = (double)(win_y - current_geometry.y) / current_geometry.height;

        // Clamp to reasonable bounds.
        rel_x = MAX(0.0, MIN(1.0, rel_x));
        rel_y = MAX(0.0, MIN(1.0, rel_y));

        log_debug("Relative position: %.2f, %.2f", rel_x, rel_y);

        // Calculate new position on next monitor.
        new_x = next_geometry.x + (int)(rel_x * next_geometry.width);
        new_y = next_geometry.y + (int)(rel_y * next_geometry.height);

        // Ensure window stays within monitor bounds.
        if (new_x + win_width > next_geometry.x + next_geometry.width) {
            new_x = next_geometry.x + next_geometry.width - win_width;
        }
        if (new_y + win_height > next_geometry.y + next_geometry.height) {
            new_y = next_geometry.y + next_geometry.height - win_height;
        }

        // Ensure window is not off screen.
        new_x = MAX(new_x, next_geometry.x);
        new_y = MAX(new_y, next_geometry.y);
    }

    log_debug("Moving window to: (%d, %d)", new_x, new_y);

    // Move the window and restore its state.
    move_window_to_position(display, window, new_x, new_y,
                            is_maximized_vert, is_maximized_horz);

    log_info("Moved window 0x%lx from monitor %d to monitor %d (position: %d,%d -> %d,%d)",
             window, current_monitor, target_monitor, win_x, win_y, new_x, new_y);

    free(monitors);
    return TRUE;
}

// Move window to next monitor using XRandR
void move_window_to_next_monitor_with_screen(Display *display, Window window, GdkScreen *screen) {
    int win_x, win_y, win_width, win_height;
    MonitorInfo *monitors = NULL;
    int monitor_count;

    if (!get_window_geometry(display, window, &win_x, &win_y, &win_width, &win_height)) {
        log_error("Failed to get geometry for window 0x%lx", window);
        return;
    }

    monitor_count = get_monitors_xrandr(display, &monitors);
    if (monitor_count <= 1 || !monitors) {
        log_info("Only %d monitor(s) detected, cannot move window", monitor_count);
        if (monitors) free(monitors);
        return;
    }

    int current_monitor = get_window_monitor_xrandr(display, win_x, win_y, win_width, win_height);
    if (current_monitor == -1) {
        current_monitor = 0;
        log_debug("Window not clearly on any monitor, using monitor 0");
    }

    int next_monitor = (current_monitor + 1) % monitor_count;
    free(monitors);

    move_window_to_monitor_with_screen(display, window, screen, next_monitor);
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

    log_info("Moved window to next monitor");
}

gboolean move_window_to_monitor_index(AppData *app, WindowInfo *window, int index) {
    if (!app || !window) {
        log_warn("No window selected for monitor move");
        return FALSE;
    }

    if (index < 0) {
        log_warn("Monitor index %d out of range", index);
        return FALSE;
    }

    GdkScreen *screen = NULL;
    if (app->window) {
        screen = gtk_window_get_screen(GTK_WINDOW(app->window));
    }

    return move_window_to_monitor_with_screen(app->display, window->id, screen, index);
}
