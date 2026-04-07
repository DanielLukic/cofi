#include "command_mode.h"
#include "command_definitions.h"
#include "workspace_slots.h"
#include "log.h"
#include "tiling.h"
#include "overlay_manager.h"
#include "monitor_move.h"
#include "display.h"
#include "selection.h"
#include "command_parser.h"
#include "x11_utils.h"
#include "workspace_utils.h"
#include "hotkey_config.h"
#include "hotkeys.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

extern void hide_window(AppData *app);
extern void dispatch_hotkey_mode(AppData *app, ShowMode mode);

static void log_commanded_window(AppData *app, WindowInfo *win) {
    if (!app || !win) return;

    char truncated_title[16];
    strncpy(truncated_title, win->title, 15);
    truncated_title[15] = '\0';

    log_info("CMD: Window commanded - ID: 0x%lx, Class: %s, Title: %s",
             win->id, win->class_name, truncated_title);
}

static void activate_commanded_window(AppData *app, WindowInfo *win) {
    if (!app || !win) return;
    activate_window(app->display, win->id);
    log_commanded_window(app, win);
}

static const CommandDef *find_command_by_primary(const char *primary) {
    for (int i = 0; COMMAND_DEFINITIONS[i].primary != NULL; i++) {
        if (strcmp(primary, COMMAND_DEFINITIONS[i].primary) == 0) {
            return &COMMAND_DEFINITIONS[i];
        }
    }
    return NULL;
}

static gboolean is_empty_arg(const char *arg) {
    return !arg || arg[0] == '\0';
}

static gboolean keeps_open_without_arg(const char *primary) {
    return strcmp(primary, "cw") == 0 ||
           strcmp(primary, "jw") == 0 ||
           strcmp(primary, "maw") == 0 ||
           strcmp(primary, "tw") == 0;
}

static gboolean command_keeps_open_on_hotkey_auto(const CommandDef *cmd, const char *arg) {
    if (!cmd || !cmd->primary) {
        return FALSE;
    }

    if (cmd->keeps_open_on_hotkey_auto) {
        return TRUE;
    }

    return is_empty_arg(arg) && keeps_open_without_arg(cmd->primary);
}

gboolean should_keep_open_on_hotkey_auto(const char *command) {
    if (!command) {
        return FALSE;
    }

    char local[512] = {0};
    strncpy(local, command, sizeof(local) - 1);
    trim_whitespace_in_place(local);

    if (local[0] == '\0') {
        return FALSE;
    }

    char *cursor = local;
    char segment[256] = {0};
    while (next_command_segment(&cursor, segment, sizeof(segment))) {
        char primary[128] = {0};
        char arg[256] = {0};

        if (!parse_command_for_execution(segment, primary, arg, sizeof(primary), sizeof(arg))) {
            continue;
        }

        const CommandDef *cmd = find_command_by_primary(primary);
        if (command_keeps_open_on_hotkey_auto(cmd, arg)) {
            return TRUE;
        }
    }

    return FALSE;
}

static TileOption parse_tile_option(const char *arg) {
    if (strlen(arg) == 1) {
        switch (arg[0]) {
            case 'l': case 'L': return TILE_LEFT_HALF;
            case 'r': case 'R': return TILE_RIGHT_HALF;
            case 't': case 'T': return TILE_TOP_HALF;
            case 'b': case 'B': return TILE_BOTTOM_HALF;
            case '1': return TILE_GRID_1;
            case '2': return TILE_GRID_2;
            case '3': return TILE_GRID_3;
            case '4': return TILE_GRID_4;
            case '5': return TILE_GRID_5;
            case '6': return TILE_GRID_6;
            case '7': return TILE_GRID_7;
            case '8': return TILE_GRID_8;
            case '9': return TILE_GRID_9;
            case 'f': case 'F': return TILE_FULLSCREEN;
            case 'c': case 'C': return TILE_CENTER;
        }
    } else if (strlen(arg) == 2 && strchr("lrtbLRTB", arg[0]) && strchr("1234", arg[1])) {
        char direction = tolower(arg[0]);
        char size = arg[1];

        if (direction == 'l') {
            switch (size) {
                case '1': return TILE_LEFT_QUARTER;
                case '2': return TILE_LEFT_HALF;
                case '3': return TILE_LEFT_TWO_THIRDS;
                case '4': return TILE_LEFT_THREE_QUARTERS;
            }
        } else if (direction == 'r') {
            switch (size) {
                case '1': return TILE_RIGHT_QUARTER;
                case '2': return TILE_RIGHT_HALF;
                case '3': return TILE_RIGHT_TWO_THIRDS;
                case '4': return TILE_RIGHT_THREE_QUARTERS;
            }
        } else if (direction == 't') {
            switch (size) {
                case '1': return TILE_TOP_QUARTER;
                case '2': return TILE_TOP_HALF;
                case '3': return TILE_TOP_TWO_THIRDS;
                case '4': return TILE_TOP_THREE_QUARTERS;
            }
        } else if (direction == 'b') {
            switch (size) {
                case '1': return TILE_BOTTOM_QUARTER;
                case '2': return TILE_BOTTOM_HALF;
                case '3': return TILE_BOTTOM_TWO_THIRDS;
                case '4': return TILE_BOTTOM_THREE_QUARTERS;
            }
        }
    }

    return (TileOption)-1;
}

