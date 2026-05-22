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

// Convert a frame-space position (as returned by get_window_geometry) to the
// client-space position expected by XMoveResizeWindow.
//
// get_window_geometry translates through the WM frame parent, so it returns the
// frame's top-left in root coordinates.  XMoveResizeWindow (with a reparenting
// WM) places the *client* at the given position, so the frame lands at
// (client_x - left, client_y - top).  Without this adjustment each
// save→restore→save cycle shifts the frame by (-left, -top) — the drift bug.
//
// Pass NULL for fe to get identity (no-frame or unknown extents case).
static inline void frame_pos_to_client_pos(int frame_x, int frame_y,
                                           const FrameExtents *fe,
                                           int *client_x, int *client_y) {
    *client_x = frame_x + (fe ? fe->left : 0);
    *client_y = frame_y + (fe ? fe->top  : 0);
}

#endif // FRAME_EXTENTS_H