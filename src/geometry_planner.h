#ifndef GEOMETRY_PLANNER_H
#define GEOMETRY_PLANNER_H

#include <stdbool.h>

// Plain-C snapshot of a window's geometry + WM state.  No X11 types.
typedef struct {
    int x, y, width, height;
    int desktop;           // -1 = unknown/unset
    bool maximized_vert;
    bool maximized_horz;
    bool fullscreen;
} GeometryState;

// Which operations apply_window_geometry_restore must emit.
// All fields false → nothing to do, XFlush must be skipped.
typedef struct {
    bool unset_fullscreen;
    bool unset_max_vert;
    bool unset_max_horz;
    bool do_move;          // XMoveResizeWindow
    bool do_desktop;       // move_window_to_desktop
    bool set_fullscreen;
    bool set_max_vert;
    bool set_max_horz;
    bool any;              // OR of the above — gate XFlush on this
} GeometryRestorePlan;

// Pure decision function: compute the delta from current to target.
// Contains no X11 calls; safe to unit-test without a display.
GeometryRestorePlan geometry_restore_plan(const GeometryState *current,
                                          const GeometryState *target);

#endif // GEOMETRY_PLANNER_H
