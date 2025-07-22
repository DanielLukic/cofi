#ifndef FRAME_EXTENTS_H
#define FRAME_EXTENTS_H

#include <X11/Xlib.h>

typedef struct {
    int left;
    int right;
    int top;
    int bottom;
} FrameExtents;

// Get window frame extents (decorations/borders added by window manager)
// Returns TRUE if successful, FALSE otherwise
int get_frame_extents(Display *display, Window window, FrameExtents *extents);

// Adjust dimensions to account for window frame
// Subtracts frame extents from the provided width/height
void adjust_for_frame_extents(Display *display, Window window, 
                             int *width, int *height);

#endif // FRAME_EXTENTS_H