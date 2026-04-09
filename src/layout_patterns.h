#ifndef LAYOUT_PATTERNS_H
#define LAYOUT_PATTERNS_H

#include <X11/Xlib.h>
#include <glib.h>

#include "tiling.h"
#include "workarea.h"

typedef struct {
    Window window_id;
    TileGeometry geometry;
} LayoutTarget;

int calculate_main_stack_targets(const Window *windows, int count, Window primary_id,
                                 const WorkArea *monitor_rect,
                                 LayoutTarget *targets, int max_targets);

gboolean layout_main_stack(Display *display, const Window *windows, int count,
                           Window primary_id, const WorkArea *monitor_rect);

#endif
