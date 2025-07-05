#ifndef X11_UTILS_H
#define X11_UTILS_H

#include <X11/Xlib.h>
#include <glib.h>
#include "constants.h"

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

// Get currently active window ID from _NET_ACTIVE_WINDOW
int get_active_window_id(Display *display);

// Get number of workspaces/desktops from _NET_NUMBER_OF_DESKTOPS
int get_number_of_desktops(Display *display);

// Get desktop names from _NET_DESKTOP_NAMES
char** get_desktop_names(Display *display, int *count);

// Get current desktop from _NET_CURRENT_DESKTOP
int get_current_desktop(Display *display);

// Switch to a specific desktop using _NET_CURRENT_DESKTOP
void switch_to_desktop(Display *display, int desktop);

// Move window to specific desktop (0-based index)
void move_window_to_desktop(Display *display, Window window, int desktop_index);

// Window state manipulation functions
void toggle_window_state(Display *display, Window window, const char *state_atom_name);
gboolean get_window_state(Display *display, Window window, const char *state_atom_name);

// Window management functions
void close_window(Display *display, Window window);
void toggle_maximize_window(Display *display, Window window);
void toggle_maximize_horizontal(Display *display, Window window);
void toggle_maximize_vertical(Display *display, Window window);

#endif // X11_UTILS_H