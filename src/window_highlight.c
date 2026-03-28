#include "window_highlight.h"
#include "app_data.h"
#include "monitor_move.h"
#include "x11_utils.h"
#include "log.h"
#include <gdk/gdkx.h>
#include <math.h>
#include <string.h>

// Circle ripple: expanding ring drawn with Cairo on a single ARGB overlay
#define RIPPLE_DURATION_MS 200
#define RIPPLE_STROKE_WIDTH 16.0
#define RIPPLE_START_FRAC 10       // start radius = 10% of screen height
#define RIPPLE_END_FRAC 30         // end radius = 30% of screen height
#define RIPPLE_COLOR_R 0.94        // #f0 / 255
#define RIPPLE_COLOR_G 0.94        // #f0 / 255
#define RIPPLE_COLOR_B 1.0         // #ff / 255

static gboolean on_ripple_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    (void)widget;
    AppData *app = (AppData *)data;
    WindowHighlight *hl = &app->highlight;

    // Clear to fully transparent
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);

    if (!hl->active || hl->start_time == 0) return FALSE;

    // Compute animation progress
    GdkFrameClock *clock = gtk_widget_get_frame_clock(hl->ripple_window);
    if (!clock) return FALSE;

    gint64 now = gdk_frame_clock_get_frame_time(clock);
    double elapsed_ms = (now - hl->start_time) / 1000.0;
    if (elapsed_ms < 0) elapsed_ms = 0;
    if (elapsed_ms >= RIPPLE_DURATION_MS) return FALSE;

    double t = elapsed_ms / RIPPLE_DURATION_MS;

    // Ease-out: starts fast, decelerates
    double eased = 1.0 - (1.0 - t) * (1.0 - t);

    // Compute radius in GDK/Cairo logical coordinates
    int screen_h = gdk_screen_get_height(gdk_screen_get_default());
    double r_start = screen_h * RIPPLE_START_FRAC / 200.0;
    double r_end = screen_h * RIPPLE_END_FRAC / 200.0;
    double radius = r_start + (r_end - r_start) * eased;

    // Fade: full brightness for 80%, then fade out
    double fade = (t < 0.8) ? 1.0 : 1.0 - (t - 0.8) / 0.2;

    // Draw ring at center of the overlay (GDK allocation = Cairo coordinate space)
    GtkAllocation alloc;
    gtk_widget_get_allocation(hl->ripple_window, &alloc);
    double cx = alloc.width / 2.0;
    double cy = alloc.height / 2.0;

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr, RIPPLE_COLOR_R, RIPPLE_COLOR_G, RIPPLE_COLOR_B, fade);
    cairo_set_line_width(cr, RIPPLE_STROKE_WIDTH);
    cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
    cairo_stroke(cr);

    return FALSE;
}

static void on_frame_update(GdkFrameClock *clock, gpointer data) {
    AppData *app = (AppData *)data;
    WindowHighlight *hl = &app->highlight;

    if (!hl->active || !hl->ripple_window) return;

    gint64 now = gdk_frame_clock_get_frame_time(clock);
    double elapsed_ms = (now - hl->start_time) / 1000.0;

    if (elapsed_ms >= RIPPLE_DURATION_MS) {
        destroy_highlight(app);
        return;
    }

    gtk_widget_queue_draw(hl->ripple_window);
}

static void ensure_ripple_window(AppData *app) {
    WindowHighlight *hl = &app->highlight;
    if (hl->ripple_window) return;

    GtkWidget *win = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(win), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(win), FALSE);

    // Enable ARGB visual for transparency (requires compositor)
    GdkScreen *screen = gdk_screen_get_default();
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (!visual) {
        log_warn("No ARGB visual available, ripple effect disabled");
        gtk_widget_destroy(win);
        return;
    }
    gtk_widget_set_visual(win, visual);
    gtk_widget_set_app_paintable(win, TRUE);

    // Connect draw handler
    hl->draw_handler_id = g_signal_connect(win, "draw", G_CALLBACK(on_ripple_draw), app);

    // Realize but don't show yet
    gtk_widget_realize(win);

    hl->ripple_window = win;
    log_debug("Ripple overlay window created");
}

static void stop_animation(WindowHighlight *hl) {
    if (hl->update_handler_id > 0 && hl->ripple_window) {
        GdkFrameClock *clock = gtk_widget_get_frame_clock(hl->ripple_window);
        if (clock) {
            g_signal_handler_disconnect(clock, hl->update_handler_id);
            gdk_frame_clock_end_updating(clock);
        } else {
            log_warn("Frame clock gone during stop_animation");
        }
        hl->update_handler_id = 0;
    }
    hl->start_time = 0;
}

void init_window_highlight(WindowHighlight *highlight) {
    memset(highlight, 0, sizeof(WindowHighlight));
}

void destroy_highlight(AppData *app) {
    WindowHighlight *hl = &app->highlight;

    stop_animation(hl);

    if (hl->ripple_window && gtk_widget_get_visible(hl->ripple_window)) {
        gtk_widget_hide(hl->ripple_window);
    }

    hl->active = 0;
}

void cleanup_window_highlight(AppData *app) {
    destroy_highlight(app);
    WindowHighlight *hl = &app->highlight;
    if (hl->ripple_window) {
        gtk_widget_destroy(hl->ripple_window);
        hl->ripple_window = NULL;
    }
}

void highlight_window(AppData *app, Window target) {
    if (!app->config.ripple_enabled) return;
    destroy_highlight(app);

    WindowHighlight *hl = &app->highlight;

    int win_x, win_y, win_w, win_h;
    if (!get_window_geometry(app->display, target, &win_x, &win_y, &win_w, &win_h)) {
        return;
    }

    ensure_ripple_window(app);

    // Compute overlay bounds in X11 pixel coordinates
    int screen_h = DisplayHeight(app->display, DefaultScreen(app->display));
    int max_diameter = screen_h * RIPPLE_END_FRAC / 100 + (int)RIPPLE_STROKE_WIDTH + 2;

    int cx = win_x + win_w / 2;
    int cy = win_y + win_h / 2;
    hl->target_cx = cx;
    hl->target_cy = cy;

    // Position via X11 directly (bypasses GDK coordinate scaling)
    int ox = cx - max_diameter / 2;
    int oy = cy - max_diameter / 2;
    GdkWindow *gdk_win = gtk_widget_get_window(hl->ripple_window);
    Window xwin = gdk_x11_window_get_xid(gdk_win);
    XMoveResizeWindow(app->display, xwin, ox, oy, max_diameter, max_diameter);

    // Show and raise
    gtk_widget_show(hl->ripple_window);
    XRaiseWindow(app->display, xwin);

    // Start frame-clock-driven animation
    GdkFrameClock *clock = gtk_widget_get_frame_clock(hl->ripple_window);
    if (clock) {
        hl->start_time = gdk_frame_clock_get_frame_time(clock);
        hl->update_handler_id = g_signal_connect(clock, "update",
                                                  G_CALLBACK(on_frame_update), app);
        gdk_frame_clock_begin_updating(clock);
    }

    hl->active = 1;
    log_info("Highlight: circle ripple on 0x%lx at (%d,%d)", target, cx, cy);
}
