#include "../src/command_api.h"

#define STUB_COMMAND_HANDLER(name) \
    gboolean name(AppData *app, WindowInfo *window, const char *args) { \
        (void)app; \
        (void)window; \
        (void)args; \
        return FALSE; \
    }

STUB_COMMAND_HANDLER(cmd_always_below)
STUB_COMMAND_HANDLER(cmd_assign_name)
STUB_COMMAND_HANDLER(cmd_rename_window)
STUB_COMMAND_HANDLER(cmd_always_on_top)
STUB_COMMAND_HANDLER(cmd_close_window)
STUB_COMMAND_HANDLER(cmd_change_workspace)
STUB_COMMAND_HANDLER(cmd_every_workspace)
STUB_COMMAND_HANDLER(cmd_harpoon_set)
STUB_COMMAND_HANDLER(cmd_horizontal_maximize)
STUB_COMMAND_HANDLER(cmd_jump_workspace)
STUB_COMMAND_HANDLER(cmd_jump_slot)
STUB_COMMAND_HANDLER(cmd_save_layout)
STUB_COMMAND_HANDLER(cmd_restore_layout)
STUB_COMMAND_HANDLER(cmd_clear_layout)
STUB_COMMAND_HANDLER(cmd_move_all_to_workspace)
STUB_COMMAND_HANDLER(cmd_minimize_window)
STUB_COMMAND_HANDLER(cmd_mouse)
STUB_COMMAND_HANDLER(cmd_maximize_window)
STUB_COMMAND_HANDLER(cmd_pull_window)
STUB_COMMAND_HANDLER(cmd_rename_workspace)
STUB_COMMAND_HANDLER(cmd_skip_taskbar)
STUB_COMMAND_HANDLER(cmd_swap_windows)
STUB_COMMAND_HANDLER(cmd_toggle_monitor)
STUB_COMMAND_HANDLER(cmd_tile_window)
STUB_COMMAND_HANDLER(cmd_vertical_maximize)

#ifndef COMMAND_STUBS_EXCLUDE_UI
STUB_COMMAND_HANDLER(cmd_show)
STUB_COMMAND_HANDLER(cmd_set_config)
STUB_COMMAND_HANDLER(cmd_help)
#endif

STUB_COMMAND_HANDLER(cmd_run)
STUB_COMMAND_HANDLER(cmd_calc)
