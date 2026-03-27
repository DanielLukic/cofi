#ifndef WINDOW_HIGHLIGHT_H
#define WINDOW_HIGHLIGHT_H

#include <X11/Xlib.h>
#include <glib.h>

typedef struct {
    // Ripple state
    Window ripple_bars[4];  // top, bottom, left, right
    int ripple_step;
    guint ripple_timer;
    int target_x, target_y, target_w, target_h;

    int active;
} WindowHighlight;

typedef struct AppData AppData;

void highlight_window(AppData *app, Window target);
void destroy_highlight(AppData *app);
void init_window_highlight(WindowHighlight *highlight);

#endif // WINDOW_HIGHLIGHT_H
