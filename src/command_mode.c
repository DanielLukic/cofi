#include "command_mode.h"
#include "command_definitions.h"
#include "log.h"
#include "tiling.h"
#include "overlay_manager.h"
#include "monitor_move.h"
#include "display.h"
#include "selection.h"
#include "x11_utils.h"
#include "app_data.h"
#include <X11/extensions/Xfixes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// External function from main.c
extern void hide_window(AppData *app);

// Global command history that persists across window recreations
static struct {
    char history[10][256];
    int history_count;
    gboolean initialized;
} g_command_history = { .initialized = FALSE };

// Helper function to log commanded window
static void log_commanded_window(AppData *app, WindowInfo *win) {
    if (!app || !win) return;
    
    // Truncate title to 15 chars max
    char truncated_title[16];
    strncpy(truncated_title, win->title, 15);
    truncated_title[15] = '\0';
    
    log_info("CMD: Window commanded - ID: 0x%lx, Class: %s, Title: %s", 
             win->id, win->class_name, truncated_title);
}

// Activate a window that was modified by a command mode action
static void activate_commanded_window(AppData *app, WindowInfo *win) {
    if (!app || !win) return;
    
    activate_window(win->id);
    log_commanded_window(app, win);
}


// Add command to global history
static void add_to_history(CommandMode *cmd, const char *command) {
    if (!command || strlen(command) == 0) return;

    // Initialize global history if needed
    if (!g_command_history.initialized) {
        g_command_history.history_count = 0;
        for (int i = 0; i < 10; i++) {
            g_command_history.history[i][0] = '\0';
        }
        g_command_history.initialized = TRUE;
        log_debug("Initialized global command history");
    }

    // Don't add if it's the same as the last command
    if (g_command_history.history_count > 0 && strcmp(g_command_history.history[0], command) == 0) {
        return;
    }

    // Shift existing history down
    for (int i = 9; i > 0; i--) {
        strcpy(g_command_history.history[i], g_command_history.history[i-1]);
    }

    // Add new command at the front
    strncpy(g_command_history.history[0], command, 255);
    g_command_history.history[0][255] = '\0';

    if (g_command_history.history_count < 10) {
        g_command_history.history_count++;
    }

    // Sync to local command mode
    if (cmd) {
        cmd->history_count = g_command_history.history_count;
        for (int i = 0; i < g_command_history.history_count; i++) {
            strcpy(cmd->history[i], g_command_history.history[i]);
        }
    }

    log_debug("Added command to global history: '%s' (total: %d)", command, g_command_history.history_count);
}

// Clear command line
static void clear_command_line(AppData *app) {
    if (!app || !app->entry) return;
    
    gtk_entry_set_text(GTK_ENTRY(app->entry), "");
    app->command_mode.history_index = -1; // Reset history browsing
}

void init_command_mode(CommandMode *cmd) {
    if (!cmd) return;

    cmd->state = CMD_MODE_NORMAL;
    cmd->command_buffer[0] = '\0';
    cmd->cursor_pos = 0;
    cmd->showing_help = FALSE;
    cmd->history_index = -1;
    cmd->close_on_exit = FALSE;

    // Initialize global history if needed
    if (!g_command_history.initialized) {
        g_command_history.history_count = 0;
        for (int i = 0; i < 10; i++) {
            g_command_history.history[i][0] = '\0';
        }
        g_command_history.initialized = TRUE;
        log_debug("Initialized global command history");
    }

    // Restore history from global store
    cmd->history_count = g_command_history.history_count;
    for (int i = 0; i < g_command_history.history_count; i++) {
        strcpy(cmd->history[i], g_command_history.history[i]);
    }

    // Clear any remaining slots
    for (int i = g_command_history.history_count; i < 10; i++) {
        cmd->history[i][0] = '\0';
    }

    log_debug("Command mode initialized with %d history entries restored", cmd->history_count);
}


