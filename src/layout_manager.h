#ifndef LAYOUT_MANAGER_H
#define LAYOUT_MANAGER_H

#include <X11/Xlib.h>
#include <glib.h>

#define MAX_LAYOUT_WORKSPACES 16

typedef struct AppData AppData;

typedef enum {
    LAYOUT_PATTERN_NONE = 0,
    LAYOUT_PATTERN_MAIN_STACK,
} LayoutPattern;

typedef struct {
    gboolean active;
    LayoutPattern pattern;
    Window primary_window_id;
} WorkspaceLayoutState;

void init_layout_states(WorkspaceLayoutState *states, int count);
gboolean apply_layout(AppData *app, LayoutPattern pattern);
gboolean refresh_layout(AppData *app);
void layout_off(AppData *app);

#endif
