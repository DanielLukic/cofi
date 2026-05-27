#include "commands/command_handlers_workspace.h"

#include "core/app/app_data.h"
#include "ui/display.h"
#include "core/log/log.h"
#include "ui/overlay_manager.h"
#include "harpoon/workspace_slots.h"
#include "x11/workspace_utils.h"
#include "ui/window_highlight.h"
#include "x11/window_list.h"
#include "x11/x11_events.h"
#include "x11/x11_utils.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

extern void hide_window(AppData *app);

gboolean cmd_change_workspace(AppData *app, WindowInfo *window, const char *args) {
    if (!window) {
        log_warn("No window selected for workspace change");
        return FALSE;
    }

    if (strlen(args) > 0) {
        int target = resolve_workspace_from_arg(app->display, args, app->config.workspaces_per_row);
        if (target >= 0) {
            log_info("USER: Moving window '%s' to workspace %d", window->title, target + 1);
            move_window_to_desktop(app->display, window->id, target);
            return TRUE;
        }

        log_warn("Invalid workspace target: %s", args);
        return FALSE;
    }

    show_workspace_jump_overlay(app);
    return FALSE;
}

gboolean cmd_jump_workspace(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    if (strlen(args) > 0) {
        int target = resolve_workspace_from_arg(app->display, args, app->config.workspaces_per_row);
        if (target >= 0) {
            log_info("USER: Switching to workspace %d", target + 1);
            switch_to_desktop(app->display, target);
            return TRUE;
        }

        log_warn("Invalid workspace target: %s", args);
        return FALSE;
    }

    show_workspace_jump_overlay(app);
    return FALSE;
}

static gboolean parse_jump_slot_arg(const char *args, int *slot_out) {
    if (!args || !slot_out) {
        return FALSE;
    }

    while (*args && isspace((unsigned char)*args)) {
        args++;
    }
    if (*args == '\0') {
        return FALSE;
    }

    errno = 0;
    char *endptr = NULL;
    long slot = strtol(args, &endptr, 10);
    if (errno != 0 || endptr == args) {
        return FALSE;
    }

    while (*endptr && isspace((unsigned char)*endptr)) {
        endptr++;
    }
    if (*endptr != '\0') {
        return FALSE;
    }

    if (slot < 1 || slot > 9) {
        return FALSE;
    }

    *slot_out = (int)slot;
    return TRUE;
}

gboolean cmd_jump_slot(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    int slot = 0;
    if (!parse_jump_slot_arg(args, &slot)) {
        log_warn("Invalid jump-slot argument '%s' (expected N in 1-9)", args ? args : "");
        return FALSE;
    }

    get_window_list(app);
    assign_workspace_slots(app);

    Window target = get_workspace_slot_window(&app->workspace_slots, slot);
    if (target == 0) {
        log_warn("No window assigned to workspace slot %d", slot);
        return FALSE;
    }

    set_workspace_switch_state(1);
    activate_window(app->display, target);
    highlight_window(app, target);
    hide_window(app);
    log_info("Jumped to workspace slot %d -> window 0x%lx", slot, target);
    return TRUE;
}

gboolean cmd_rename_workspace(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    int workspace_index;

    if (strlen(args) > 0) {
        int workspace_num = atoi(args);
        if (workspace_num < 1 || workspace_num > 36) {
            log_warn("Invalid workspace number: %d (must be 1-36)", workspace_num);
            return FALSE;
        }

        int workspace_count = get_number_of_desktops(app->display);
        if (workspace_num > workspace_count) {
            log_warn("Workspace %d does not exist (only %d workspaces available)",
                     workspace_num, workspace_count);
            return FALSE;
        }

        workspace_index = workspace_num - 1;
        log_info("USER: Renaming workspace %d", workspace_num);
        show_workspace_rename_overlay(app, workspace_index);
        return TRUE;
    }

    workspace_index = get_current_desktop(app->display);
    log_info("USER: Renaming current workspace (index %d)", workspace_index);
    show_workspace_rename_overlay(app, workspace_index);
    return TRUE;
}

static gboolean window_is_sticky_desktop(const WindowInfo *window) {
    return window->desktop == -1 || window->desktop == 0xFFFFFFFF;
}

static gboolean should_move_window(AppData *app, WindowInfo *win, int current_workspace) {
    if (strcmp(win->type, "Special") == 0) {
        log_debug("Skipping special window: %s", win->title);
        return FALSE;
    }

    if (win->desktop != current_workspace) {
        if (window_is_sticky_desktop(win)) {
            log_debug("Skipping sticky window: %s", win->title);
        }
        return FALSE;
    }

    if (get_window_state(app->display, win->id, "_NET_WM_STATE_STICKY")) {
        log_debug("Skipping sticky window (state check): %s", win->title);
        return FALSE;
    }

    return TRUE;
}

static int collect_windows_to_move(AppData *app, int current_workspace) {
    app->windows_to_move_count = 0;

    for (int i = 0; i < app->window_count; i++) {
        WindowInfo *win = &app->windows[i];
        if (!should_move_window(app, win, current_workspace)) {
            continue;
        }

        if (app->windows_to_move_count >= MAX_WINDOWS) {
            continue;
        }

        app->windows_to_move[app->windows_to_move_count++] = win->id;
        log_debug("Will move window: %s (ID: 0x%lx)", win->title, win->id);
    }

    return app->windows_to_move_count;
}

static void move_collected_windows(AppData *app, int target_workspace) {
    for (int i = 0; i < app->windows_to_move_count; i++) {
        move_window_to_desktop(app->display, app->windows_to_move[i], target_workspace);
    }
}

static gboolean move_collected_windows_to_target(AppData *app, const char *args,
                                                  int current_workspace) {
    int target = resolve_workspace_from_arg(app->display, args, app->config.workspaces_per_row);
    if (target < 0) {
        log_warn("Invalid workspace target: %s", args);
        return FALSE;
    }

    move_collected_windows(app, target);
    switch_to_desktop(app->display, target);
    log_info("USER: Moved %d windows from workspace %d to %d",
             app->windows_to_move_count, current_workspace + 1, target + 1);
    return TRUE;
}

gboolean cmd_move_all_to_workspace(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    if (!app || !app->display) {
        log_warn("Cannot move windows: display not available");
        return FALSE;
    }

    int current_workspace = get_current_desktop(app->display);
    int movable_count = collect_windows_to_move(app, current_workspace);
    if (movable_count == 0) {
        log_warn("No movable windows found on current workspace %d", current_workspace + 1);
        return FALSE;
    }

    log_info("USER: Found %d windows to move from workspace %d", movable_count, current_workspace + 1);
    if (!args || args[0] == '\0') {
        show_workspace_move_all_overlay(app);
        return FALSE;
    }

    return move_collected_windows_to_target(app, args, current_workspace);
}