void enter_command_mode(AppData *app) {
    if (!app || !app->entry) return;
    
    app->command_mode.state = CMD_MODE_COMMAND;
    app->command_mode.command_buffer[0] = '\0';
    app->command_mode.cursor_pos = 0;
    
    // Reset selection to 0 only if user hasn't navigated from alt-tab default (index 1)
    if (app->current_tab == TAB_WINDOWS && app->filtered_count > 0 && 
        app->selection.window_index == 1) {
        app->selection.window_index = 0;
        app->selection.selected_window_id = app->filtered[0].id;
        update_display(app); // Update visual display to show correct selection
        log_debug("Command mode: reset selection from alt-tab default (index 1) to index 0");
    }
    
    // Change mode indicator to ":"
    if (app->mode_indicator) {
        gtk_label_set_text(GTK_LABEL(app->mode_indicator), ":");
    }
    
    // Clear the entry field
    gtk_entry_set_text(GTK_ENTRY(app->entry), "");
    
    log_info("USER: Entered command mode");
}

void exit_command_mode(AppData *app) {
    if (!app || !app->entry) return;
    
    // Check if we should close the window when exiting command mode
    gboolean should_close = app->command_mode.close_on_exit;
    
    app->command_mode.state = CMD_MODE_NORMAL;
    app->command_mode.command_buffer[0] = '\0';
    app->command_mode.cursor_pos = 0;
    app->command_mode.showing_help = FALSE;
    app->command_mode.history_index = -1;
    app->command_mode.close_on_exit = FALSE; // Reset flag
    
    // If we should close, close the window
    if (should_close) {
        log_info("USER: Exited command mode (started with --command, closing window)");
        hide_window(app);
        return;
    }
    
    // Otherwise, return to normal search mode
    // Change mode indicator back to ">"
    if (app->mode_indicator) {
        gtk_label_set_text(GTK_LABEL(app->mode_indicator), ">");
    }
    
    gtk_entry_set_text(GTK_ENTRY(app->entry), "");
    update_display(app);
    log_info("USER: Exited command mode");
}

gboolean handle_command_key(GdkEventKey *event, AppData *app) {
    if (!app || app->command_mode.state != CMD_MODE_COMMAND) {
        return FALSE;
    }
    
    // If help is being shown, any key press should dismiss it first
    if (app->command_mode.showing_help) {
        app->command_mode.showing_help = FALSE;
        
        // If they pressed Escape, exit command mode entirely
        if (event->keyval == GDK_KEY_Escape) {
            exit_command_mode(app);
            return TRUE;
        }
        
        // For any other key, just dismiss help and return to command mode
        update_display(app);
        // Let the key be processed normally below
    }
    
    switch (event->keyval) {
        case GDK_KEY_Escape:
            exit_command_mode(app);
            return TRUE;
            
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter: {
            // Get command from entry widget (no prefix anymore)
            const char *command = gtk_entry_get_text(GTK_ENTRY(app->entry));
            
            // Add to history if not empty
            if (command && strlen(command) > 0) {
                add_to_history(&app->command_mode, command);
            }
            
            // Execute the command
            gboolean should_exit = execute_command(command, app);
            
            if (should_exit) {
                exit_command_mode(app);
            } else {
                // Stay in command mode but clear to just ':'
                clear_command_line(app);
            }
            return TRUE;
        }
        
        
        case GDK_KEY_u:
            if (event->state & GDK_CONTROL_MASK) {
                // Ctrl+U: Clear line
                clear_command_line(app);
                return TRUE;
            }
            return FALSE;

        case GDK_KEY_j:
            if (event->state & GDK_CONTROL_MASK) {
                // Ctrl+J: Move selection down
                move_selection_down(app);
                return TRUE;
            }
            return FALSE;

        case GDK_KEY_k:
            if (event->state & GDK_CONTROL_MASK) {
                // Ctrl+K: Move selection up
                move_selection_up(app);
                return TRUE;
            }
            return FALSE;
            
        case GDK_KEY_Up:
            // Browse command history backwards
            if (app->command_mode.history_count > 0) {
                if (app->command_mode.history_index == -1) {
                    // First time browsing history
                    app->command_mode.history_index = 0;
                } else if (app->command_mode.history_index < app->command_mode.history_count - 1) {
                    app->command_mode.history_index++;
                }

                // Set the entry text to the historical command
                gtk_entry_set_text(GTK_ENTRY(app->entry), 
                        app->command_mode.history[app->command_mode.history_index]);
                gtk_editable_set_position(GTK_EDITABLE(app->entry), -1); // Move cursor to end
                log_debug("History backward: index=%d, command='%s'",
                          app->command_mode.history_index,
                          app->command_mode.history[app->command_mode.history_index]);
            }
            return TRUE;

        case GDK_KEY_Down:
            // Browse command history forwards
            if (app->command_mode.history_index > 0) {
                app->command_mode.history_index--;

                // Set the entry text to the historical command
                gtk_entry_set_text(GTK_ENTRY(app->entry), 
                        app->command_mode.history[app->command_mode.history_index]);
                gtk_editable_set_position(GTK_EDITABLE(app->entry), -1); // Move cursor to end
                log_debug("History forward: index=%d, command='%s'",
                          app->command_mode.history_index,
                          app->command_mode.history[app->command_mode.history_index]);
            } else if (app->command_mode.history_index == 0) {
                // Go back to empty command line
                app->command_mode.history_index = -1;
                clear_command_line(app);
                log_debug("History forward: cleared to empty command line");
            }
            return TRUE;
            
        case GDK_KEY_colon:
            // Ignore ':' in command mode - it should not be typed
            return TRUE;
            
        default:
            // Let the entry widget handle all other keys naturally
            return FALSE; // Let GTK handle the key event
    }
}

