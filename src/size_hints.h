#ifndef SIZE_HINTS_H
#define SIZE_HINTS_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>

// Structure to hold window size hints
typedef struct {
    int min_width, min_height;
    int max_width, max_height;
    int base_width, base_height;
    int width_inc, height_inc;
    int min_aspect_x, min_aspect_y;
    int max_aspect_x, max_aspect_y;
    int flags;
} WindowSizeHints;

// Get size hints for a window
// Returns 1 on success, 0 on failure
int get_window_size_hints(Display *display, Window window, WindowSizeHints *hints);

// Ensure a rectangle satisfies size hints
// Modifies rect in place to satisfy constraints
void ensure_size_hints_satisfied(int *x, int *y, int *width, int *height, 
                                WindowSizeHints *hints);

#endif // SIZE_HINTS_H