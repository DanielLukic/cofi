#ifndef X11_UTILS_H
#define X11_UTILS_H

#include <X11/Xlib.h>
#include <glib.h>
#include "core/utils/constants.h"
#include "x11/atom_cache.h"

// Generic X11 property getter - centralizes XGetWindowProperty pattern
CofiResult get_x11_property(Display *display, Window window, Atom property, Atom req_type,
                     unsigned long max_items, Atom *actual_type_return,
                     int *actual_format_return, unsigned long *n_items_return,
                     unsigned char **prop_return);

// Get a window property as a string
char* get_window_property(Display *display, Window window, Atom property);

// Get window type (Normal/Special) from _NET_WM_WINDOW_TYPE
char* get_window_type(Display *display, Window window);

// Get window PID from _NET_WM_PID
int get_window_pid(Display *display, Window window);

// Get window class (instance and class) from WM_CLASS
void get_window_class(Display *display, Window window, char *instance, char *class_name);

// Cached versions - use pre-interned atoms
char* get_window_type_cached(Display *display, Window window, AtomCache *atoms);
int get_window_pid_cached(Display *display, Window window, AtomCache *atoms);
void get_window_class_cached(Display *display, Window window, char *instance, char *class_name);

// Get currently active window ID from _NET_ACTIVE_WINDOW
int get_active_window_id(Display *display);

// Get number of workspaces/desktops from _NET_NUMBER_OF_DESKTOPS
int get_number_of_desktops(Display *display);

// Get desktop names from _NET_DESKTOP_NAMES
char** get_desktop_names(Display *display, int *count);

// Set desktop names via _NET_DESKTOP_NAMES
int set_desktop_names(Display *display, char **names, int num_desktops);

// Get current desktop from _NET_CURRENT_DESKTOP
int get_current_desktop(Display *display);

// Get a specific window's desktop from _NET_WM_DESKTOP
int get_window_desktop(Display *display, Window window);

// Switch to a specific desktop using _NET_CURRENT_DESKTOP
void switch_to_desktop(Display *display, int desktop);

// Move window to specific desktop (0-based index)
void move_window_to_desktop(Display *display, Window window, int desktop_index);

// Window state manipulation functions
typedef enum {
    WINDOW_STATE_UNSET = 0,
    WINDOW_STATE_SET = 1,
    WINDOW_STATE_TOGGLE = 2,
} WindowStateAction;

gboolean get_window_state(Display *display, Window window, const char *state_atom_name);
gboolean window_is_hidden(Display *display, Window window);
gboolean window_is_shaded(Display *display, Window window);
gboolean window_is_sticky(Display *display, Window window);
gboolean window_is_fullscreen(Display *display, Window window);
gboolean window_is_maximized_horizontal(Display *display, Window window);
gboolean window_is_maximized_vertical(Display *display, Window window);
void set_window_maximized(Display *display, Window window, WindowStateAction action);
void set_window_maximized_horizontal(Display *display, Window window, WindowStateAction action);
void set_window_maximized_vertical(Display *display, Window window, WindowStateAction action);
void set_window_fullscreen(Display *display, Window window, WindowStateAction action);
void set_window_above(Display *display, Window window, WindowStateAction action);
void set_window_below(Display *display, Window window, WindowStateAction action);
void set_window_skip_taskbar(Display *display, Window window, WindowStateAction action);
void set_window_sticky(Display *display, Window window, WindowStateAction action);

// Force the window title (sets both _NET_WM_NAME/UTF8 and legacy WM_NAME)
void set_window_name(Display *display, Window window, const char *name);

// Window management functions
void close_window(Display *display, Window window);
void minimize_window(Display *display, Window window);

// Send _NET_REQUEST_FRAME_EXTENTS to ask the WM to populate _NET_FRAME_EXTENTS.
// Best-effort: call early so the WM has time to set the property before first restore.
void request_frame_extents(Display *display, Window window);

// Move and resize a window given FRAME-space coordinates (as returned by get_window_geometry).
// Reads _NET_FRAME_EXTENTS and converts to client-space before calling XMoveResizeWindow.
// Falls back to raw (frame_x, frame_y) when extents are unavailable.
void xmove_resize_frame_aware(Display *display, Window window,
                               int frame_x, int frame_y, int width, int height);

#endif // X11_UTILS_H
