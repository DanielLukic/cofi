#ifndef XRANDR_HELPERS_H
#define XRANDR_HELPERS_H

#include <X11/Xlib.h>

typedef struct {
    int x, y;
    int width, height;
} MonitorInfo;

int get_monitors_xrandr(Display *display, MonitorInfo **monitors);
int get_window_monitor_xrandr(Display *display, int win_x, int win_y,
                              int win_width, int win_height);

#endif
