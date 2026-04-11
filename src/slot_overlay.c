#include "slot_overlay.h"
#include "app_data.h"
#include "monitor_move.h"
#include "log.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <string.h>

// Catppuccin Mocha colors
#define OVERLAY_BG_COLOR    0x1e1e2e
#define OVERLAY_TEXT_COLOR   "#cdd6f4"
#define OVERLAY_BORDER_COLOR 0x585b70

// Overlay size as fraction of screen height (8 = 1/8th of screen)
#define OVERLAY_SCREEN_FRACTION 8

// Font size as fraction of overlay size (3 = 1/3rd)
#define OVERLAY_FONT_FRACTION 3

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
    }

    if (state->count > 0) {
        XFlush(app->display);
        log_debug("Destroyed %d slot overlays", state->count);
    }
    state->count = 0;
}

static Window create_overlay_window(Display *display, int x, int y,
                                     int width, int height) {
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    attrs.background_pixel = OVERLAY_BG_COLOR;
    attrs.border_pixel = OVERLAY_BORDER_COLOR;

    Window root = DefaultRootWindow(display);
    Window win = XCreateWindow(display, root,
        x, y, width, height,
        0,  // no border
        CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWBackPixel | CWBorderPixel,
        &attrs);

    // Set window type to notification (hint for compositors)
    Atom wtype = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom wtype_notif = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
    XChangeProperty(display, win, wtype, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&wtype_notif, 1);

    return win;
}

static void draw_number(Display *display, Window win, int screen,
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

    // Create Xft draw context
    Visual *visual = DefaultVisual(display, screen);
    Colormap colormap = DefaultColormap(display, screen);
    XftDraw *draw = XftDrawCreate(display, win, visual, colormap);
    if (!draw) {
        XftFontClose(display, font);
        return;
    }

    // Parse text color
    XftColor color;
    XftColorAllocName(display, visual, colormap, OVERLAY_TEXT_COLOR, &color);

    // Measure text for centering
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

        Window ow = create_overlay_window(app->display, ox, oy, size, size);
        XMapRaised(app->display, ow);

        // Need to process the MapNotify before drawing
        XFlush(app->display);

        draw_number(app->display, ow, screen, size, size, i + 1);

        state->windows[state->count++] = ow;
        log_debug("Slot overlay %d at (%d,%d) size %dx%d", i + 1, ox, oy, size, size);
    }

    XFlush(app->display);

    // Schedule auto-destroy
    state->timeout_id = g_timeout_add(app->config.slot_overlay_duration_ms,
                                       destroy_overlays_timeout, app);

    log_info("Showing %d slot overlays for %dms",
             state->count, app->config.slot_overlay_duration_ms);
}
