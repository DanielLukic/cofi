#ifndef X11_UTILS_H
#define X11_UTILS_H

#include <X11/Xlib.h>

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

#endif // X11_UTILS_H