#include "command_api.h"
#include "command_availability.h"
#include "cofi_tab_provider.h"
#include "command_parser.h"
#include "command_registry.h"

#ifndef COMMAND_POLICY_ONLY
#include "display.h"
#include "log.h"
#include "selection.h"
#include "x11_utils.h"
#endif

#include <string.h>

#ifndef COMMAND_POLICY_ONLY
typedef struct {
    AppData *app;
    WindowInfo *window;
    gboolean background;
} ExecuteContext;

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

    if (!command_primary_is_available(primary)) {
        log_warn("Unknown command: '%s'. Type 'help' for available commands.", primary);
        return FALSE;
    }

    const CommandSpec *cmd = cofi_command_by_primary(primary);
    CommandHandler handler = NULL;
    int activates = 0;

    if (cmd) {
        handler = cmd->handler;
        activates = cmd->activates;
    }

    if (!handler) {
        log_warn("Unknown command: '%s'. Type 'help' for available commands.", primary);
        return FALSE;
    }

    gboolean result = handler(app, window, arg);
    if (result && activates && !background) {
        activate_commanded_window(app, window);
    }

    return result;
}

static gboolean execute_segment_callback(const char *segment, void *user_data) {
    ExecuteContext *ctx = user_data;
    return execute_single_command(segment, ctx->app, ctx->window, ctx->background);
}

static gboolean execute_commands(const char *command, AppData *app,
                                 WindowInfo *window, gboolean background) {
    ExecuteContext ctx = {
        .app = app,
        .window = window,
        .background = background,
    };

    return visit_command_segments(command, execute_segment_callback, &ctx);
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
#endif

static gboolean command_keeps_open(const char *primary, const char *arg) {
    if (!primary || primary[0] == '\0') {
        return FALSE;
    }

    const CommandSpec *cmd = cofi_command_by_primary(primary);
    if (!cmd || !command_primary_is_available(primary)) {
        return FALSE;
    }

    if (cmd && cmd->keeps_open_on_hotkey_auto) {
        return TRUE;
    }

    return cmd && cmd->keeps_open_without_arg && (!arg || arg[0] == '\0');
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
    char primary[128] = {0};
    char arg[256] = {0};

    while (next_command_segment(&cursor, segment, sizeof(segment))) {
        if (!parse_command_for_execution(segment, primary, arg, sizeof(primary), sizeof(arg))) {
            return FALSE;
        }
        if (command_keeps_open(primary, arg)) {
            return TRUE;
        }
    }

    return FALSE;
}