// Helper function to parse commands that might not have spaces between command and argument
// Returns TRUE if successfully parsed, FALSE otherwise
static gboolean parse_command_and_arg(const char *input, char *cmd_out, char *arg_out, size_t cmd_size, size_t arg_size) {
    if (!input || !cmd_out || !arg_out) return FALSE;
    
    // Clear output buffers
    cmd_out[0] = '\0';
    arg_out[0] = '\0';
    
    // Skip leading whitespace
    while (*input == ' ' || *input == '\t') input++;
    
    // Try to parse with space first (backward compatibility)
    if (sscanf(input, "%31s %31s", cmd_out, arg_out) == 2) {
        return TRUE;
    }
    
    // If no space found, try to parse known commands without spaces
    size_t len = strlen(input);
    
    // Check for 'cw' followed by number (change-workspace)
    if (len >= 3 && strncmp(input, "cw", 2) == 0 && isdigit(input[2])) {
        strncpy(cmd_out, "cw", cmd_size - 1);
        cmd_out[cmd_size - 1] = '\0';
        strncpy(arg_out, input + 2, arg_size - 1);
        arg_out[arg_size - 1] = '\0';
        return TRUE;
    }
    
    // Check for 'jw' followed by number (jump-workspace)
    if (len >= 3 && strncmp(input, "jw", 2) == 0 && isdigit(input[2])) {
        strncpy(cmd_out, "jw", cmd_size - 1);
        cmd_out[cmd_size - 1] = '\0';
        strncpy(arg_out, input + 2, arg_size - 1);
        arg_out[arg_size - 1] = '\0';
        return TRUE;
    }
    
    // Check for 'j' followed by number (jump shortcut)
    if (len >= 2 && input[0] == 'j' && isdigit(input[1])) {
        strncpy(cmd_out, "j", cmd_size - 1);
        cmd_out[cmd_size - 1] = '\0';
        strncpy(arg_out, input + 1, arg_size - 1);
        arg_out[arg_size - 1] = '\0';
        return TRUE;
    }
    
    // Check for 'tw' followed by tiling option
    if (len >= 3 && strncmp(input, "tw", 2) == 0 && 
        (isdigit(input[2]) || strchr("LRTBFClrtbfc", input[2]))) {
        strncpy(cmd_out, "tw", cmd_size - 1);
        cmd_out[cmd_size - 1] = '\0';
        strncpy(arg_out, input + 2, arg_size - 1);
        arg_out[arg_size - 1] = '\0';
        return TRUE;
    }
    
    // Check for 't' followed by tiling option
    if (len >= 2 && input[0] == 't' && 
        (isdigit(input[1]) || strchr("LRTBFClrtbfc", input[1]))) {
        strncpy(cmd_out, "t", cmd_size - 1);
        cmd_out[cmd_size - 1] = '\0';
        strncpy(arg_out, input + 1, arg_size - 1);
        arg_out[arg_size - 1] = '\0';
        return TRUE;
    }
    
    // Check for 'm' followed by mouse action (ma, ms, mh)
    if (len == 2 && input[0] == 'm' && strchr("ash", input[1])) {
        strncpy(cmd_out, "m", cmd_size - 1);
        cmd_out[cmd_size - 1] = '\0';
        strncpy(arg_out, input + 1, arg_size - 1);
        arg_out[arg_size - 1] = '\0';
        return TRUE;
    }
    
    // Check for direct tiling commands like 'tr4' (tile right 75%) or 'tc4' (center 100%)
    if (len >= 3 && input[0] == 't' && strchr("lrtbcLRTBC", input[1]) && strchr("1234", input[2])) {
        strncpy(cmd_out, "t", cmd_size - 1);
        cmd_out[cmd_size - 1] = '\0';
        strncpy(arg_out, input + 1, arg_size - 1);
        arg_out[arg_size - 1] = '\0';
        return TRUE;
    }
    
    // No argument found, just copy the command
    strncpy(cmd_out, input, cmd_size - 1);
    cmd_out[cmd_size - 1] = '\0';
    return TRUE;
}