static gboolean execute_single_command(const char *command, AppData *app,
                                       WindowInfo *window, gboolean background) {
    char primary[128] = {0};
    char arg[256] = {0};

    if (!parse_command_for_execution(command, primary, arg, sizeof(primary), sizeof(arg))) {
        return FALSE;
    }

    if (primary[0] == '\0') {
        return TRUE;
    }

    const CommandDef *cmd = find_command_by_primary(primary);
    if (!cmd) {
        log_warn("Unknown command: '%s'. Type 'help' for available commands.", primary);
        return FALSE;
    }

    gboolean result = cmd->handler(app, window, arg);
    if (result && cmd->activates && !background) {
        activate_commanded_window(app, window);
    }

    return result;
}

static gboolean execute_commands(const char *command, AppData *app,
                                 WindowInfo *window, gboolean background) {
    char local[512] = {0};
    strncpy(local, command, sizeof(local) - 1);
    trim_whitespace_in_place(local);

    if (local[0] == '\0') {
        return TRUE;
    }

    char *cursor = local;
    char segment[256] = {0};
    while (next_command_segment(&cursor, segment, sizeof(segment))) {
        if (!execute_single_command(segment, app, window, background)) {
            return FALSE;
        }
    }

    return TRUE;
}

gboolean execute_command(const char *command, AppData *app) {
    if (!command || !app) return FALSE;
    log_info("USER: Executing command: '%s'", command);
    return execute_commands(command, app, get_selected_window(app), FALSE);
}

gboolean execute_command_with_window(const char *command, AppData *app, WindowInfo *window) {
    if (!command || !app) return FALSE;
    log_info("HOTKEY: Executing command: '%s'", command);
    return execute_commands(command, app, window, FALSE);
}

gboolean execute_command_background(const char *command, AppData *app, WindowInfo *window) {
    if (!command || !app) return FALSE;
    log_info("RULE: Executing command: '%s'", command);
    return execute_commands(command, app, window, TRUE);
}

char *generate_command_help_text(HelpFormat format) {
    size_t buffer_size = 1024;
    for (int i = 0; COMMAND_DEFINITIONS[i].primary != NULL; i++) {
        buffer_size += strlen(COMMAND_DEFINITIONS[i].help_format);
        buffer_size += strlen(COMMAND_DEFINITIONS[i].description);
        buffer_size += 100;
    }

    char *help_text = malloc(buffer_size);
    if (!help_text) {
        return NULL;
    }

    if (format == HELP_FORMAT_CLI) {
        strcpy(help_text, "COFI Command Mode Help\n");
        strcat(help_text, "======================\n\n");
    } else {
        strcpy(help_text, "");
    }

    strcat(help_text, "Available commands:\n\n");
    for (int i = 0; COMMAND_DEFINITIONS[i].primary != NULL; i++) {
        char line[256];
        snprintf(line, sizeof(line), "  %-40s - %s\n",
                 COMMAND_DEFINITIONS[i].help_format,
                 COMMAND_DEFINITIONS[i].description);
        strcat(help_text, line);
    }

    strcat(help_text, "\nUsage:\n");
    strcat(help_text, "  Press ':' to enter command mode. Press Escape to cancel.\n");
    strcat(help_text, "  Type command and press Enter\n");
    strcat(help_text, "  Commands with arguments can be typed without spaces (e.g., 'cw2', 'j5', 'tL')\n");
    strcat(help_text, "  Chain multiple commands with commas (e.g., 'tc,vm' or 'cw2,tc')\n");
    strcat(help_text, "  Direct tiling: 'tr4' (right 75%), 'tl2' (left 50%), 'tc1' (center 33%)");

    return help_text;
}

