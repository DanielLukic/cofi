#ifndef COMMAND_DEFINITIONS_H
#define COMMAND_DEFINITIONS_H

// Forward declarations
typedef struct AppData AppData;
typedef struct WindowInfo WindowInfo;

// Command handler function type
typedef gboolean (*CommandHandler)(AppData *app, WindowInfo *window, const char *args);

// Command structure
typedef struct {
    const char *primary;                    // Primary command name
    const char *aliases[5];                 // Up to 5 aliases (NULL-terminated)
    CommandHandler handler;                 // Handler function
    const char *description;                // Help description
    const char *help_format;                // Format for help display (e.g., "cw [N]")
} CommandDef;

// Command handler declarations
gboolean cmd_change_workspace(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_pull_window(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_toggle_monitor(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_skip_taskbar(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_always_on_top(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_always_below(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_every_workspace(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_close_window(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_maximize_window(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_horizontal_maximize(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_vertical_maximize(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_jump_workspace(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_rename_workspace(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_tile_window(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_assign_name(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_help(AppData *app, WindowInfo *window, const char *args);
gboolean cmd_mouse(AppData *app, WindowInfo *window, const char *args);

// Master command definitions - single source of truth
static const CommandDef COMMAND_DEFINITIONS[] = {
    {
        .primary = "ab",
        .aliases = {"always-below", NULL},
        .handler = cmd_always_below,
        .description = "Toggle always below for selected window",
        .help_format = "ab, always-below"
    },
    {
        .primary = "an",
        .aliases = {"assign-name", "n", NULL},
        .handler = cmd_assign_name,
        .description = "Assign custom name to selected window",
        .help_format = "an, assign-name, n"
    },
    {
        .primary = "aot",
        .aliases = {"at", "always-on-top", NULL},
        .handler = cmd_always_on_top,
        .description = "Toggle always on top for selected window",
        .help_format = "at, always-on-top, aot"
    },
    {
        .primary = "cl",
        .aliases = {"c", "close", "close-window", NULL},
        .handler = cmd_close_window,
        .description = "Close selected window",
        .help_format = "cl, close-window, c"
    },
    {
        .primary = "cw",
        .aliases = {"change-workspace", NULL},
        .handler = cmd_change_workspace,
        .description = "Move selected window to different workspace (N = workspace number)",
        .help_format = "cw, change-workspace [N]"
    },
    {
        .primary = "ew",
        .aliases = {"every-workspace", NULL},
        .handler = cmd_every_workspace,
        .description = "Toggle show on every workspace for selected window",
        .help_format = "ew, every-workspace"
    },
    {
        .primary = "hmw",
        .aliases = {"hm", "horizontal-maximize-window", NULL},
        .handler = cmd_horizontal_maximize,
        .description = "Toggle horizontal maximize",
        .help_format = "hm, horizontal-maximize-window, hmw"
    },
    {
        .primary = "jw",
        .aliases = {"jump-workspace", "j", NULL},
        .handler = cmd_jump_workspace,
        .description = "Jump to different workspace (N = workspace number)",
        .help_format = "jw, jump-workspace, j [N]"
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
        .help_format = "mw, max, maximize-window"
    },
    {
        .primary = "pw",
        .aliases = {"pull-window", "p", NULL},
        .handler = cmd_pull_window,
        .description = "Pull selected window to current workspace",
        .help_format = "pw, pull-window, p"
    },
    {
        .primary = "rw",
        .aliases = {"rename-workspace", NULL},
        .handler = cmd_rename_workspace,
        .description = "Rename a workspace (N = workspace number, or current if omitted)",
        .help_format = "rw, rename-workspace [N]"
    },
    {
        .primary = "sb",
        .aliases = {"skip-taskbar", NULL},
        .handler = cmd_skip_taskbar,
        .description = "Toggle skip taskbar for selected window",
        .help_format = "sb, skip-taskbar"
    },
    {
        .primary = "tm",
        .aliases = {"toggle-monitor", NULL},
        .handler = cmd_toggle_monitor,
        .description = "Move selected window to next monitor",
        .help_format = "tm, toggle-monitor"
    },
    {
        .primary = "tw",
        .aliases = {"tile-window", "t", NULL},
        .handler = cmd_tile_window,
        .description = "Tile window (L/R/T/B/C, 1-9, F, or [lrtbc][1-4] for sizes)",
        .help_format = "tw, tile-window, t [OPT]"
    },
    {
        .primary = "vmw",
        .aliases = {"vm", "vertical-maximize-window", NULL},
        .handler = cmd_vertical_maximize,
        .description = "Toggle vertical maximize",
        .help_format = "vm, vertical-maximize-window, vmw"
    },
    {
        .primary = "help",
        .aliases = {"h", "?", NULL},
        .handler = cmd_help,
        .description = "Show this help message",
        .help_format = "help, h, ?"
    },
    {NULL, {NULL}, NULL, NULL, NULL} // Sentinel
};

#endif // COMMAND_DEFINITIONS_H