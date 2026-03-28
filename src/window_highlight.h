#ifndef WINDOW_HIGHLIGHT_H
#define WINDOW_HIGHLIGHT_H

#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <glib.h>

typedef struct {
    GtkWidget *ripple_window;       // Persistent ARGB overlay window
    gulong draw_handler_id;         // "draw" signal handler
    gulong update_handler_id;       // GdkFrameClock "update" signal handler
    gint64 start_time;              // Animation start (microseconds, from frame clock)
    int target_cx, target_cy;       // Center of target window
    int active;
} WindowHighlight;

typedef struct AppData AppData;

void highlight_window(AppData *app, Window target);
void destroy_highlight(AppData *app);
void cleanup_window_highlight(AppData *app);
void init_window_highlight(WindowHighlight *highlight);

#endif // WINDOW_HIGHLIGHT_H
