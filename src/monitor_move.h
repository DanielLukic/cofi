#ifndef MONITOR_MOVE_H
#define MONITOR_MOVE_H

#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include "window_info.h"
#include "app_data.h"

// Move the selected window to the next monitor
void move_window_to_next_monitor(AppData *app);

// Move a specific window to the next monitor
void move_window_to_next_monitor_by_id(Display *display, Window window);

// Move window to next monitor with provided GdkScreen
void move_window_to_next_monitor_with_screen(Display *display, Window window, GdkScreen *screen);

// Get window geometry
gboolean get_window_geometry(Display *display, Window window, 
                           int *x, int *y, int *width, int *height);

// Move window to specific position
void move_window_to_position(Display *display, Window window, int x, int y,
                           gboolean restore_maximized_vert, gboolean restore_maximized_horz);

#endif // MONITOR_MOVE_H