// Moves window to target workspace. Activates: yes (dispatcher focuses moved window).
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

// Pulls window from another workspace to current. Activates: yes (dispatcher focuses pulled window).
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

// Moves window to next monitor. Activates: yes (dispatcher focuses moved window).
gboolean cmd_toggle_monitor(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for monitor toggle");
        return FALSE;
    }

    move_window_to_next_monitor(app);
    return TRUE;
}

// Toggles _NET_WM_STATE_SKIP_TASKBAR. Activates: yes (dispatcher refocuses).
gboolean cmd_skip_taskbar(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for skip taskbar toggle");
        return FALSE;
    }

    toggle_window_state(app->display, window->id, "_NET_WM_STATE_SKIP_TASKBAR");
    return TRUE;
}

// Toggles _NET_WM_STATE_ABOVE. Activates: yes (dispatcher refocuses).
gboolean cmd_always_on_top(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for always on top toggle");
        return FALSE;
    }

    toggle_window_state(app->display, window->id, "_NET_WM_STATE_ABOVE");
    return TRUE;
}

// Toggles _NET_WM_STATE_BELOW. Activates: yes (dispatcher refocuses).
gboolean cmd_always_below(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for always below toggle");
        return FALSE;
    }

    toggle_window_state(app->display, window->id, "_NET_WM_STATE_BELOW");
    return TRUE;
}

// Toggles _NET_WM_STATE_STICKY. Activates: yes (dispatcher refocuses).
gboolean cmd_every_workspace(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for every workspace toggle");
        return FALSE;
    }

    toggle_window_state(app->display, window->id, "_NET_WM_STATE_STICKY");
    return TRUE;
}

// Sends close request to window. Activates: no (window is closing).
gboolean cmd_close_window(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for closing");
        return FALSE;
    }

    close_window(app->display, window->id);
    return TRUE;
}

// Toggles minimize/restore. Activates: no (handles activation directly for restore case, hides cofi).
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

// Toggles _NET_WM_STATE_MAXIMIZED_BOTH. Activates: yes (dispatcher refocuses).
gboolean cmd_maximize_window(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for maximizing");
        return FALSE;
    }

    toggle_window_state(app->display, window->id, "_NET_WM_STATE_MAXIMIZED_BOTH");
    return TRUE;
}

// Toggles _NET_WM_STATE_MAXIMIZED_HORZ. Activates: yes (dispatcher refocuses).
gboolean cmd_horizontal_maximize(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for horizontal maximizing");
        return FALSE;
    }

    toggle_window_state(app->display, window->id, "_NET_WM_STATE_MAXIMIZED_HORZ");
    return TRUE;
}

// Toggles _NET_WM_STATE_MAXIMIZED_VERT. Activates: yes (dispatcher refocuses).
gboolean cmd_vertical_maximize(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for vertical maximizing");
        return FALSE;
    }

    toggle_window_state(app->display, window->id, "_NET_WM_STATE_MAXIMIZED_VERT");
    return TRUE;
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

