#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>
#include "log.h"

typedef struct {
    int left;
    int right;
    int top;
    int bottom;
} FrameExtents;

// Get window frame extents (decorations/borders added by window manager)
// Returns TRUE if successful, FALSE otherwise
int get_frame_extents(Display *display, Window window, FrameExtents *extents) {
    if (!display || !window || !extents) {
        return 0;
    }
    
    // Initialize to zero
    memset(extents, 0, sizeof(FrameExtents));
    
    Atom net_frame_extents = XInternAtom(display, "_NET_FRAME_EXTENTS", False);
    Atom actual_type;
    int actual_format;
    unsigned long n_items, bytes_after;
    unsigned char *data = NULL;
    
    if (XGetWindowProperty(display, window, net_frame_extents,
                          0, 4, False, XA_CARDINAL,
                          &actual_type, &actual_format, &n_items, &bytes_after,
                          &data) == Success && data != NULL) {
        if (actual_format == 32 && n_items == 4) {
            long *values = (long *)data;
            extents->left = values[0];
            extents->right = values[1];
            extents->top = values[2];
            extents->bottom = values[3];
            
            log_debug("Frame extents for window 0x%lx: left=%d, right=%d, top=%d, bottom=%d",
                     window, extents->left, extents->right, extents->top, extents->bottom);
            
            XFree(data);
            return 1;
        }
        if (data) XFree(data);
    }
    
    log_debug("No frame extents found for window 0x%lx", window);
    return 0;
}

// Adjust dimensions to account for window frame
void adjust_for_frame_extents(Display *display, Window window, 
                             int *width, int *height) {
    FrameExtents extents;
    if (get_frame_extents(display, window, &extents)) {
        if (width) {
            *width -= (extents.left + extents.right);
            if (*width < 1) *width = 1; // Minimum width
        }
        if (height) {
            *height -= (extents.top + extents.bottom);
            if (*height < 1) *height = 1; // Minimum height
        }
        log_debug("Adjusted dimensions for frame: width reduced by %d, height reduced by %d",
                 extents.left + extents.right, extents.top + extents.bottom);
    }
}