#include "commands/core_commands.h"

#include "commands/command_handlers_tiling.h"
#include "commands/command_handlers_ui.h"
#include "commands/command_handlers_window.h"
#include "commands/command_handlers_workspace.h"

static const CommandSpec s_core_commands[] = {
    {
        .primary = "ab",
        .aliases = {"always-below", NULL},
        .compact_suffix = "+-",
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_always_below,
        .description = "Set always below for selected window (default: toggle)",
        .help_format = "ab, always-below [toggle|on|off]",
        .activates = 1
    },
    {
        .primary = "an",
        .aliases = {"assign-name", "n", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_assign_name,
        .description = "Assign custom name to selected window",
        .help_format = "an, assign-name, n",
        .keeps_open_on_hotkey_auto = 1
    },
    {
        .primary = "aot",
        .aliases = {"at", "always-on-top", NULL},
        .compact_suffix = "+-",
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_always_on_top,
        .description = "Set always on top for selected window (default: toggle)",
        .help_format = "at, always-on-top, aot [toggle|on|off]",
        .activates = 1
    },
    {
        .primary = "cl",
        .aliases = {"c", "close", "close-window", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_close_window,
        .description = "Close selected window",
        .help_format = "cl, close-window, c"
    },
    {
        .primary = "cw",
        .aliases = {"change-workspace", NULL},
        .compact_suffix = "0123456789hjkl",
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_change_workspace,
        .description = "Move selected window to different workspace (N = workspace number)",
        .help_format = "cw, change-workspace [N]",
        .activates = 1,
        .keeps_open_without_arg = 1
    },
    {
        .primary = "ew",
        .aliases = {"every-workspace", NULL},
        .compact_suffix = "+-",
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_every_workspace,
        .description = "Set show on every workspace for selected window (default: toggle)",
        .help_format = "ew, every-workspace [toggle|on|off]",
        .activates = 1
    },
    {
        .primary = "hs",
        .aliases = {"harpoon-set", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_harpoon_set,
        .description = "Assign selected window to harpoon slot (0-9, a-z)",
        .help_format = "hs <key>, harpoon-set <key>"
    },
    {
        .primary = "hmw",
        .aliases = {"hm", "horizontal-maximize-window", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_horizontal_maximize,
        .description = "Toggle horizontal maximize",
        .help_format = "hm, horizontal-maximize-window, hmw",
        .activates = 1
    },
    {
        .primary = "jw",
        .aliases = {"jump-workspace", "j", NULL},
        .compact_suffix = "0123456789hjkl",
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_jump_workspace,
        .description = "Jump to different workspace (N = workspace number)",
        .help_format = "jw, jump-workspace, j [N]",
        .keeps_open_without_arg = 1
    },
    {
        .primary = "jump-slot",
        .aliases = {"js", NULL},
        .compact_suffix = "123456789",
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_jump_slot,
        .description = "Jump to the Nth window on the current workspace by screen position (1-9)",
        .help_format = "js, jump-slot N"
    },
    {
        .primary = "save-layout",
        .aliases = {"sl", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_save_layout,
        .description = "Save the selected window's position, size and workspace",
        .help_format = "save-layout, sl",
        .activates = 1
    },
    {
        .primary = "restore-layout",
        .aliases = {"rl", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_restore_layout,
        .description = "Restore the saved position, size and workspace",
        .help_format = "restore-layout, rl",
        .activates = 1
    },
    {
        .primary = "delete-layout",
        .aliases = {"dl", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_clear_layout,
        .description = "Forget the saved layout for the selected window",
        .help_format = "delete-layout, dl",
        .activates = 1
    },
    {
        .primary = "maw",
        .aliases = {"move-all-to-workspace", NULL},
        .compact_suffix = "0123456789hjkl",
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_move_all_to_workspace,
        .description = "Move all windows from current workspace to target workspace",
        .help_format = "maw, move-all-to-workspace [N]",
        .keeps_open_without_arg = 1
    },
    {
        .primary = "miw",
        .aliases = {"min", "minimize-window", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_minimize_window,
        .description = "Toggle minimize selected window (restore if already minimized)",
        .help_format = "miw, min, minimize-window"
    },
    {
        .primary = "mouse",
        .aliases = {"ma", "ms", "mh", NULL},
        .compact_suffix = "ash",
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_mouse,
        .description = "Mouse control: away/show/hide",
        .help_format = "mouse, ma/ms/mh [away|show|hide]"
    },
    {
        .primary = "mw",
        .aliases = {"max", "maximize-window", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_maximize_window,
        .description = "Toggle maximize selected window",
        .help_format = "mw, max, maximize-window",
        .activates = 1
    },
    {
        .primary = "pw",
        .aliases = {"pull-window", "p", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_pull_window,
        .description = "Pull selected window to current workspace",
        .help_format = "pw, pull-window, p",
        .activates = 1
    },
    {
        .primary = "rename",
        .aliases = {"rn", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_rename_window,
        .description = "Force the selected window's title (sets _NET_WM_NAME/WM_NAME)",
        .help_format = "rename, rn <title>"
    },
    {
        .primary = "rw",
        .aliases = {"rename-workspace", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_rename_workspace,
        .description = "Rename a workspace (N = workspace number, or current if omitted)",
        .help_format = "rw, rename-workspace [N]",
        .keeps_open_on_hotkey_auto = 1
    },
    {
        .primary = "show",
        .aliases = {"s", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_show,
        .description = "Show cofi in a specific mode (windows/command/run/workspaces/harpoon/matching/config/rules/apps/emoji/projects/calc/proc/sinks/sessions/profiles)",
        .help_format = "show [MODE]",
        .keeps_open_on_hotkey_auto = 1
    },
    {
        .primary = "set",
        .aliases = {NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_set_config,
        .description = "Set config option: set <key> <value>",
        .help_format = "set <key> <value>",
        .keeps_open_on_hotkey_auto = 1
    },
    {
        .primary = "sb",
        .aliases = {"skip-taskbar", NULL},
        .compact_suffix = "+-",
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_skip_taskbar,
        .description = "Set skip taskbar for selected window (default: toggle)",
        .help_format = "sb, skip-taskbar [toggle|on|off]",
        .activates = 1
    },
    {
        .primary = "sw",
        .aliases = {"swap-windows", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_swap_windows,
        .description = "Swap position and size of selected window with first in list",
        .help_format = "sw, swap-windows"
    },
    {
        .primary = "tm",
        .aliases = {"toggle-monitor", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_toggle_monitor,
        .description = "Move selected window to next monitor",
        .help_format = "tm, toggle-monitor",
        .activates = 1
    },
    {
        .primary = "tw",
        .aliases = {"tile-window", "t", NULL},
        .compact_suffix = "0123456789LRTBFClrtbfc",
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_tile_window,
        .description = "Tile window (L/R/T/B/C, 1-9, F, or [lrtbc][1-4] for sizes)",
        .help_format = "tw, tile-window, t [OPT]",
        .activates = 1,
        .keeps_open_without_arg = 1
    },
    {
        .primary = "vmw",
        .aliases = {"vm", "vertical-maximize-window", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_vertical_maximize,
        .description = "Toggle vertical maximize",
        .help_format = "vm, vertical-maximize-window, vmw",
        .activates = 1
    },
    {
        .primary = "help",
        .aliases = {"h", "?", NULL},
        .owner_provider_id = COMMAND_OWNER_CORE,
        .handler = cmd_help,
        .description = "Show this help message",
        .help_format = "help, h, ?",
        .keeps_open_on_hotkey_auto = 1
    },
};

void cofi_register_core_commands(void) {
    int count = (int)(sizeof(s_core_commands) / sizeof(s_core_commands[0]));
    for (int i = 0; i < count; i++) {
        cofi_register_command(&s_core_commands[i]);
    }
}
