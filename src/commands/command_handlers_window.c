#include "commands/command_handlers_window.h"

#include "core/app/app_data.h"
#include "harpoon/harpoon.h"
#include "harpoon/harpoon_config.h"
#include "harpoon/key_handler_harpoon.h"
#include "core/log/log.h"
#include "matching/match_entry.h"
#include "matching/match_entry_config.h"
#include "x11/monitor_move.h"
#include "ui/overlay_manager.h"
#include "core/slot_store/slot_store.h"
#include "geom/window_geometry_matching.h"
#include "x11/x11_utils.h"

#include <X11/Xlib.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
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

gboolean cmd_toggle_monitor(AppData *app, WindowInfo *window, const char *args) {
    if (!window) {
        log_warn("No window selected for monitor toggle");
        return FALSE;
    }

    if (!args || args[0] == '\0') {
        move_window_to_next_monitor(app);
        return TRUE;
    }

    errno = 0;
    char *end = NULL;
    long index = strtol(args, &end, 10);
    if (errno != 0 || end == args || *end != '\0' || index < 0 || index > INT_MAX) {
        log_warn("Usage: tm [N] (got '%s')", args);
        return FALSE;
    }

    if (!move_window_to_monitor_index(app, window, (int)index)) {
        log_warn("Failed to move window to monitor %ld", index);
        return FALSE;
    }

    return TRUE;
}

static gboolean parse_state_action_arg(const char *args, const char *usage,
                                       WindowStateAction *action_out) {
    if (!action_out) {
        return FALSE;
    }

    *action_out = WINDOW_STATE_TOGGLE;
    if (!args || args[0] == '\0' || strcmp(args, "toggle") == 0) {
        return TRUE;
    }
    if (strcmp(args, "on") == 0 || strcmp(args, "+") == 0) {
        *action_out = WINDOW_STATE_SET;
        return TRUE;
    }
    if (strcmp(args, "off") == 0 || strcmp(args, "-") == 0) {
        *action_out = WINDOW_STATE_UNSET;
        return TRUE;
    }

    log_warn("Usage: %s (got '%s')", usage, args);
    return FALSE;
}

gboolean cmd_skip_taskbar(AppData *app, WindowInfo *window, const char *args) {
    if (!window) {
        log_warn("No window selected for skip taskbar toggle");
        return FALSE;
    }

    WindowStateAction action = WINDOW_STATE_TOGGLE;
    if (!parse_state_action_arg(args, "sb [toggle|on|off]", &action)) {
        return FALSE;
    }

    set_window_skip_taskbar(app->display, window->id, action);
    return TRUE;
}

gboolean cmd_always_on_top(AppData *app, WindowInfo *window, const char *args) {
    if (!window) {
        log_warn("No window selected for always on top toggle");
        return FALSE;
    }

    WindowStateAction action = WINDOW_STATE_TOGGLE;
    if (!parse_state_action_arg(args, "aot [toggle|on|off]", &action)) {
        return FALSE;
    }

    set_window_above(app->display, window->id, action);
    return TRUE;
}

gboolean cmd_always_below(AppData *app, WindowInfo *window, const char *args) {
    if (!window) {
        log_warn("No window selected for always below toggle");
        return FALSE;
    }

    WindowStateAction action = WINDOW_STATE_TOGGLE;
    if (!parse_state_action_arg(args, "ab [toggle|on|off]", &action)) {
        return FALSE;
    }

    set_window_below(app->display, window->id, action);
    return TRUE;
}

gboolean cmd_every_workspace(AppData *app, WindowInfo *window, const char *args) {
    if (!window) {
        log_warn("No window selected for every workspace toggle");
        return FALSE;
    }

    WindowStateAction action = WINDOW_STATE_TOGGLE;
    if (!parse_state_action_arg(args, "ew [toggle|on|off]", &action)) {
        return FALSE;
    }

    set_window_sticky(app->display, window->id, action);
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

    if (window_is_hidden(app->display, window->id)) {
        activate_window(app->display, window->id);
        log_info("CMD: Restored minimized window '%s'", window->title);
    } else {
        minimize_window(app->display, window->id);
        log_info("CMD: Minimized window '%s'", window->title);
    }

    hide_window(app);
    return TRUE;
}

gboolean cmd_maximize_window(AppData *app, WindowInfo *window, const char *args) {
    if (!window) {
        log_warn("No window selected for maximizing");
        return FALSE;
    }

    WindowStateAction action = WINDOW_STATE_TOGGLE;
    if (!parse_state_action_arg(args, "mw [toggle|on|off]", &action)) {
        return FALSE;
    }

    set_window_maximized(app->display, window->id, action);
    return TRUE;
}

gboolean cmd_horizontal_maximize(AppData *app, WindowInfo *window, const char *args) {
    if (!window) {
        log_warn("No window selected for horizontal maximizing");
        return FALSE;
    }

    WindowStateAction action = WINDOW_STATE_TOGGLE;
    if (!parse_state_action_arg(args, "hm [toggle|on|off]", &action)) {
        return FALSE;
    }

    set_window_maximized_horizontal(app->display, window->id, action);
    return TRUE;
}

gboolean cmd_vertical_maximize(AppData *app, WindowInfo *window, const char *args) {
    if (!window) {
        log_warn("No window selected for vertical maximizing");
        return FALSE;
    }

    WindowStateAction action = WINDOW_STATE_TOGGLE;
    if (!parse_state_action_arg(args, "vm [toggle|on|off]", &action)) {
        return FALSE;
    }

    set_window_maximized_vertical(app->display, window->id, action);
    return TRUE;
}

