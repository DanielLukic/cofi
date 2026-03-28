#include "window_highlight.h"
#include "app_data.h"
#include "monitor_move.h"
#include "x11_utils.h"
#include "log.h"
#include <X11/Xatom.h>
#include <string.h>

// Ripple: expanding ring centered on window
#define RIPPLE_STEPS 10
#define RIPPLE_INTERVAL_MS 16  // ~60fps
#define RIPPLE_BAR_WIDTH 16
#define RIPPLE_START_FRAC 10   // start at 10% of screen height
#define RIPPLE_END_FRAC 30     // grow to 30% of screen height
#define RIPPLE_COLOR_R 0xf0
#define RIPPLE_COLOR_G 0xf0
#define RIPPLE_COLOR_B 0xff

static unsigned long make_color(Display *display, int r, int g, int b) {
    int screen = DefaultScreen(display);
    Colormap cmap = DefaultColormap(display, screen);
    XColor color;
    color.red = r << 8;
    color.green = g << 8;
    color.blue = b << 8;
    color.flags = DoRed | DoGreen | DoBlue;
    XAllocColor(display, cmap, &color);
    return color.pixel;
}

void init_window_highlight(WindowHighlight *highlight) {
    memset(highlight, 0, sizeof(WindowHighlight));
}

static void destroy_ripple_bars(AppData *app) {
    WindowHighlight *hl = &app->highlight;
    for (int i = 0; i < 4; i++) {
        if (hl->ripple_bars[i] != 0) {
            XDestroyWindow(app->display, hl->ripple_bars[i]);
            hl->ripple_bars[i] = 0;
        }
    }
}

void destroy_highlight(AppData *app) {
    WindowHighlight *hl = &app->highlight;

    if (hl->ripple_timer > 0) {
        g_source_remove(hl->ripple_timer);
        hl->ripple_timer = 0;
    }

    destroy_ripple_bars(app);

    if (hl->active) {
        XFlush(app->display);
    }

    hl->ripple_step = 0;
    hl->active = 0;
}

static void create_ripple_rect(AppData *app, int rx, int ry, int rw, int rh,
                               int bw, unsigned long color) {
    WindowHighlight *hl = &app->highlight;
    Window root = DefaultRootWindow(app->display);
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    attrs.background_pixel = color;

    if (rw < 1) rw = 1;
    if (rh < 1) rh = 1;
    if (bw < 1) bw = 1;

    // Top
    hl->ripple_bars[0] = XCreateWindow(app->display, root,
        rx, ry, rw, bw,
        0, CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWBackPixel, &attrs);
    // Bottom
    hl->ripple_bars[1] = XCreateWindow(app->display, root,
        rx, ry + rh - bw, rw, bw,
        0, CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWBackPixel, &attrs);
    // Left
    hl->ripple_bars[2] = XCreateWindow(app->display, root,
        rx, ry, bw, rh,
        0, CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWBackPixel, &attrs);
    // Right
    hl->ripple_bars[3] = XCreateWindow(app->display, root,
        rx + rw - bw, ry, bw, rh,
        0, CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWBackPixel, &attrs);

    // Ensure ripple bars are fully opaque (compositor might dim them)
    Atom opacity_atom = XInternAtom(app->display, "_NET_WM_WINDOW_OPACITY", False);
    unsigned long full_opacity = 0xFFFFFFFF;
    for (int i = 0; i < 4; i++) {
        XChangeProperty(app->display, hl->ripple_bars[i], opacity_atom,
                        XA_CARDINAL, 32, PropModeReplace,
                        (unsigned char *)&full_opacity, 1);
        XMapRaised(app->display, hl->ripple_bars[i]);
    }
}

static gboolean ripple_step_cb(gpointer data) {
    AppData *app = (AppData *)data;
    WindowHighlight *hl = &app->highlight;

    hl->ripple_step++;

    if (hl->ripple_step >= RIPPLE_STEPS) {
        destroy_ripple_bars(app);
        XFlush(app->display);
        hl->ripple_timer = 0;
        return FALSE;
    }

    destroy_ripple_bars(app);

    // Interpolate size from start to end fraction of screen
    float t = (float)hl->ripple_step / (RIPPLE_STEPS - 1);
    int screen_h = DisplayHeight(app->display, DefaultScreen(app->display));
    int size = (screen_h * RIPPLE_START_FRAC + (screen_h * (RIPPLE_END_FRAC - RIPPLE_START_FRAC)) * hl->ripple_step / (RIPPLE_STEPS - 1)) / 100;

    // Center on target window
    int cx = hl->target_x + hl->target_w / 2;
    int cy = hl->target_y + hl->target_h / 2;

    // Hold full brightness for 80% of animation, then fade quickly
    float fade;
    if (t < 0.8f) {
        fade = 1.0f;
    } else {
        fade = 1.0f - (t - 0.8f) / 0.2f;  // 0.8->1.0 maps to 1.0->0.0
    }
    int r = (int)(RIPPLE_COLOR_R * fade);
    int g = (int)(RIPPLE_COLOR_G * fade);
    int b = (int)(RIPPLE_COLOR_B * fade);
    unsigned long color = make_color(app->display, r, g, b);

    int rx = cx - size / 2;
    int ry = cy - size / 2;
    create_ripple_rect(app, rx, ry, size, size, RIPPLE_BAR_WIDTH, color);
    XFlush(app->display);

    return TRUE;
}

void highlight_window(AppData *app, Window target) {
    if (!app->config.ripple_enabled) return;
    destroy_highlight(app);

    WindowHighlight *hl = &app->highlight;

    // Get target window geometry for ripple
    int win_x, win_y, win_w, win_h;
    if (get_window_geometry(app->display, target, &win_x, &win_y, &win_w, &win_h)) {
        hl->target_x = win_x;
        hl->target_y = win_y;
        hl->target_w = win_w;
        hl->target_h = win_h;

        // Draw first ripple frame immediately, then animate
        hl->ripple_step = 0;

        int screen_h = DisplayHeight(app->display, DefaultScreen(app->display));
        int size = screen_h * RIPPLE_START_FRAC / 100;
        int cx = win_x + win_w / 2;
        int cy = win_y + win_h / 2;
        unsigned long color = make_color(app->display, RIPPLE_COLOR_R, RIPPLE_COLOR_G, RIPPLE_COLOR_B);
        create_ripple_rect(app, cx - size / 2, cy - size / 2, size, size, RIPPLE_BAR_WIDTH, color);
        XFlush(app->display);

        hl->ripple_timer = g_timeout_add(RIPPLE_INTERVAL_MS, ripple_step_cb, app);
    }

    hl->active = 1;

    log_info("Highlight: ripple on 0x%lx", target);
}
