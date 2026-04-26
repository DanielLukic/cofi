#ifndef COMMAND_DEFINITIONS_H
#define COMMAND_DEFINITIONS_H

#include <glib.h>

#include "command_handlers_tiling.h"
#include "command_handlers_ui.h"
#include "command_handlers_window.h"
#include "command_handlers_workspace.h"

// Command handler function type
typedef gboolean (*CommandHandler)(AppData *app, WindowInfo *window, const char *args);

// Command structure
typedef struct {
    const char *primary;                    // Primary command name
    const char *aliases[5];                 // Up to 5 aliases (NULL-terminated)
    CommandHandler handler;                 // Handler function
    const char *description;                // Help description
    const char *help_format;                // Format for help display (e.g., "cw [N]")
    int activates;                          // Dispatcher activates target window after handler
    int keeps_open_on_hotkey_auto;          // Auto-! hotkeys keep cofi open after command
} CommandDef;

// Master command definitions - single source of truth
static const CommandDef COMMAND_DEFINITIONS[] = {
    {
        .primary = "ab",
        .aliases = {"always-below", NULL},
        .handler = cmd_always_below,
        .description = "Set always below for selected window (default: toggle)",
        .help_format = "ab, always-below [toggle|on|off]",
        .activates = 1
    },
    {
        .primary = "an",
        .aliases = {"assign-name", "n", NULL},
        .handler = cmd_assign_name,
        .description = "Assign custom name to selected window",
        .help_format = "an, assign-name, n",
        .keeps_open_on_hotkey_auto = 1
    },
    {
        .primary = "as",
        .aliases = {"assign-slots", NULL},
        .handler = cmd_assign_slots,
        .description = "Assign workspace window slots by screen position (1-9)",
        .help_format = "as, assign-slots"
    },
    {
        .primary = "aot",
        .aliases = {"at", "always-on-top", NULL},
        .handler = cmd_always_on_top,
        .description = "Set always on top for selected window (default: toggle)",
        .help_format = "at, always-on-top, aot [toggle|on|off]",
        .activates = 1
    },
    {
        .primary = "cl",
        .aliases = {"c", "close", "close-window", NULL},
        .handler = cmd_close_window,
        .description = "Close selected window",
        .help_format = "cl, close-window, c"
    },
    {
        .primary = "config",
        .aliases = {"conf", "cfg", NULL},
        .handler = cmd_show_config,
        .description = "Show current configuration",
        .help_format = "config, conf",
        .keeps_open_on_hotkey_auto = 1
    },
    {
        .primary = "cw",
        .aliases = {"change-workspace", NULL},
        .handler = cmd_change_workspace,
        .description = "Move selected window to different workspace (N = workspace number)",
        .help_format = "cw, change-workspace [N]",
        .activates = 1
    },
    {
        .primary = "ew",
        .aliases = {"every-workspace", NULL},
        .handler = cmd_every_workspace,
        .description = "Set show on every workspace for selected window (default: toggle)",
        .help_format = "ew, every-workspace [toggle|on|off]",
        .activates = 1
    },
    {
        .primary = "harpoon",
        .aliases = {"hp", NULL},
        .handler = cmd_harpoon,
        .description = "Switch to Harpoon tab",
        .help_format = "harpoon, hp",
        .keeps_open_on_hotkey_auto = 1
    },
    {
        .primary = "hmw",
        .aliases = {"hm", "horizontal-maximize-window", NULL},
        .handler = cmd_horizontal_maximize,
        .description = "Toggle horizontal maximize",
        .help_format = "hm, horizontal-maximize-window, hmw",
        .activates = 1
    },
    {
        .primary = "hotkeys",
        .aliases = {"hotkey", "hk", NULL},
        .handler = cmd_hotkeys,
        .description = "Manage system hotkey bindings",
        .help_format = "hotkeys [key] [command]",
        .keeps_open_on_hotkey_auto = 1
    },
    {
        .primary = "jw",
        .aliases = {"jump-workspace", "j", NULL},
        .handler = cmd_jump_workspace,
        .description = "Jump to different workspace (N = workspace number)",
        .help_format = "jw, jump-workspace, j [N]"
    },
    {
        .primary = "names",
        .aliases = {"nm", NULL},
        .handler = cmd_names,
        .description = "Switch to Names tab",
        .help_format = "names, nm",
        .keeps_open_on_hotkey_auto = 1
    },
    {
        .primary = "maw",
        .aliases = {"move-all-to-workspace", NULL},
        .handler = cmd_move_all_to_workspace,
        .description = "Move all windows from current workspace to target workspace",
        .help_format = "maw, move-all-to-workspace [N]"
    },
    {
        .primary = "miw",
        .aliases = {"min", "minimize-window", NULL},
        .handler = cmd_minimize_window,
        .description = "Toggle minimize selected window (restore if already minimized)",
        .help_format = "miw, min, minimize-window"
    },
    {
        .primary = "mouse",
        .aliases = {"m", "ma", "ms", "mh", NULL},
        .handler = cmd_mouse,
        .description = "Mouse control: away/show/hide",
        .help_format = "m, mouse [away|show|hide]"
    },
    {
        .primary = "mw",
        .aliases = {"max", "maximize-window", NULL},
        .handler = cmd_maximize_window,
        .description = "Toggle maximize selected window",
        .help_format = "mw, max, maximize-window",
        .activates = 1
    },
    {
        .primary = "pw",
        .aliases = {"pull-window", "p", NULL},
        .handler = cmd_pull_window,
        .description = "Pull selected window to current workspace",
        .help_format = "pw, pull-window, p",
        .activates = 1
    },
    {
        .primary = "rw",
        .aliases = {"rename-workspace", NULL},
        .handler = cmd_rename_workspace,
        .description = "Rename a workspace (N = workspace number, or current if omitted)",
        .help_format = "rw, rename-workspace [N]",
        .keeps_open_on_hotkey_auto = 1
    },
    {
        .primary = "show",
        .aliases = {"s", NULL},
        .handler = cmd_show,
        .description = "Show cofi in a specific mode (windows/command/run/workspaces/harpoon/names)",
        .help_format = "show [windows|command|run|workspaces|harpoon|names]",
        .keeps_open_on_hotkey_auto = 1
    },
    {
        .primary = "set",
        .aliases = {NULL},
        .handler = cmd_set_config,
        .description = "Set config option: set <key> <value>",
        .help_format = "set <key> <value>",
        .keeps_open_on_hotkey_auto = 1
    },
    {
        .primary = "sb",
        .aliases = {"skip-taskbar", NULL},
        .handler = cmd_skip_taskbar,
        .description = "Set skip taskbar for selected window (default: toggle)",
        .help_format = "sb, skip-taskbar [toggle|on|off]",
        .activates = 1
    },
    {
        .primary = "sw",
        .aliases = {"swap-windows", NULL},
        .handler = cmd_swap_windows,
        .description = "Swap position and size of selected window with first in list",
        .help_format = "sw, swap-windows"
    },
    {
        .primary = "tm",
        .aliases = {"toggle-monitor", NULL},
        .handler = cmd_toggle_monitor,
        .description = "Move selected window to next monitor",
        .help_format = "tm, toggle-monitor",
        .activates = 1
    },
    {
        .primary = "tw",
        .aliases = {"tile-window", "t", NULL},
        .handler = cmd_tile_window,
        .description = "Tile window (L/R/T/B/C, 1-9, F, or [lrtbc][1-4] for sizes)",
        .help_format = "tw, tile-window, t [OPT]",
        .activates = 1
    },
    {
        .primary = "vmw",
        .aliases = {"vm", "vertical-maximize-window", NULL},
        .handler = cmd_vertical_maximize,
        .description = "Toggle vertical maximize",
        .help_format = "vm, vertical-maximize-window, vmw",
        .activates = 1
    },
    {
        .primary = "workspaces",
        .aliases = {"ws", NULL},
        .handler = cmd_workspaces,
        .description = "Switch to Workspaces tab",
        .help_format = "workspaces, ws",
        .keeps_open_on_hotkey_auto = 1
    },
    {
        .primary = "help",
        .aliases = {"h", "?", NULL},
        .handler = cmd_help,
        .description = "Show this help message",
        .help_format = "help, h, ?",
        .keeps_open_on_hotkey_auto = 1
    },
    {
        .primary = NULL,
        .aliases = {NULL},
        .handler = NULL,
        .description = NULL,
        .help_format = NULL,
        .activates = 0,
        .keeps_open_on_hotkey_auto = 0
    } // Sentinel
};

#endif // COMMAND_DEFINITIONS_H
