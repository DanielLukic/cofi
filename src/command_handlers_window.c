#include "command_handlers_window.h"

#include "app_data.h"
#include "log.h"
#include "monitor_move.h"
#include "overlay_manager.h"
#include "x11_utils.h"

#include <X11/Xlib.h>
#include <string.h>
#include <unistd.h>

extern void hide_window(AppData *app);

gboolean cmd_pull_window(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for pull");
        return FALSE;
    }

    int current_workspace = get_current_desktop(app->display);
    log_info("USER: Pulling window '%s' to current workspace %d",
             window->title, current_workspace + 1);
    move_window_to_desktop(app->display, window->id, current_workspace);
    return TRUE;
}

gboolean cmd_toggle_monitor(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for monitor toggle");
        return FALSE;
    }

    move_window_to_next_monitor(app);
    return TRUE;
}

gboolean cmd_skip_taskbar(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for skip taskbar toggle");
        return FALSE;
    }

    toggle_window_state(app->display, window->id, "_NET_WM_STATE_SKIP_TASKBAR");
    return TRUE;
}

gboolean cmd_always_on_top(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for always on top toggle");
        return FALSE;
    }

    toggle_window_state(app->display, window->id, "_NET_WM_STATE_ABOVE");
    return TRUE;
}

gboolean cmd_always_below(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for always below toggle");
        return FALSE;
    }

    toggle_window_state(app->display, window->id, "_NET_WM_STATE_BELOW");
    return TRUE;
}

gboolean cmd_every_workspace(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for every workspace toggle");
        return FALSE;
    }

    toggle_window_state(app->display, window->id, "_NET_WM_STATE_STICKY");
    return TRUE;
}

gboolean cmd_close_window(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for closing");
        return FALSE;
    }

    close_window(app->display, window->id);
    return TRUE;
}

gboolean cmd_minimize_window(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for minimizing");
        return FALSE;
    }

    if (get_window_state(app->display, window->id, "_NET_WM_STATE_HIDDEN")) {
        activate_window(app->display, window->id);
        log_info("CMD: Restored minimized window '%s'", window->title);
    } else {
        minimize_window(app->display, window->id);
        log_info("CMD: Minimized window '%s'", window->title);
    }

    hide_window(app);
    return TRUE;
}

gboolean cmd_maximize_window(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for maximizing");
        return FALSE;
    }

    toggle_window_state(app->display, window->id, "_NET_WM_STATE_MAXIMIZED_BOTH");
    return TRUE;
}

gboolean cmd_horizontal_maximize(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for horizontal maximizing");
        return FALSE;
    }

    toggle_window_state(app->display, window->id, "_NET_WM_STATE_MAXIMIZED_HORZ");
    return TRUE;
}

gboolean cmd_vertical_maximize(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for vertical maximizing");
        return FALSE;
    }

    toggle_window_state(app->display, window->id, "_NET_WM_STATE_MAXIMIZED_VERT");
    return TRUE;
}

gboolean cmd_assign_name(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_error("No window selected for name assignment");
        return TRUE;
    }

    if (app->current_tab != TAB_WINDOWS) {
        log_error("Name assignment only available from Windows tab");
        return TRUE;
    }

    show_name_assign_overlay(app);
    log_info("CMD: Opening name assignment overlay for window 0x%lx", window->id);
    return FALSE;
}

typedef struct {
    Window id;
    int x;
    int y;
    int width;
    int height;
    gboolean max_vert;
    gboolean max_horz;
} SwapWindowState;

typedef struct {
    Atom net_wm_state;
    Atom max_vert;
    Atom max_horz;
} SwapAtoms;

static WindowInfo *find_swap_partner(AppData *app, Window selected_id) {
    for (int i = 0; i < app->filtered_count; i++) {
        if (app->filtered[i].id != selected_id) {
            return &app->filtered[i];
        }
    }
    return NULL;
}