gboolean cmd_assign_name(AppData *app, WindowInfo *window, const char *args) {
    if (!window) {
        log_error("No window selected for name assignment");
        return TRUE;
    }

    if (app->current_tab != TAB_WINDOWS) {
        log_error("Name assignment only available from Windows tab");
        return TRUE;
    }

    const char *label_start = args ? args : "";
    while (*label_start == ' ' || *label_start == '\t' || *label_start == '\n' || *label_start == '\r') {
        label_start++;
    }

    const char *label_end = label_start + strlen(label_start);
    while (label_end > label_start &&
           (label_end[-1] == ' ' || label_end[-1] == '\t' || label_end[-1] == '\n' || label_end[-1] == '\r')) {
        label_end--;
    }

    size_t label_len = (size_t)(label_end - label_start);
    if (label_len > 0) {
        char inline_label[MAX_TITLE_LEN];
        size_t copy_len = label_len;
        if (copy_len >= sizeof(inline_label)) {
            copy_len = sizeof(inline_label) - 1;
        }

        memcpy(inline_label, label_start, copy_len);
        inline_label[copy_len] = '\0';
        match_entry_assign_custom_name(&app->matching, window, inline_label);
        save_match_entries(&app->matching);
        hide_window(app);
        log_info("CMD: Assigned inline name '%s' to window 0x%lx", inline_label, window->id);
        return TRUE;
    }

    show_name_assign_overlay(app);
    log_info("CMD: Opening name assignment overlay for window 0x%lx", window->id);
    return FALSE;
}

static gboolean parse_slot_key_arg(const char *args, char *slot_key_out) {
    if (!args || !slot_key_out) {
        return FALSE;
    }

    while (*args == ' ' || *args == '\t' || *args == '\n' || *args == '\r') {
        args++;
    }

    if (*args == '\0') {
        return FALSE;
    }

    char slot_key = *args++;
    while (*args == ' ' || *args == '\t' || *args == '\n' || *args == '\r') {
        args++;
    }

    if (*args != '\0') {
        return FALSE;
    }

    *slot_key_out = slot_key;
    return TRUE;
}

gboolean cmd_harpoon_set(AppData *app, WindowInfo *window, const char *args) {
    if (app->current_tab != TAB_WINDOWS) {
        log_warn("Harpoon set only available from Windows tab");
        return TRUE;
    }
    if (!window) {
        log_warn("No window selected for harpoon set");
        return TRUE;
    }

    char slot_key = '\0';
    if (!parse_slot_key_arg(args, &slot_key)) {
        log_warn("Invalid hs argument '%s' (expected one slot key)", args ? args : "");
        return TRUE;
    }

    int slot = slot_index_from_key(slot_key);
    if (slot < 0) {
        log_warn("Invalid hs slot key '%c' (allowed: 0-9, a-z)", slot_key);
        return TRUE;
    }

    if (!harpoon_assign_or_toggle_window(app, window, slot)) {
        log_warn("Failed to assign selected window to slot %d", slot);
        return TRUE;
    }
    hide_window(app);
    log_info("CMD: Assigned window 0x%lx to harpoon slot key '%c'", window->id, slot_key);
    return TRUE;
}

gboolean cmd_rename_window(AppData *app, WindowInfo *window, const char *args) {
    if (!window) {
        log_warn("No window selected for rename");
        return FALSE;
    }

    if (app->current_tab != TAB_WINDOWS) {
        log_error("Rename only available from Windows tab");
        return FALSE;
    }

    const char *title = args ? args : "";
    while (*title == ' ') title++;
    if (*title == '\0') {
        log_warn("Rename requires a title argument");
        return FALSE;
    }

    set_window_name(app->display, window->id, title);
    log_info("USER: Renamed window 0x%lx to \"%s\"", window->id, title);
    return TRUE;
}

gboolean cmd_save_layout(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for layout save");
        return FALSE;
    }

    return save_window_geometry_for_window(app, window);
}

gboolean cmd_restore_layout(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for layout restore");
        return FALSE;
    }

    return restore_window_geometry_for_window(app, window);
}

gboolean cmd_clear_layout(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for layout clear");
        return FALSE;
    }

    return clear_window_geometry_for_window(app, window);
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

    state->max_vert = window_is_maximized_vertical(app->display, window->id);
    state->max_horz = window_is_maximized_horizontal(app->display, window->id);
    return TRUE;
}

static void send_maximize_change(Display *display, Window window,
                                 WindowStateAction action,
                                 gboolean set_vert, gboolean set_horz) {
    if (!set_vert && !set_horz) {
        return;
    }

    if (action == WINDOW_STATE_UNSET) {
        set_window_maximized(display, window, WINDOW_STATE_UNSET);
        return;
    }

    if (set_vert) {
        set_window_maximized_vertical(display, window, action);
    }

    if (set_horz) {
        set_window_maximized_horizontal(display, window, action);
    }
}

static void swap_window_geometry(Display *display,
                                 const SwapWindowState *first,
                                 const SwapWindowState *second) {
    xmove_resize_frame_aware(display, first->id, second->x, second->y, second->width, second->height);
    xmove_resize_frame_aware(display, second->id, first->x, first->y, first->width, first->height);
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
    send_maximize_change(app->display, first->id, WINDOW_STATE_UNSET, first->max_vert, first->max_horz);
    send_maximize_change(app->display, second->id, WINDOW_STATE_UNSET, second->max_vert, second->max_horz);
    XFlush(app->display);
    usleep(100000);

    swap_window_geometry(app->display, first, second);
    XFlush(app->display);
    usleep(50000);

    send_maximize_change(app->display, first->id, WINDOW_STATE_SET, second->max_vert, second->max_horz);
    send_maximize_change(app->display, second->id, WINDOW_STATE_SET, first->max_vert, first->max_horz);
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
