#ifndef WINDOW_GEOMETRY_MATCHING_H
#define WINDOW_GEOMETRY_MATCHING_H

#include <X11/Xlib.h>
#include <gtk/gtk.h>

#include "app_data.h"
#include "layout_store.h"
#include "match_entry.h"

typedef struct {
    Window window;
    int x;
    int y;
    int width;
    int height;
    int desktop;
    gboolean maximized_vert;
    gboolean maximized_horz;
    gboolean fullscreen;
} WindowGeometryRestoreTarget;

gboolean resolve_window_geometry_restore_target(const MatchEntryManager *manager,
                                                const LayoutStore *store,
                                                int match_id,
                                                WindowGeometryRestoreTarget *out);

// move_deferred_out: if non-NULL, set to TRUE when do_move was skipped because
// _NET_FRAME_EXTENTS was missing or all-zero (WM not yet ready).
gboolean apply_window_geometry_restore(Display *display,
                                       const WindowGeometryRestoreTarget *target,
                                       gboolean *move_deferred_out);

gboolean save_window_geometry_for_window(AppData *app, const WindowInfo *window);
gboolean restore_window_geometry_for_window(AppData *app, const WindowInfo *window);
gboolean clear_window_geometry_for_window(AppData *app, const WindowInfo *window);

// Called by x11_events when _NET_FRAME_EXTENTS changes for a window.
// Dequeues the window and re-runs restore now that extents are populated.
void process_pending_geometry_restore(AppData *app, Window window_id);

#endif
