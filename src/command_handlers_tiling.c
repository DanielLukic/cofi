#include "command_handlers_tiling.h"

#include "app_data.h"
#include "log.h"
#include "overlay_manager.h"
#include "tiling.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <ctype.h>
#include <string.h>

extern void hide_window(AppData *app);

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

static const char *skip_spaces(const char *text) {
    while (text && *text == ' ') {
        text++;
    }
    return text;
}

static gboolean matches_mouse_action(const char *action, const char *word, char short_name) {
    return strncmp(action, word, strlen(word)) == 0 ||
           strncmp(action, &short_name, 1) == 0;
}

static gboolean perform_mouse_action(AppData *app, const char *action) {
    Window root = DefaultRootWindow(app->display);

    if (matches_mouse_action(action, "away", 'a')) {
        XWarpPointer(app->display, None, root, 0, 0, 0, 0, 0, 0);
        XFlush(app->display);
        log_info("USER: Mouse moved to corner");
        return TRUE;
    }

    if (matches_mouse_action(action, "show", 's')) {
        XFixesShowCursor(app->display, root);
        XFlush(app->display);
        log_info("USER: Mouse cursor shown");
        return TRUE;
    }

    if (matches_mouse_action(action, "hide", 'h')) {
        XFixesHideCursor(app->display, root);
        XFlush(app->display);
        log_info("USER: Mouse cursor hidden");
        return TRUE;
    }

    return FALSE;
}

gboolean cmd_mouse(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    if (!app || !app->display) {
        log_warn("Cannot control mouse: display not available");
        return FALSE;
    }

    const char *action = skip_spaces(args);
    if (!action || action[0] == '\0') {
        log_warn("Mouse command requires an action: away, show, or hide");
        return FALSE;
    }

    if (!perform_mouse_action(app, action)) {
        log_warn("Unknown mouse action: %s (use away/a, show/s, or hide/h)", action);
        return FALSE;
    }

    hide_window(app);
    return TRUE;
}