static gboolean load_swap_state(AppData *app, WindowInfo *window,
                                SwapWindowState *state, const char *label) {
    state->id = window->id;
    if (!get_window_geometry(app->display, window->id,
                             &state->x, &state->y, &state->width, &state->height)) {
        log_error("Failed to get geometry for %s", label);
        return FALSE;
    }

    state->max_vert = get_window_state(app->display, window->id, "_NET_WM_STATE_MAXIMIZED_VERT");
    state->max_horz = get_window_state(app->display, window->id, "_NET_WM_STATE_MAXIMIZED_HORZ");
    return TRUE;
}

static SwapAtoms init_swap_atoms(Display *display) {
    SwapAtoms atoms;
    atoms.net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    atoms.max_vert = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    atoms.max_horz = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    return atoms;
}

static void send_maximize_change(Display *display, Window window, const SwapAtoms *atoms,
                                 long action, gboolean set_vert, gboolean set_horz) {
    if (!set_vert && !set_horz) {
        return;
    }

    XEvent event;
    memset(&event, 0, sizeof(event));
    event.type = ClientMessage;
    event.xclient.window = window;
    event.xclient.message_type = atoms->net_wm_state;
    event.xclient.format = 32;
    event.xclient.data.l[0] = action;

    if (action == 0) {
        event.xclient.data.l[1] = atoms->max_vert;
        event.xclient.data.l[2] = atoms->max_horz;
        XSendEvent(display, DefaultRootWindow(display), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &event);
        return;
    }

    if (set_vert) {
        event.xclient.data.l[1] = atoms->max_vert;
        event.xclient.data.l[2] = 0;
        XSendEvent(display, DefaultRootWindow(display), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &event);
    }

    if (set_horz) {
        event.xclient.data.l[1] = atoms->max_horz;
        event.xclient.data.l[2] = 0;
        XSendEvent(display, DefaultRootWindow(display), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &event);
    }
}

static void swap_window_geometry(Display *display,
                                 const SwapWindowState *first,
                                 const SwapWindowState *second) {
    XMoveResizeWindow(display, first->id, second->x, second->y, second->width, second->height);
    XMoveResizeWindow(display, second->id, first->x, first->y, first->width, first->height);
}

static gboolean prepare_swap_states(AppData *app, WindowInfo *window,
                                    WindowInfo **partner_out,
                                    SwapWindowState *first,
                                    SwapWindowState *second) {
    if (!window) {
        log_warn("No window selected for swap");
        return FALSE;
    }

    if (app->filtered_count < 2) {
        log_warn("Need at least 2 windows to swap (have %d)", app->filtered_count);
        return FALSE;
    }

    *partner_out = find_swap_partner(app, window->id);
    if (!*partner_out) {
        log_warn("Could not find second window to swap with");
        return FALSE;
    }

    return load_swap_state(app, window, first, "window 1") &&
           load_swap_state(app, *partner_out, second, "window 2");
}

static void run_swap_sequence(AppData *app, const SwapWindowState *first,
                              const SwapWindowState *second) {
    SwapAtoms atoms = init_swap_atoms(app->display);
    send_maximize_change(app->display, first->id, &atoms, 0, first->max_vert, first->max_horz);
    send_maximize_change(app->display, second->id, &atoms, 0, second->max_vert, second->max_horz);
    XFlush(app->display);
    usleep(100000);

    swap_window_geometry(app->display, first, second);
    XFlush(app->display);
    usleep(50000);

    send_maximize_change(app->display, first->id, &atoms, 1, second->max_vert, second->max_horz);
    send_maximize_change(app->display, second->id, &atoms, 1, first->max_vert, first->max_horz);
    XFlush(app->display);
    usleep(50000);

    swap_window_geometry(app->display, first, second);
    XFlush(app->display);
}

gboolean cmd_swap_windows(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    WindowInfo *partner = NULL;
    SwapWindowState first = {0};
    SwapWindowState second = {0};

    if (!prepare_swap_states(app, window, &partner, &first, &second)) {
        return FALSE;
    }

    log_info("Swapping windows: '%s' <-> '%s'", window->title, partner->title);
    run_swap_sequence(app, &first, &second);
    log_info("Window swap completed");
    return TRUE;
}
