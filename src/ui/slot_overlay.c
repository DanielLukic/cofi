#include "ui/slot_overlay.h"
#include "core/app/app_data.h"
#include "x11/monitor_move.h"
#include "core/log/log.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrender.h>
#include <cairo/cairo-xlib.h>
#include <cairo/cairo-xlib-xrender.h>
#include <stdio.h>
#include <string.h>

// Catppuccin Mocha colors
#define OVERLAY_BG_COLOR    0x1e1e2e
#define OVERLAY_TEXT_COLOR   "#cdd6f4"
#define OVERLAY_BORDER_COLOR 0x585b70

#define OVERLAY_BG_R       (0x1e / 255.0)
#define OVERLAY_BG_G       (0x1e / 255.0)
#define OVERLAY_BG_B       (0x2e / 255.0)
#define OVERLAY_BORDER_R   (0x58 / 255.0)
#define OVERLAY_BORDER_G   (0x5b / 255.0)
#define OVERLAY_BORDER_B   (0x70 / 255.0)
#define OVERLAY_BG_ALPHA   0.7
#define OVERLAY_BORDER_ALPHA 0.8

// Overlay size as fraction of screen height (8 = 1/8th of screen)
#define OVERLAY_SCREEN_FRACTION 8

// Font size as fraction of overlay size (3 = 1/3rd)
#define OVERLAY_FONT_FRACTION 3

#define OVERLAY_BORDER_WIDTH 2.0

typedef struct {
    Visual *visual;
    int depth;
    XRenderPictFormat *format;
} ArgbVisualInfo;

static int g_argb_checked = 0;
static int g_argb_available = 0;
static ArgbVisualInfo g_argb;

static gboolean compositor_running(Display *display, int screen) {
    char atom_name[32];
    snprintf(atom_name, sizeof(atom_name), "_NET_WM_CM_S%d", screen);
    Atom compositor_atom = XInternAtom(display, atom_name, False);
    return XGetSelectionOwner(display, compositor_atom) != None;
}

static gboolean find_argb_visual(Display *display, int screen, ArgbVisualInfo *out) {
    XVisualInfo template;
    memset(&template, 0, sizeof(template));
    template.screen = screen;

    int count = 0;
    XVisualInfo *infos = XGetVisualInfo(display, VisualScreenMask, &template, &count);
    if (!infos) return FALSE;

    for (int i = 0; i < count; i++) {
        if (infos[i].depth != 32 || infos[i].class != TrueColor) {
            continue;
        }

        XRenderPictFormat *format = XRenderFindVisualFormat(display, infos[i].visual);
        if (format && format->type == PictTypeDirect && format->direct.alphaMask) {
            out->visual = infos[i].visual;
            out->depth = infos[i].depth;
            out->format = format;
            XFree(infos);
            return TRUE;
        }
    }

    XFree(infos);
    return FALSE;
}

static gboolean get_argb_visual(Display *display, int screen, ArgbVisualInfo *out) {
    if (!g_argb_checked) {
        g_argb_checked = 1;
        g_argb_available = compositor_running(display, screen) &&
                           find_argb_visual(display, screen, &g_argb);
    }

    if (!g_argb_available) return FALSE;
    *out = g_argb;
    return TRUE;
}

static gboolean destroy_overlays_timeout(gpointer data) {
    AppData *app = (AppData *)data;
    destroy_slot_overlays(app);
    return FALSE;  // Remove timeout
}

void init_slot_overlay_state(SlotOverlayState *state) {
    memset(state, 0, sizeof(SlotOverlayState));
}

void destroy_slot_overlays(AppData *app) {
    SlotOverlayState *state = &app->slot_overlays;

    if (state->timeout_id > 0) {
        g_source_remove(state->timeout_id);
        state->timeout_id = 0;
    }

    for (int i = 0; i < state->count; i++) {
        if (state->windows[i] != 0) {
            XDestroyWindow(app->display, state->windows[i]);
            state->windows[i] = 0;
        }
        if (state->colormaps[i] != 0) {
            XFreeColormap(app->display, state->colormaps[i]);
            state->colormaps[i] = 0;
        }
    }

    if (state->count > 0) {
        XFlush(app->display);
        log_debug("Destroyed %d slot overlays", state->count);
    }
    state->count = 0;
}

static void set_overlay_window_type(Display *display, Window win) {
    Atom wtype = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom wtype_notif = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
    XChangeProperty(display, win, wtype, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&wtype_notif, 1);
}

static Window create_overlay_window(Display *display, int x, int y,
                                    int width, int height,
                                    const ArgbVisualInfo *argb,
                                    Colormap *out_colormap) {
    XSetWindowAttributes attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.override_redirect = True;

    Window root = DefaultRootWindow(display);
    Window win;

    *out_colormap = 0;
    if (argb) {
        attrs.colormap = XCreateColormap(display, root, argb->visual, AllocNone);
        attrs.background_pixmap = None;
        attrs.border_pixel = 0;
        *out_colormap = attrs.colormap;

        win = XCreateWindow(display, root,
            x, y, width, height,
            0,
            argb->depth, InputOutput, argb->visual,
            CWOverrideRedirect | CWColormap | CWBackPixmap | CWBorderPixel,
            &attrs);
    } else {
        attrs.background_pixel = OVERLAY_BG_COLOR;
        attrs.border_pixel = OVERLAY_BORDER_COLOR;

        win = XCreateWindow(display, root,
            x, y, width, height,
            0,  // no border
            CopyFromParent, InputOutput, CopyFromParent,
            CWOverrideRedirect | CWBackPixel | CWBorderPixel,
            &attrs);
    }

    set_overlay_window_type(display, win);
    return win;
}