// Helper functions
static const CommandDef* find_command(const char *cmd_name);
static TileOption parse_tile_option(const char *arg);

// Find command in dispatch table
static const CommandDef* find_command(const char *cmd_name) {
    for (int i = 0; COMMAND_DEFINITIONS[i].primary != NULL; i++) {
        // Check primary name
        if (strcmp(cmd_name, COMMAND_DEFINITIONS[i].primary) == 0) {
            return &COMMAND_DEFINITIONS[i];
        }
        
        // Check aliases
        for (int j = 0; j < 5 && COMMAND_DEFINITIONS[i].aliases[j] != NULL; j++) {
            if (strcmp(cmd_name, COMMAND_DEFINITIONS[i].aliases[j]) == 0) {
                return &COMMAND_DEFINITIONS[i];
            }
        }
    }
    return NULL;
}

// Parse tiling option from string
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
    return (TileOption)-1; // Invalid option
}


gboolean execute_command(const char *command, AppData *app) {
    if (!command || !app) return FALSE;
    
    log_info("USER: Executing command: '%s'", command);
    
    // Trim leading/trailing whitespace
    while (*command == ' ' || *command == '\t') command++;
    
    if (strlen(command) == 0) {
        return TRUE; // Empty command, just exit
    }
    
    // Parse command and optional argument
    char cmd_name[32] = {0};
    char arg[32] = {0};
    parse_command_and_arg(command, cmd_name, arg, sizeof(cmd_name), sizeof(arg));
    
    // Find and execute command using dispatch table
    const CommandDef *cmd = find_command(cmd_name);
    if (cmd) {
        WindowInfo *selected_window = get_selected_window(app);
        return cmd->handler(app, selected_window, arg);
    } else {
        log_warn("Unknown command: '%s'. Type 'help' for available commands.", cmd_name);
        return FALSE; // Stay in command mode
    }
}

// Command handler implementations
gboolean cmd_change_workspace(AppData *app, WindowInfo *window, const char *args) {
    if (!window) {
        log_warn("No window selected for workspace change");
        return FALSE;
    }
    
    if (strlen(args) > 0) {
        int workspace_num = atoi(args);
        if (workspace_num >= 1 && workspace_num <= 36) {
            int workspace_count = get_number_of_desktops(app->display);
            if (workspace_num <= workspace_count) {
                int target_workspace = workspace_num - 1;
                log_info("USER: Moving window '%s' to workspace %d", window->title, workspace_num);
                move_window_to_desktop(app->display, window->id, target_workspace);
                activate_commanded_window(app, window);
                return TRUE;
            } else {
                log_warn("Workspace %d does not exist (only %d workspaces available)", workspace_num, workspace_count);
            }
        } else {
            log_warn("Invalid workspace number: %d (must be 1-36)", workspace_num);
        }
    } else {
        show_workspace_jump_overlay((struct AppData *)app);
    }
    return FALSE;
}

gboolean cmd_pull_window(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for pull");
        return FALSE;
    }
    
    // Get current workspace
    int current_workspace = get_current_desktop(app->display);
    
    // Move window to current workspace
    log_info("USER: Pulling window '%s' to current workspace %d", 
             window->title, current_workspace + 1);
    move_window_to_desktop(app->display, window->id, current_workspace);
    
    // Activate the window
    activate_commanded_window(app, window);
    
    return TRUE; // Exit command mode
}

gboolean cmd_toggle_monitor(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for monitor toggle");
        return FALSE;
    }
    
    move_window_to_next_monitor(app);
    activate_commanded_window(app, window);
    return TRUE;
}