// Tiles window to position. Activates: yes (dispatcher focuses tiled window).
gboolean cmd_tile_window(AppData *app, WindowInfo *window, const char *args) {
    if (!window) {
        log_warn("No window selected for tiling");
        return FALSE;
    }

    if (strlen(args) > 0) {
        TileOption option = parse_tile_option(args);
        if (option == (TileOption)-1) {
            log_warn("Invalid tiling option: %s", args);
            return FALSE;
        }

        log_info("USER: Tiling window '%s' with option: %s", window->title, args);
        apply_tiling(app->display, window->id, option, app->config.tile_columns);
        return TRUE;
    }

    show_tiling_overlay(app);
    return FALSE;
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

gboolean cmd_help(AppData *app, WindowInfo *window __attribute__((unused)),
                  const char *args __attribute__((unused))) {
    show_help_commands(app);
    return FALSE;
}

gboolean cmd_mouse(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    if (!app || !app->display) {
        log_warn("Cannot control mouse: display not available");
        return FALSE;
    }

    Window root = DefaultRootWindow(app->display);
    if (!args || strlen(args) == 0) {
        log_warn("Mouse command requires an action: away, show, or hide");
        return FALSE;
    }

    while (*args == ' ') {
        args++;
    }

    if (strncmp(args, "away", 4) == 0 || strncmp(args, "a", 1) == 0) {
        XWarpPointer(app->display, None, root, 0, 0, 0, 0, 0, 0);
        XFlush(app->display);
        log_info("USER: Mouse moved to corner");
    } else if (strncmp(args, "show", 4) == 0 || strncmp(args, "s", 1) == 0) {
        XFixesShowCursor(app->display, root);
        XFlush(app->display);
        log_info("USER: Mouse cursor shown");
    } else if (strncmp(args, "hide", 4) == 0 || strncmp(args, "h", 1) == 0) {
        XFixesHideCursor(app->display, root);
        XFlush(app->display);
        log_info("USER: Mouse cursor hidden");
    } else {
        log_warn("Unknown mouse action: %s (use away/a, show/s, or hide/h)", args);
        return FALSE;
    }

    hide_window(app);
    return TRUE;
}

gboolean cmd_move_all_to_workspace(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    if (!app || !app->display) {
        log_warn("Cannot move windows: display not available");
        return FALSE;
    }

    int current_workspace = get_current_desktop(app->display);
    app->windows_to_move_count = 0;

    int normal_window_count = 0;
    for (int i = 0; i < app->window_count; i++) {
        WindowInfo *win = &app->windows[i];
        if (strcmp(win->type, "Special") == 0) {
            log_debug("Skipping special window: %s", win->title);
            continue;
        }

        if (win->desktop != current_workspace) {
            if (win->desktop == -1 || win->desktop == 0xFFFFFFFF) {
                log_debug("Skipping sticky window: %s", win->title);
            }
            continue;
        }

        if (get_window_state(app->display, win->id, "_NET_WM_STATE_STICKY")) {
            log_debug("Skipping sticky window (state check): %s", win->title);
            continue;
        }

        if (app->windows_to_move_count < MAX_WINDOWS) {
            app->windows_to_move[app->windows_to_move_count++] = win->id;
            normal_window_count++;
            log_debug("Will move window: %s (ID: 0x%lx)", win->title, win->id);
        }
    }

    if (normal_window_count == 0) {
        log_warn("No movable windows found on current workspace %d", current_workspace + 1);
        return FALSE;
    }

    log_info("USER: Found %d windows to move from workspace %d",
             normal_window_count, current_workspace + 1);

    if (strlen(args) > 0) {
        int target = resolve_workspace_from_arg(app->display, args, app->config.workspaces_per_row);
        if (target < 0) {
            log_warn("Invalid workspace target: %s", args);
            return FALSE;
        }

        for (int i = 0; i < app->windows_to_move_count; i++) {
            move_window_to_desktop(app->display, app->windows_to_move[i], target);
        }
        switch_to_desktop(app->display, target);
        log_info("USER: Moved %d windows from workspace %d to %d",
                 app->windows_to_move_count, current_workspace + 1, target + 1);
        return TRUE;
    }

    show_workspace_move_all_overlay(app);
    return FALSE;
}

// Swap window positions and sizes
gboolean cmd_swap_windows(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for swap");
        return FALSE;
    }

    if (app->filtered_count < 2) {
        log_warn("Need at least 2 windows to swap (have %d)", app->filtered_count);
        return FALSE;
    }

    WindowInfo *window1 = window;
    WindowInfo *window2 = NULL;
    for (int i = 0; i < app->filtered_count; i++) {
        if (app->filtered[i].id != window1->id) {
            window2 = &app->filtered[i];
            break;
        }
    }

    if (!window2) {
        log_warn("Could not find second window to swap with");
        return FALSE;
    }

    log_info("Swapping windows: '%s' <-> '%s'", window1->title, window2->title);

    int x1, y1, w1, h1;
    int x2, y2, w2, h2;

    if (!get_window_geometry(app->display, window1->id, &x1, &y1, &w1, &h1)) {
        log_error("Failed to get geometry for window 1");
        return FALSE;
    }

    if (!get_window_geometry(app->display, window2->id, &x2, &y2, &w2, &h2)) {
        log_error("Failed to get geometry for window 2");
        return FALSE;
    }

    log_debug("Window 1 geometry: %dx%d at (%d,%d)", w1, h1, x1, y1);
    log_debug("Window 2 geometry: %dx%d at (%d,%d)", w2, h2, x2, y2);

    gboolean win1_max_vert = get_window_state(app->display, window1->id, "_NET_WM_STATE_MAXIMIZED_VERT");
    gboolean win1_max_horz = get_window_state(app->display, window1->id, "_NET_WM_STATE_MAXIMIZED_HORZ");
    gboolean win2_max_vert = get_window_state(app->display, window2->id, "_NET_WM_STATE_MAXIMIZED_VERT");
    gboolean win2_max_horz = get_window_state(app->display, window2->id, "_NET_WM_STATE_MAXIMIZED_HORZ");

    log_debug("Window 1 max state: vert=%d, horz=%d", win1_max_vert, win1_max_horz);
    log_debug("Window 2 max state: vert=%d, horz=%d", win2_max_vert, win2_max_horz);

    Atom net_wm_state = XInternAtom(app->display, "_NET_WM_STATE", False);
    Atom net_wm_state_maximized_vert = XInternAtom(app->display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    Atom net_wm_state_maximized_horz = XInternAtom(app->display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);

    if (win1_max_vert || win1_max_horz) {
        XEvent event;
        memset(&event, 0, sizeof(event));
        event.type = ClientMessage;
        event.xclient.window = window1->id;
        event.xclient.message_type = net_wm_state;
        event.xclient.format = 32;
        event.xclient.data.l[0] = 0;
        event.xclient.data.l[1] = net_wm_state_maximized_vert;
        event.xclient.data.l[2] = net_wm_state_maximized_horz;

        XSendEvent(app->display, DefaultRootWindow(app->display), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &event);
    }

    if (win2_max_vert || win2_max_horz) {
        XEvent event;
        memset(&event, 0, sizeof(event));
        event.type = ClientMessage;
        event.xclient.window = window2->id;
        event.xclient.message_type = net_wm_state;
        event.xclient.format = 32;
        event.xclient.data.l[0] = 0;
        event.xclient.data.l[1] = net_wm_state_maximized_vert;
        event.xclient.data.l[2] = net_wm_state_maximized_horz;

        XSendEvent(app->display, DefaultRootWindow(app->display), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &event);
    }

    XFlush(app->display);
    usleep(100000);

    XMoveResizeWindow(app->display, window1->id, x2, y2, w2, h2);
    XMoveResizeWindow(app->display, window2->id, x1, y1, w1, h1);
    XFlush(app->display);
    usleep(50000);

    if (win2_max_vert || win2_max_horz) {
        XEvent event;
        memset(&event, 0, sizeof(event));
        event.type = ClientMessage;
        event.xclient.window = window1->id;
        event.xclient.message_type = net_wm_state;
        event.xclient.format = 32;
        event.xclient.data.l[0] = 1;

        if (win2_max_vert) {
            event.xclient.data.l[1] = net_wm_state_maximized_vert;
            event.xclient.data.l[2] = 0;
            XSendEvent(app->display, DefaultRootWindow(app->display), False,
                       SubstructureRedirectMask | SubstructureNotifyMask, &event);
        }

        if (win2_max_horz) {
            event.xclient.data.l[1] = net_wm_state_maximized_horz;
            event.xclient.data.l[2] = 0;
            XSendEvent(app->display, DefaultRootWindow(app->display), False,
                       SubstructureRedirectMask | SubstructureNotifyMask, &event);
        }
    }

    if (win1_max_vert || win1_max_horz) {
        XEvent event;
        memset(&event, 0, sizeof(event));
        event.type = ClientMessage;
        event.xclient.window = window2->id;
        event.xclient.message_type = net_wm_state;
        event.xclient.format = 32;
        event.xclient.data.l[0] = 1;

        if (win1_max_vert) {
            event.xclient.data.l[1] = net_wm_state_maximized_vert;
            event.xclient.data.l[2] = 0;
            XSendEvent(app->display, DefaultRootWindow(app->display), False,
                       SubstructureRedirectMask | SubstructureNotifyMask, &event);
        }

        if (win1_max_horz) {
            event.xclient.data.l[1] = net_wm_state_maximized_horz;
            event.xclient.data.l[2] = 0;
            XSendEvent(app->display, DefaultRootWindow(app->display), False,
                       SubstructureRedirectMask | SubstructureNotifyMask, &event);
        }
    }

    XFlush(app->display);
    usleep(50000);

    XMoveResizeWindow(app->display, window1->id, x2, y2, w2, h2);
    XMoveResizeWindow(app->display, window2->id, x1, y1, w1, h1);
    XFlush(app->display);

    log_info("Window swap completed");
    return TRUE;
}

static void show_error_in_display(AppData *app, const char *msg) {
    if (app->textbuffer) {
        gtk_text_buffer_set_text(app->textbuffer, msg, -1);
    }
    app->command_mode.showing_help = TRUE;
}

gboolean cmd_set_config(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    if (!args || args[0] == '\0') {
        show_error_in_display(app, "Usage: set <key> <value>\n\nType :config to see available keys.");
        return FALSE;
    }

    char key[64] = {0};
    const char *value = "";
    const char *sep = args;
    while (*sep && *sep != ' ' && *sep != '=') {
        sep++;
    }

    size_t key_len = (size_t)(sep - args);
    if (key_len >= sizeof(key)) {
        key_len = sizeof(key) - 1;
    }

    memcpy(key, args, key_len);
    while (*sep == ' ' || *sep == '=') {
        sep++;
    }
    value = sep;

    if (value[0] == '\0') {
        char msg[256];
        snprintf(msg, sizeof(msg), "Missing value for '%s'.\n\nType :config to see current values.", key);
        show_error_in_display(app, msg);
        return FALSE;
    }

    char err[256] = {0};
    if (apply_config_setting(&app->config, key, value, err, sizeof(err))) {
        save_config(&app->config);
        log_info("Config: %s = %s", key, value);
        exit_command_mode(app);
        switch_to_tab(app, TAB_CONFIG);
    } else {
        char msg[512];
        snprintf(msg, sizeof(msg), "Error: %s\n\nType :config to see available keys.", err);
        show_error_in_display(app, msg);
    }
    return FALSE;
}

gboolean cmd_show_config(AppData *app, WindowInfo *window __attribute__((unused)),
                         const char *args __attribute__((unused))) {
    exit_command_mode(app);
    switch_to_tab(app, TAB_CONFIG);
    return FALSE;
}

gboolean cmd_show(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    ShowMode mode = SHOW_MODE_WINDOWS;

    if (args && args[0] != '\0') {
        if (strcmp(args, "windows") == 0) mode = SHOW_MODE_WINDOWS;
        else if (strcmp(args, "command") == 0) mode = SHOW_MODE_COMMAND;
        else if (strcmp(args, "workspaces") == 0) mode = SHOW_MODE_WORKSPACES;
        else if (strcmp(args, "harpoon") == 0) mode = SHOW_MODE_HARPOON;
        else if (strcmp(args, "names") == 0) {
            exit_command_mode(app);
            switch_to_tab(app, TAB_NAMES);
            return FALSE;
        } else if (strcmp(args, "config") == 0) {
            exit_command_mode(app);
            switch_to_tab(app, TAB_CONFIG);
            return FALSE;
        } else {
            show_error_in_display(app, "Usage: show [windows|command|workspaces|harpoon|names|config]");
            return FALSE;
        }
    }

    exit_command_mode(app);
    dispatch_hotkey_mode(app, mode);
    return FALSE;
}

gboolean cmd_hotkeys(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    char key[64] = {0};
    char cmd[256] = {0};
    int action = parse_hotkey_command(args, key, sizeof(key), cmd, sizeof(cmd));

    if (action == 1) {
        add_hotkey_binding(&app->hotkey_config, key, cmd);
        save_hotkey_config(&app->hotkey_config);
        regrab_hotkeys(app);
        log_info("Hotkey bound: %s → %s", key, cmd);
    } else if (action == 2) {
        if (remove_hotkey_binding(&app->hotkey_config, key)) {
            save_hotkey_config(&app->hotkey_config);
            regrab_hotkeys(app);
            log_info("Hotkey unbound: %s", key);
        } else {
            log_warn("No hotkey binding for: %s", key);
        }
    }

    char buf[4096] = {0};
    format_hotkey_display(&app->hotkey_config, buf, sizeof(buf));
    gtk_text_buffer_set_text(app->textbuffer, buf, -1);
    app->command_mode.showing_help = TRUE;
    return FALSE;
}

gboolean cmd_assign_slots(AppData *app, WindowInfo *window __attribute__((unused)),
                          const char *args __attribute__((unused))) {
    assign_workspace_slots(app);
    hide_window(app);
    log_info("Assigned workspace slots via command mode");
    return TRUE;
}
