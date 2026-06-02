#include "x11/xrandr_helpers.h"
#include "core/log/log.h"

#include <X11/extensions/Xrandr.h>
#include <stdlib.h>

int get_monitors_xrandr(Display *display, MonitorInfo **monitors) {
    Window root = DefaultRootWindow(display);
    XRRScreenResources *screen_resources;
    int monitor_count = 0;
    MonitorInfo *monitor_list = NULL;

    int xrandr_event_base, xrandr_error_base;
    if (!XRRQueryExtension(display, &xrandr_event_base, &xrandr_error_base)) {
        log_error("XRandR extension not available");
        *monitors = NULL;
        return 0;
    }

    screen_resources = XRRGetScreenResources(display, root);
    if (!screen_resources) {
        log_error("Failed to get XRandR screen resources");
        *monitors = NULL;
        return 0;
    }

    for (int i = 0; i < screen_resources->ncrtc; i++) {
        XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(display, screen_resources,
                                                screen_resources->crtcs[i]);
        if (crtc_info && crtc_info->noutput > 0 && crtc_info->mode != None) {
            monitor_count++;
        }
        if (crtc_info) XRRFreeCrtcInfo(crtc_info);
    }

    if (monitor_count == 0) {
        log_warn("No active monitors found via XRandR");
        XRRFreeScreenResources(screen_resources);
        *monitors = NULL;
        return 0;
    }

    monitor_list = malloc(monitor_count * sizeof(MonitorInfo));
    if (!monitor_list) {
        log_error("Failed to allocate memory for monitor list");
        XRRFreeScreenResources(screen_resources);
        *monitors = NULL;
        return 0;
    }

    int monitor_index = 0;
    for (int i = 0; i < screen_resources->ncrtc && monitor_index < monitor_count; i++) {
        XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(display, screen_resources,
                                                screen_resources->crtcs[i]);
        if (crtc_info && crtc_info->noutput > 0 && crtc_info->mode != None) {
            monitor_list[monitor_index].x = crtc_info->x;
            monitor_list[monitor_index].y = crtc_info->y;
            monitor_list[monitor_index].width = crtc_info->width;
            monitor_list[monitor_index].height = crtc_info->height;

            log_debug("Monitor %d: %dx%d at (%d,%d)", monitor_index,
                      monitor_list[monitor_index].width, monitor_list[monitor_index].height,
                      monitor_list[monitor_index].x, monitor_list[monitor_index].y);

            monitor_index++;
        }
        if (crtc_info) XRRFreeCrtcInfo(crtc_info);
    }

    XRRFreeScreenResources(screen_resources);
    *monitors = monitor_list;
    return monitor_index;
}

int get_window_monitor_xrandr(Display *display, int win_x, int win_y,
                              int win_width, int win_height) {
    MonitorInfo *monitors;
    int monitor_count = get_monitors_xrandr(display, &monitors);

    if (monitor_count == 0) {
        return -1;
    }

    int current_monitor = -1;
    int win_center_x = win_x + win_width / 2;
    int win_center_y = win_y + win_height / 2;

    log_debug("Window center: (%d, %d)", win_center_x, win_center_y);

    for (int i = 0; i < monitor_count; i++) {
        if (win_center_x >= monitors[i].x && win_center_x < monitors[i].x + monitors[i].width &&
            win_center_y >= monitors[i].y && win_center_y < monitors[i].y + monitors[i].height) {
            current_monitor = i;
            log_debug("Window is on monitor %d", i);
            break;
        }
    }

    free(monitors);
    return current_monitor;
}