gboolean cmd_skip_taskbar(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for skip taskbar toggle");
        return FALSE;
    }
    
    toggle_window_state(app->display, window->id, "_NET_WM_STATE_SKIP_TASKBAR");
    activate_commanded_window(app, window);
    return TRUE;
}

gboolean cmd_always_on_top(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for always on top toggle");
        return FALSE;
    }
    
    toggle_window_state(app->display, window->id, "_NET_WM_STATE_ABOVE");
    activate_commanded_window(app, window);
    return TRUE;
}

gboolean cmd_always_below(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for always below toggle");
        return FALSE;
    }
    
    toggle_window_state(app->display, window->id, "_NET_WM_STATE_BELOW");
    activate_commanded_window(app, window);
    return TRUE;
}

gboolean cmd_every_workspace(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for every workspace toggle");
        return FALSE;
    }
    
    toggle_window_state(app->display, window->id, "_NET_WM_STATE_STICKY");
    activate_commanded_window(app, window);
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

gboolean cmd_maximize_window(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for maximizing");
        return FALSE;
    }
    
    toggle_window_state(app->display, window->id, "_NET_WM_STATE_MAXIMIZED_BOTH");
    activate_commanded_window(app, window);
    return TRUE;
}

gboolean cmd_horizontal_maximize(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for horizontal maximizing");
        return FALSE;
    }
    
    toggle_window_state(app->display, window->id, "_NET_WM_STATE_MAXIMIZED_HORZ");
    activate_commanded_window(app, window);
    return TRUE;
}

gboolean cmd_vertical_maximize(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for vertical maximizing");
        return FALSE;
    }
    
    toggle_window_state(app->display, window->id, "_NET_WM_STATE_MAXIMIZED_VERT");
    activate_commanded_window(app, window);
    return TRUE;
}

gboolean cmd_jump_workspace(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    if (strlen(args) > 0) {
        int workspace_num = atoi(args);
        if (workspace_num >= 1 && workspace_num <= 36) {
            int workspace_count = get_number_of_desktops(app->display);
            if (workspace_num <= workspace_count) {
                int target_workspace = workspace_num - 1;
                log_info("USER: Switching to workspace %d", workspace_num);
                switch_to_desktop(app->display, target_workspace);
                return TRUE;
            } else {
                log_warn("Workspace %d does not exist (only %d workspaces available)", workspace_num, workspace_count);
            }
        } else {
            log_warn("Invalid workspace number: %d (must be 1-36)", workspace_num);
        }
    } else {
        show_workspace_jump_overlay((struct AppData *)app);
    }
    return FALSE;
}

gboolean cmd_rename_workspace(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    int workspace_index;
    
    if (strlen(args) > 0) {
        int workspace_num = atoi(args);
        if (workspace_num >= 1 && workspace_num <= 36) {
            int workspace_count = get_number_of_desktops(app->display);
            if (workspace_num <= workspace_count) {
                workspace_index = workspace_num - 1;
                log_info("USER: Renaming workspace %d", workspace_num);
                show_workspace_rename_overlay(app, workspace_index);
                return TRUE;
            } else {
                log_warn("Workspace %d does not exist (only %d workspaces available)", workspace_num, workspace_count);
            }
        } else {
            log_warn("Invalid workspace number: %d (must be 1-36)", workspace_num);
        }
    } else {
        // No argument - rename current workspace
        workspace_index = get_current_desktop(app->display);
        log_info("USER: Renaming current workspace (index %d)", workspace_index);
        show_workspace_rename_overlay(app, workspace_index);
        return TRUE;
    }
    return FALSE;
}

gboolean cmd_tile_window(AppData *app, WindowInfo *window, const char *args) {
    if (!window) {
        log_warn("No window selected for tiling");
        return FALSE;
    }
    
    if (strlen(args) > 0) {
        TileOption option = parse_tile_option(args);
        if (option != (TileOption)-1) {
            log_info("USER: Tiling window '%s' with option: %s", window->title, args);
            apply_tiling(app->display, window->id, option, 3);
            activate_commanded_window(app, window);
            return TRUE;
        } else {
            log_warn("Invalid tiling option: %s", args);
        }
    } else {
        show_tiling_overlay((struct AppData *)app);
    }
    return FALSE;
}

