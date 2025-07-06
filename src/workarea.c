#include "workarea.h"
#include "log.h"
#include <X11/Xatom.h>
#include <stdlib.h>
#include <string.h>

// Get the current desktop number
static int get_current_desktop(Display *display) {
    Atom net_current_desktop = XInternAtom(display, "_NET_CURRENT_DESKTOP", False);
    Atom actual_type;
    int actual_format;
    unsigned long n_items, bytes_after;
    unsigned char *prop = NULL;
    
    int result = XGetWindowProperty(display, DefaultRootWindow(display),
                                   net_current_desktop, 0, 1, False,
                                   XA_CARDINAL, &actual_type, &actual_format,
                                   &n_items, &bytes_after, &prop);
    
    if (result == Success && prop && n_items > 0) {
        int desktop = *((long*)prop);
        XFree(prop);
        return desktop;
    }
    
    if (prop) XFree(prop);
    return 0; // Default to first desktop
}

// Get the work area for current desktop
int get_current_work_area(Display *display, WorkArea *work_area) {
    Atom net_workarea = XInternAtom(display, "_NET_WORKAREA", False);
    Atom actual_type;
    int actual_format;
    unsigned long n_items, bytes_after;
    unsigned char *prop = NULL;
    
    // Default to full screen if _NET_WORKAREA is not available
    Screen *screen = DefaultScreenOfDisplay(display);
    work_area->x = 0;
    work_area->y = 0;
    work_area->width = WidthOfScreen(screen);
    work_area->height = HeightOfScreen(screen);
    
    int result = XGetWindowProperty(display, DefaultRootWindow(display),
                                   net_workarea, 0, (~0L), False,
                                   XA_CARDINAL, &actual_type, &actual_format,
                                   &n_items, &bytes_after, &prop);
    
    if (result == Success && prop && n_items >= 4) {
        // _NET_WORKAREA returns x, y, width, height for each desktop
        int current_desktop = get_current_desktop(display);
        long *workarea = (long*)prop;
        
        // Each desktop has 4 values (x, y, width, height)
        int offset = current_desktop * 4;
        if (offset + 3 < n_items) {
            work_area->x = workarea[offset];
            work_area->y = workarea[offset + 1];
            work_area->width = workarea[offset + 2];
            work_area->height = workarea[offset + 3];
            
            log_debug("Got work area for desktop %d: %dx%d+%d+%d", 
                     current_desktop, work_area->width, work_area->height,
                     work_area->x, work_area->y);
        }
        XFree(prop);
        return 1;
    }
    
    if (prop) XFree(prop);
    
    log_debug("_NET_WORKAREA not available, using full screen dimensions");
    return 0; // Using defaults
}

// Get work area for a specific monitor
int get_work_area_for_monitor(Display *display, int monitor_index, WorkArea *work_area) {
    // For now, just get the current work area
    // In a full implementation, this would intersect with monitor bounds
    return get_current_work_area(display, work_area);
}