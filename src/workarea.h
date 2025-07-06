#ifndef WORKAREA_H
#define WORKAREA_H

#include <X11/Xlib.h>

// Structure to hold work area dimensions
typedef struct {
    int x, y;
    int width, height;
} WorkArea;

// Get the work area for a specific monitor (excluding panels/docks)
// Returns 1 on success, 0 on failure
int get_work_area_for_monitor(Display *display, int monitor_index, WorkArea *work_area);

// Get the current desktop's work area
// Returns 1 on success, 0 on failure  
int get_current_work_area(Display *display, WorkArea *work_area);

#endif // WORKAREA_H