gboolean cmd_assign_name(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_error("No window selected for name assignment");
        return TRUE;
    }
    
    // Check if we're in Names tab - only allow from Windows tab
    if (app->current_tab != TAB_WINDOWS) {
        log_error("Name assignment only available from Windows tab");
        return TRUE;
    }
    
    // Open overlay for name assignment
    show_name_assign_overlay(app);
    
    log_info("CMD: Opening name assignment overlay for window 0x%lx", window->id);
    return FALSE; // Stay in cofi to show overlay
}

gboolean cmd_help(AppData *app, WindowInfo *window __attribute__((unused)), const char *args __attribute__((unused))) {
    show_help_commands(app);
    return FALSE; // Stay in command mode to show help
}

gboolean cmd_mouse(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    if (!app || !app->display) {
        log_warn("Cannot control mouse: display not available");
        return FALSE;
    }
    
    Window root = DefaultRootWindow(app->display);
    
    // Parse the action from args
    if (!args || strlen(args) == 0) {
        log_warn("Mouse command requires an action: away, show, or hide");
        return FALSE;
    }
    
    // Skip any leading spaces
    while (*args == ' ') args++;
    
    // Check for the action - support both full words and single letters
    if (strncmp(args, "away", 4) == 0 || strncmp(args, "a", 1) == 0) {
        // Move mouse to top-left corner (0,0)
        XWarpPointer(app->display, None, root, 0, 0, 0, 0, 0, 0);
        XFlush(app->display);
        log_info("USER: Mouse moved to corner");
    }
    else if (strncmp(args, "show", 4) == 0 || strncmp(args, "s", 1) == 0) {
        // Show the cursor on the root window
        XFixesShowCursor(app->display, root);
        XFlush(app->display);
        log_info("USER: Mouse cursor shown");
    }
    else if (strncmp(args, "hide", 4) == 0 || strncmp(args, "h", 1) == 0) {
        // Hide the cursor on the root window
        XFixesHideCursor(app->display, root);
        XFlush(app->display);
        log_info("USER: Mouse cursor hidden");
    }
    else {
        log_warn("Unknown mouse action: %s (use away/a, show/s, or hide/h)", args);
        return FALSE;
    }
    
    // Always hide the window after this command
    hide_window(app);
    
    return TRUE; // This return value doesn't matter since we called hide_window
}

// Generate command help text in different formats
char* generate_command_help_text(HelpFormat format) {
    // Calculate required buffer size
    size_t buffer_size = 1024;  // Base size for headers and footers
    for (int i = 0; COMMAND_DEFINITIONS[i].primary != NULL; i++) {
        buffer_size += strlen(COMMAND_DEFINITIONS[i].help_format) + strlen(COMMAND_DEFINITIONS[i].description) + 100;
    }

    char *help_text = malloc(buffer_size);
    if (!help_text) return NULL;

    // Add header for CLI format only
    if (format == HELP_FORMAT_CLI) {
        strcpy(help_text, "COFI Command Mode Help\n");
        strcat(help_text, "======================\n\n");
    } else {
        strcpy(help_text, "");
    }

    // Commands list - same for both formats
    strcat(help_text, "Available Commands:\n");
    for (int i = 0; COMMAND_DEFINITIONS[i].primary != NULL; i++) {
        char line[256];
        snprintf(line, sizeof(line), "  %-40s - %s\n",
                 COMMAND_DEFINITIONS[i].help_format, COMMAND_DEFINITIONS[i].description);
        strcat(help_text, line);
    }

    // Usage section - same for both formats
    strcat(help_text, "\nUsage:\n");
    strcat(help_text, "  Press ':' to enter command mode. Press Escape to cancel.\n");
    strcat(help_text, "  Type command and press Enter\n");
    strcat(help_text, "  Commands with arguments can be typed without spaces (e.g., 'cw2', 'j5', 'tL')\n");
    strcat(help_text, "  Direct tiling: 'tr4' (right 75%), 'tl2' (left 50%), 'tc1' (center 33%)\n");

    return help_text;
}

void show_help_commands(AppData *app) {
    if (!app || !app->textbuffer) return;

    char *help_text = generate_command_help_text(HELP_FORMAT_GUI);
    if (help_text) {
        app->command_mode.showing_help = TRUE;
        gtk_text_buffer_set_text(app->textbuffer, help_text, -1);
        free(help_text);
        log_debug("Showing command help");
    }
}