#ifndef WINDOW_GEOMETRY_MATCHING_H
#define WINDOW_GEOMETRY_MATCHING_H

#include <X11/Xlib.h>
#include <gtk/gtk.h>

#include "core/app/app_data.h"
#include "geom/layout_store.h"
#include "matching/match_entry.h"

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
    gboolean restore_desktop;
    gboolean disabled;
} WindowGeometryRestoreTarget;

gboolean resolve_window_geometry_restore_target(const MatchEntryManager *manager,
                                                const LayoutStore *store,
                                                int match_id,
                                                WindowGeometryRestoreTarget *out);

gboolean apply_window_geometry_restore(Display *display,
                                       const WindowGeometryRestoreTarget *target);

gboolean save_window_geometry_for_window(AppData *app, const WindowInfo *window);
gboolean restore_window_geometry_for_window(AppData *app, const WindowInfo *window);
gboolean clear_window_geometry_for_window(AppData *app, const WindowInfo *window);

#endif
