#include "size_hints.h"
#include "log.h"
#include <string.h>
#include <limits.h>

// Get size hints for a window
int get_window_size_hints(Display *display, Window window, WindowSizeHints *hints) {
    XSizeHints *size_hints;
    long supplied_return;
    
    // Initialize with defaults
    memset(hints, 0, sizeof(WindowSizeHints));
    hints->min_width = 1;
    hints->min_height = 1;
    hints->max_width = INT_MAX;
    hints->max_height = INT_MAX;
    hints->width_inc = 1;
    hints->height_inc = 1;
    
    size_hints = XAllocSizeHints();
    if (!size_hints) {
        return 0;
    }
    
    if (XGetWMNormalHints(display, window, size_hints, &supplied_return)) {
        hints->flags = size_hints->flags;
        
        if (size_hints->flags & PMinSize) {
            hints->min_width = size_hints->min_width;
            hints->min_height = size_hints->min_height;
        }
        
        if (size_hints->flags & PMaxSize) {
            hints->max_width = size_hints->max_width;
            hints->max_height = size_hints->max_height;
        }
        
        if (size_hints->flags & PBaseSize) {
            hints->base_width = size_hints->base_width;
            hints->base_height = size_hints->base_height;
        }
        
        if (size_hints->flags & PResizeInc) {
            hints->width_inc = size_hints->width_inc;
            hints->height_inc = size_hints->height_inc;
        }
        
        if (size_hints->flags & PAspect) {
            hints->min_aspect_x = size_hints->min_aspect.x;
            hints->min_aspect_y = size_hints->min_aspect.y;
            hints->max_aspect_x = size_hints->max_aspect.x;
            hints->max_aspect_y = size_hints->max_aspect.y;
        }
        
        XFree(size_hints);
        return 1;
    }
    
    XFree(size_hints);
    return 0;
}

// Ensure a rectangle satisfies size hints
void ensure_size_hints_satisfied(int *x, int *y, int *width, int *height, 
                                WindowSizeHints *hints) {
    (void)x; // x position doesn't affect size hints
    (void)y; // y position doesn't affect size hints
    
    // Enforce minimum size
    if (*width < hints->min_width) {
        log_debug("Width %d below minimum %d, adjusting", *width, hints->min_width);
        *width = hints->min_width;
    }
    if (*height < hints->min_height) {
        log_debug("Height %d below minimum %d, adjusting", *height, hints->min_height);
        *height = hints->min_height;
    }
    
    // Enforce maximum size
    if (*width > hints->max_width) {
        log_debug("Width %d above maximum %d, adjusting", *width, hints->max_width);
        *width = hints->max_width;
    }
    if (*height > hints->max_height) {
        log_debug("Height %d above maximum %d, adjusting", *height, hints->max_height);
        *height = hints->max_height;
    }
    
    // Handle increment constraints (for terminal windows)
    if (hints->width_inc > 1) {
        int base = (hints->flags & PBaseSize) ? hints->base_width : hints->min_width;
        int extra = *width - base;
        int units = extra / hints->width_inc;
        *width = base + units * hints->width_inc;
    }
    
    if (hints->height_inc > 1) {
        int base = (hints->flags & PBaseSize) ? hints->base_height : hints->min_height;
        int extra = *height - base;
        int units = extra / hints->height_inc;
        *height = base + units * hints->height_inc;
    }
    
    // TODO: Handle aspect ratio constraints if needed
}