static void draw_argb_background(Display *display, Window win, int screen,
                                 int width, int height,
                                 const ArgbVisualInfo *argb,
                                 double bg_alpha) {
    Screen *xscreen = ScreenOfDisplay(display, screen);
    cairo_surface_t *surface = cairo_xlib_surface_create_with_xrender_format(
        display, win, xscreen, argb->format, width, height);
    cairo_t *cr = cairo_create(surface);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);

    cairo_set_source_rgba(cr, OVERLAY_BG_R, OVERLAY_BG_G, OVERLAY_BG_B, bg_alpha);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr, OVERLAY_BORDER_R, OVERLAY_BORDER_G, OVERLAY_BORDER_B,
                          OVERLAY_BORDER_ALPHA);
    cairo_set_line_width(cr, OVERLAY_BORDER_WIDTH);
    cairo_rectangle(cr, 1.0, 1.0, width - 2.0, height - 2.0);
    cairo_stroke(cr);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

static void draw_number(Display *display, Window win, int screen,
                        Visual *visual, Colormap colormap,
                        int width, int height, int number) {
    char text[4];
    snprintf(text, sizeof(text), "%d", number);

    // Font size derived from overlay size
    int font_size = height / OVERLAY_FONT_FRACTION;
    if (font_size < 16) font_size = 16;

    // Open Xft font
    char font_desc[64];
    snprintf(font_desc, sizeof(font_desc), "monospace:size=%d:bold", font_size);
    XftFont *font = XftFontOpenName(display, screen, font_desc);
    if (!font) {
        log_warn("Failed to open Xft font, trying fallback");
        font = XftFontOpenName(display, screen, "fixed");
        if (!font) return;
    }

    XftDraw *draw = XftDrawCreate(display, win, visual, colormap);
    if (!draw) {
        XftFontClose(display, font);
        return;
    }

    XftColor color;
    XftColorAllocName(display, visual, colormap, OVERLAY_TEXT_COLOR, &color);

    XGlyphInfo extents;
    XftTextExtentsUtf8(display, font, (FcChar8 *)text, strlen(text), &extents);

    int text_x = (width - extents.xOff) / 2 - extents.x;
    int text_y = (height - (font->ascent + font->descent)) / 2 + font->ascent;

    XftDrawStringUtf8(draw, &color, font, text_x, text_y,
                      (FcChar8 *)text, strlen(text));

    XftColorFree(display, visual, colormap, &color);
    XftDrawDestroy(draw);
    XftFontClose(display, font);
}

void show_slot_overlays(AppData *app) {
    log_debug("show_slot_overlays: entered, duration=%d, slot_count=%d",
              app->config.slot_overlay_duration_ms, app->workspace_slots.count);
    // Check if overlays are disabled
    if (app->config.slot_overlay_duration_ms <= 0) {
        log_debug("show_slot_overlays: disabled (duration=%d)", app->config.slot_overlay_duration_ms);
        return;
    }

    // Destroy any existing overlays first
    destroy_slot_overlays(app);

    WorkspaceSlotManager *slots = &app->workspace_slots;
    if (slots->count == 0) return;

    SlotOverlayState *state = &app->slot_overlays;
    int screen = DefaultScreen(app->display);

    ArgbVisualInfo argb;
    gboolean use_argb = get_argb_visual(app->display, screen, &argb);

    for (int i = 0; i < slots->count; i++) {
        // Get target window geometry
        int win_x, win_y, win_w, win_h;
        if (!get_window_geometry(app->display, slots->slots[i].id,
                                 &win_x, &win_y, &win_w, &win_h)) {
            continue;
        }

        // Overlay size: square, fraction of screen height
        int screen_h = DisplayHeight(app->display, screen);
        int size = screen_h / OVERLAY_SCREEN_FRACTION;

        // Prefer centroid of largest visible fragment, if available.
        // Fallback to full-window center for backwards compatibility.
        int cx, cy;
        if (slots->slots[i].has_overlay_pos) {
            cx = slots->slots[i].overlay_x;
            cy = slots->slots[i].overlay_y;
        } else {
            cx = win_x + win_w / 2;
            cy = win_y + win_h / 2;
        }

        int ox = cx - size / 2;
        int oy = cy - size / 2;

        Colormap colormap = 0;
        Window ow = create_overlay_window(app->display, ox, oy, size, size,
                                          use_argb ? &argb : NULL, &colormap);
        XMapRaised(app->display, ow);

        // Need to process the MapNotify before drawing
        XFlush(app->display);

        if (use_argb) {
            draw_argb_background(app->display, ow, screen, size, size, &argb,
                                 OVERLAY_BG_ALPHA);
            draw_number(app->display, ow, screen, argb.visual, colormap,
                        size, size, i + 1);
        } else {
            draw_number(app->display, ow, screen,
                        DefaultVisual(app->display, screen),
                        DefaultColormap(app->display, screen),
                        size, size, i + 1);
        }

        state->windows[state->count] = ow;
        state->colormaps[state->count] = colormap;
        state->count++;
        log_debug("Slot overlay %d at (%d,%d) size %dx%d argb=%d alpha=%.1f",
                  i + 1, ox, oy, size, size, use_argb, OVERLAY_BG_ALPHA);
    }

    XFlush(app->display);

    // Schedule auto-destroy
    state->timeout_id = g_timeout_add(app->config.slot_overlay_duration_ms,
                                       destroy_overlays_timeout, app);

    log_info("Showing %d slot overlays for %dms",
             state->count, app->config.slot_overlay_duration_ms);
}
