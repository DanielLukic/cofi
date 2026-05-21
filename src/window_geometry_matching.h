#ifndef WINDOW_GEOMETRY_MATCHING_H
#define WINDOW_GEOMETRY_MATCHING_H

#include <X11/Xlib.h>
#include <gtk/gtk.h>

#include "app_data.h"
#include "match_entry.h"

typedef struct {
    Window window;
    int x;
    int y;
    int width;
    int height;
    int desktop;
} WindowGeometryRestoreTarget;

gboolean resolve_window_geometry_restore_target(const MatchEntryManager *manager,
                                                int match_id,
                                                WindowGeometryRestoreTarget *out);

gboolean apply_window_geometry_restore(Display *display,
                                       const WindowGeometryRestoreTarget *target);

gboolean handle_window_geometry_save(GdkEventKey *event, AppData *app);
gboolean handle_window_geometry_restore(GdkEventKey *event, AppData *app);

#endif
