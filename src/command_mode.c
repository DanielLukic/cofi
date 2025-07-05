#include "command_mode.h"
#include "log.h"
#include "workspace_dialog.h"
#include "tiling_dialog.h"
#include "monitor_move.h"
#include "display.h"
#include "selection.h"
#include "x11_utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
    
    app->last_commanded_window_id = win->id;
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

// Clear command line to just ':'
static void clear_command_line(AppData *app) {
    if (!app || !app->entry) return;
    
    gtk_entry_set_text(GTK_ENTRY(app->entry), ":");
    gtk_editable_set_position(GTK_EDITABLE(app->entry), 1);
    app->command_mode.history_index = -1; // Reset history browsing
}

void init_command_mode(CommandMode *cmd) {
    if (!cmd) return;

    cmd->state = CMD_MODE_NORMAL;
    cmd->command_buffer[0] = '\0';
    cmd->cursor_pos = 0;
    cmd->showing_help = FALSE;
    cmd->history_index = -1;

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
    
    // Clear the entry and set the command prompt
    gtk_entry_set_text(GTK_ENTRY(app->entry), ":");
    gtk_editable_set_position(GTK_EDITABLE(app->entry), 1); // Position cursor after ':'
    
    log_info("USER: Entered command mode");
}

void exit_command_mode(AppData *app) {
    if (!app || !app->entry) return;
    
    app->command_mode.state = CMD_MODE_NORMAL;
    app->command_mode.command_buffer[0] = '\0';
    app->command_mode.cursor_pos = 0;
    app->command_mode.showing_help = FALSE;
    app->command_mode.history_index = -1;
    
    // Clear the entry back to normal search mode
    gtk_entry_set_text(GTK_ENTRY(app->entry), "");
    
    // Refresh display to show normal window list
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
            // Get command from entry widget (skip the ':' prefix)
            const char *entry_text = gtk_entry_get_text(GTK_ENTRY(app->entry));
            const char *command = (entry_text && entry_text[0] == ':') ? entry_text + 1 : entry_text;
            
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
        
        case GDK_KEY_BackSpace: {
            // Prevent deleting the ':' prefix
            int cursor_pos = gtk_editable_get_position(GTK_EDITABLE(app->entry));
            if (cursor_pos <= 1) {
                return TRUE; // Block the backspace
            }
            return FALSE; // Let GTK handle it
        }
        
        case GDK_KEY_Left:
        case GDK_KEY_Home: {
            // Prevent cursor from going before ':'
            int cursor_pos = gtk_editable_get_position(GTK_EDITABLE(app->entry));
            if (cursor_pos <= 1) {
                return TRUE; // Block the movement
            }
            return FALSE; // Let GTK handle it
        }
        
        case GDK_KEY_u:
            if (event->state & GDK_CONTROL_MASK) {
                // Ctrl+U: Clear line but keep the ':' prefix
                clear_command_line(app);
                return TRUE;
            }
            return FALSE;

        case GDK_KEY_j:
            if (event->state & GDK_CONTROL_MASK) {
                // Ctrl+J: Navigate history forward (same as Down arrow)
                goto handle_history_forward;
            }
            return FALSE;

        case GDK_KEY_k:
            if (event->state & GDK_CONTROL_MASK) {
                // Ctrl+K: Navigate history backward (same as Up arrow)
                goto handle_history_backward;
            }
            return FALSE;
            
        case GDK_KEY_Up:
        handle_history_backward:
            // Browse command history backwards
            if (app->command_mode.history_count > 0) {
                if (app->command_mode.history_index == -1) {
                    // First time browsing history
                    app->command_mode.history_index = 0;
                } else if (app->command_mode.history_index < app->command_mode.history_count - 1) {
                    app->command_mode.history_index++;
                }

                // Set the entry text to the historical command
                char full_command[258];
                snprintf(full_command, sizeof(full_command), ":%s",
                        app->command_mode.history[app->command_mode.history_index]);
                gtk_entry_set_text(GTK_ENTRY(app->entry), full_command);
                gtk_editable_set_position(GTK_EDITABLE(app->entry), -1); // Move cursor to end
                log_debug("History backward: index=%d, command='%s'",
                          app->command_mode.history_index,
                          app->command_mode.history[app->command_mode.history_index]);
            }
            return TRUE;

        case GDK_KEY_Down:
        handle_history_forward:
            // Browse command history forwards
            if (app->command_mode.history_index > 0) {
                app->command_mode.history_index--;

                // Set the entry text to the historical command
                char full_command[258];
                snprintf(full_command, sizeof(full_command), ":%s",
                        app->command_mode.history[app->command_mode.history_index]);
                gtk_entry_set_text(GTK_ENTRY(app->entry), full_command);
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
            
        default:
            // Let the entry widget handle all other keys naturally
            return FALSE; // Let GTK handle the key event
    }
}

gboolean execute_command(const char *command, AppData *app) {
    if (!command || !app) return FALSE;
    
    log_info("USER: Executing command: '%s'", command);
    
    // Trim leading/trailing whitespace
    while (*command == ' ' || *command == '\t') command++;
    
    if (strlen(command) == 0) {
        return TRUE; // Empty command, just exit
    }
    
    // Command shortcuts
    if (strncmp(command, "cw", 2) == 0 || strncmp(command, "change-workspace", 16) == 0) {
        // Parse change-workspace command with optional workspace argument
        char cmd_name[32];
        char workspace_arg[32];
        int parsed = sscanf(command, "%31s %31s", cmd_name, workspace_arg);

        // Verify it's actually one of our change-workspace commands
        if (!(strcmp(cmd_name, "cw") == 0 || strcmp(cmd_name, "change-workspace") == 0)) {
            log_warn("Unknown command: '%s'. Type 'help' for available commands.", command);
            return FALSE; // Stay in command mode
        }

        WindowInfo *selected_window = get_selected_window(app);
        if (!selected_window) {
            log_warn("No window selected for workspace change");
            return FALSE; // Stay in command mode
        }

        if (parsed == 2) {
            // Direct move with workspace number
            int workspace_num = atoi(workspace_arg);
            if (workspace_num >= 1 && workspace_num <= 36) {
                // Get workspace count to validate
                int workspace_count = get_number_of_desktops(app->display);
                if (workspace_num <= workspace_count) {
                    int target_workspace = workspace_num - 1; // Convert to 0-based

                    log_info("USER: Command '%s %d' -> Moving window '%s' to workspace %d",
                             cmd_name, workspace_num, selected_window->title, workspace_num);

                    // Move the window to the target workspace
                    move_window_to_desktop(app->display, selected_window->id, target_workspace);

                    // Activate the window (this will also switch to the workspace)
                    activate_commanded_window(app, selected_window);

                    log_info("Moved window '%s' to workspace %d",
                             selected_window->title, workspace_num);

                    return TRUE; // Exit command mode after direct move
                } else {
                    log_warn("Workspace %d does not exist (only %d workspaces available)",
                             workspace_num, workspace_count);
                    return FALSE; // Stay in command mode
                }
            } else {
                log_warn("Invalid workspace number: %d (must be 1-36)", workspace_num);
                return FALSE; // Stay in command mode
            }
        } else {
            // No argument provided, show dialog
            show_workspace_move_dialog((struct AppData *)app);
            return TRUE; // Exit command mode after opening dialog
        }
    } else if (strcmp(command, "tm") == 0 || strcmp(command, "toggle-monitor") == 0) {
        WindowInfo *selected_window = get_selected_window(app);
        if (selected_window) {
            move_window_to_next_monitor(app);
            return FALSE; // Stay in command mode after moving window
        } else {
            log_warn("No window selected for monitor move");
            return FALSE; // Stay in command mode
        }
    } else if (strcmp(command, "sb") == 0 || strcmp(command, "skip-taskbar") == 0) {
        WindowInfo *selected_window = get_selected_window(app);
        if (selected_window) {
            log_info("USER: Command 'sb' -> Toggling skip-taskbar for window '%s' (ID: 0x%lx)",
                     selected_window->title, selected_window->id);
            toggle_window_state(app->display, selected_window->id, "_NET_WM_STATE_SKIP_TASKBAR");
            activate_commanded_window(app, selected_window);  // Activate modified window
            return FALSE; // Stay in command mode after toggling state
        } else {
            log_warn("No window selected for skip-taskbar toggle");
            return FALSE; // Stay in command mode
        }
    } else if (strcmp(command, "aot") == 0 || strcmp(command, "at") == 0 || strcmp(command, "always-on-top") == 0) {
        WindowInfo *selected_window = get_selected_window(app);
        if (selected_window) {
            toggle_window_state(app->display, selected_window->id, "_NET_WM_STATE_ABOVE");
            activate_commanded_window(app, selected_window);  // Activate modified window
            return FALSE; // Stay in command mode after toggling state
        } else {
            log_warn("No window selected for always-on-top toggle");
            return FALSE; // Stay in command mode
        }
    } else if (strcmp(command, "ew") == 0 || strcmp(command, "every-workspace") == 0) {
        WindowInfo *selected_window = get_selected_window(app);
        if (selected_window) {
            toggle_window_state(app->display, selected_window->id, "_NET_WM_STATE_STICKY");
            activate_commanded_window(app, selected_window);  // Activate modified window
            return FALSE; // Stay in command mode after toggling state
        } else {
            log_warn("No window selected for every-workspace toggle");
            return FALSE; // Stay in command mode
        }
    } else if (strcmp(command, "cl") == 0 || strcmp(command, "c") == 0 || strcmp(command, "close") == 0 || strcmp(command, "close-window") == 0) {
        WindowInfo *selected_window = get_selected_window(app);
        if (selected_window) {
            close_window(app->display, selected_window->id);
            return FALSE; // Stay in command mode after closing window
        } else {
            log_warn("No window selected for close");
            return FALSE; // Stay in command mode
        }
    } else if (strcmp(command, "mw") == 0 || strcmp(command, "m") == 0 || strcmp(command, "maximize-window") == 0) {
        WindowInfo *selected_window = get_selected_window(app);
        if (selected_window) {
            toggle_maximize_window(app->display, selected_window->id);
            activate_commanded_window(app, selected_window);  // Activate modified window
            return FALSE; // Stay in command mode after toggling maximize
        } else {
            log_warn("No window selected for maximize");
            return FALSE; // Stay in command mode
        }
    } else if (strcmp(command, "hmw") == 0 || strcmp(command, "hm") == 0 || strcmp(command, "horizontal-maximize-window") == 0) {
        WindowInfo *selected_window = get_selected_window(app);
        if (selected_window) {
            toggle_maximize_horizontal(app->display, selected_window->id);
            activate_commanded_window(app, selected_window);  // Activate modified window
            return FALSE; // Stay in command mode after toggling horizontal maximize
        } else {
            log_warn("No window selected for horizontal maximize");
            return FALSE; // Stay in command mode
        }
    } else if (strcmp(command, "vmw") == 0 || strcmp(command, "vm") == 0 || strcmp(command, "vertical-maximize-window") == 0) {
        WindowInfo *selected_window = get_selected_window(app);
        if (selected_window) {
            toggle_maximize_vertical(app->display, selected_window->id);
            activate_commanded_window(app, selected_window);  // Activate modified window
            return FALSE; // Stay in command mode after toggling vertical maximize
        } else {
            log_warn("No window selected for vertical maximize");
            return FALSE; // Stay in command mode
        }
    } else if (strncmp(command, "jw", 2) == 0 || strncmp(command, "jump-workspace", 14) == 0 || strncmp(command, "j", 1) == 0) {
        // Parse jump-workspace command with optional workspace argument
        char cmd_name[32];
        char workspace_arg[32];
        int parsed = sscanf(command, "%31s %31s", cmd_name, workspace_arg);

        // Verify it's actually one of our jump commands (not just starting with 'j')
        if (!(strcmp(cmd_name, "j") == 0 || strcmp(cmd_name, "jw") == 0 || strcmp(cmd_name, "jump-workspace") == 0)) {
            log_warn("Unknown command: '%s'. Type 'help' for available commands.", command);
            return FALSE; // Stay in command mode
        }

        if (parsed == 2) {
            // Direct jump with workspace number
            int workspace_num = atoi(workspace_arg);
            if (workspace_num >= 1 && workspace_num <= 36) {
                // Get workspace count to validate
                int workspace_count = get_number_of_desktops(app->display);
                if (workspace_num <= workspace_count) {
                    int current_workspace = get_current_desktop(app->display);
                    int target_workspace = workspace_num - 1; // Convert to 0-based

                    if (target_workspace != current_workspace) {
                        log_info("USER: Command '%s %d' -> Jumping directly to workspace %d",
                                 cmd_name, workspace_num, workspace_num);
                        switch_to_desktop(app->display, target_workspace);
                        log_info("Jumped from workspace %d to workspace %d",
                                 current_workspace + 1, workspace_num);
                    } else {
                        log_debug("Already on target workspace %d", workspace_num);
                    }
                    return TRUE; // Exit command mode after direct jump
                } else {
                    log_warn("Workspace %d does not exist (only %d workspaces available)",
                             workspace_num, workspace_count);
                    return FALSE; // Stay in command mode
                }
            } else {
                log_warn("Invalid workspace number: %d (must be 1-36)", workspace_num);
                return FALSE; // Stay in command mode
            }
        } else {
            // No argument provided, show dialog
            show_workspace_jump_dialog((struct AppData *)app);
            return TRUE; // Exit command mode after opening dialog
        }
    } else if (strncmp(command, "tw", 2) == 0 || strncmp(command, "tile-window", 11) == 0 || strncmp(command, "t", 1) == 0) {
        // Parse tiling command with optional tiling option argument
        char cmd_name[32];
        char tiling_arg[32];
        int parsed = sscanf(command, "%31s %31s", cmd_name, tiling_arg);

        // Verify it's actually one of our tiling commands (not just starting with 't')
        if (!(strcmp(cmd_name, "t") == 0 || strcmp(cmd_name, "tw") == 0 || strcmp(cmd_name, "tile-window") == 0)) {
            log_warn("Unknown command: '%s'. Type 'help' for available commands.", command);
            return FALSE; // Stay in command mode
        }

        WindowInfo *selected_window = get_selected_window(app);
        if (!selected_window) {
            log_warn("No window selected for tiling");
            return FALSE; // Stay in command mode
        }

        if (parsed == 2) {
            // Direct tiling with option
            TileOption option;
            gboolean valid_option = TRUE;

            // Parse tiling option
            if (strlen(tiling_arg) == 1) {
                switch (tiling_arg[0]) {
                    case 'l':
                    case 'L':
                        option = TILE_LEFT_HALF;
                        break;
                    case 'r':
                    case 'R':
                        option = TILE_RIGHT_HALF;
                        break;
                    case 't':
                    case 'T':
                        option = TILE_TOP_HALF;
                        break;
                    case 'b':
                    case 'B':
                        option = TILE_BOTTOM_HALF;
                        break;
                    case '1':
                        option = TILE_GRID_1;
                        break;
                    case '2':
                        option = TILE_GRID_2;
                        break;
                    case '3':
                        option = TILE_GRID_3;
                        break;
                    case '4':
                        option = TILE_GRID_4;
                        break;
                    case '5':
                        option = TILE_GRID_5;
                        break;
                    case '6':
                        option = TILE_GRID_6;
                        break;
                    case '7':
                        option = TILE_GRID_7;
                        break;
                    case '8':
                        option = TILE_GRID_8;
                        break;
                    case '9':
                        option = TILE_GRID_9;
                        break;
                    case 'f':
                    case 'F':
                        option = TILE_FULLSCREEN;
                        break;
                    case 'c':
                    case 'C':
                        option = TILE_CENTER;
                        break;
                    default:
                        valid_option = FALSE;
                        break;
                }
            } else {
                valid_option = FALSE;
            }

            if (valid_option) {
                log_info("USER: Command '%s %s' -> Tiling window '%s' with option %d",
                         cmd_name, tiling_arg, selected_window->title, option);

                // Apply the tiling directly
                apply_tiling(app->display, selected_window->id, option);

                // Activate the tiled window
                activate_commanded_window(app, selected_window);

                log_info("Applied tiling option %s to window '%s'",
                         tiling_arg, selected_window->title);

                return TRUE; // Exit command mode after direct tiling
            } else {
                log_warn("Invalid tiling option: '%s' (use L/R/T/B, 1-9, F, or C)", tiling_arg);
                return FALSE; // Stay in command mode
            }
        } else {
            // No argument provided, show dialog
            show_tiling_dialog((struct AppData *)app);
            return TRUE; // Exit command mode after opening dialog
        }
    } else if (strcmp(command, "help") == 0 || strcmp(command, "h") == 0 || strcmp(command, "?") == 0) {
        show_help_commands(app);
        return FALSE; // Stay in command mode to show help
    } else {
        log_warn("Unknown command: '%s'. Type 'help' for available commands.", command);
        return FALSE; // Stay in command mode
    }
}

// Generate command help text in different formats
char* generate_command_help_text(HelpFormat format) {
    // Command data structure - single source of truth
    typedef struct {
        const char *shortcuts;
        const char *description;
    } CommandInfo;

    static const CommandInfo commands[] = {
        {"cw, change-workspace [N]", "Move selected window to different workspace (N = workspace number)"},
        {"jw, jump-workspace, j [N]", "Jump to different workspace (N = workspace number)"},
        {"tw, tile-window, t [OPT]", "Tile selected window (OPT: L/R/T/B halves, 1-9 grid, F fullscreen, C center)"},
        {"tm, toggle-monitor", "Move selected window to next monitor"},
        {"sb, skip-taskbar", "Toggle skip taskbar for selected window"},
        {"at, always-on-top, aot", "Toggle always on top for selected window"},
        {"ew, every-workspace", "Toggle show on every workspace for selected window"},
        {"cl, close-window, c", "Close selected window"},
        {"mw, maximize-window, m", "Toggle maximize selected window"},
        {"hm, horizontal-maximize-window, hmw", "Toggle horizontal maximize"},
        {"vm, vertical-maximize-window, vmw", "Toggle vertical maximize"},
        {"help, h, ?", "Show this help"},
        {NULL, NULL}  // Sentinel
    };

    // Calculate required buffer size
    size_t buffer_size = 1024;  // Base size for headers and footers
    for (int i = 0; commands[i].shortcuts != NULL; i++) {
        buffer_size += strlen(commands[i].shortcuts) + strlen(commands[i].description) + 100;
    }

    char *help_text = malloc(buffer_size);
    if (!help_text) return NULL;

    if (format == HELP_FORMAT_CLI) {
        // CLI format
        strcpy(help_text, "COFI Command Mode Help\n");
        strcat(help_text, "======================\n\n");
        strcat(help_text, "Available Commands:\n");

        for (int i = 0; commands[i].shortcuts != NULL; i++) {
            char line[256];
            snprintf(line, sizeof(line), "  %-40s - %s\n",
                     commands[i].shortcuts, commands[i].description);
            strcat(help_text, line);
        }

        strcat(help_text, "\nUsage:\n");
        strcat(help_text, "  Press ':' to enter command mode\n");
        strcat(help_text, "  Type command and press Enter\n");
        strcat(help_text, "  Press Escape to cancel\n");

    } else {
        // GUI format
        strcpy(help_text, "COFI Command Mode Help\n");
        strcat(help_text, "=====================\n\n");
        strcat(help_text, "Available Commands:\n");

        for (int i = 0; commands[i].shortcuts != NULL; i++) {
            char line[256];
            snprintf(line, sizeof(line), "  %-40s - %s\n",
                     commands[i].shortcuts, commands[i].description);
            strcat(help_text, line);
        }

        strcat(help_text, "\nUsage:\n");
        strcat(help_text, "  Press ':' to enter command mode\n");
        strcat(help_text, "  Type command and press Enter\n");
        strcat(help_text, "  Press Escape to cancel\n\n");
        strcat(help_text, "Press Escape to return to window list");
    }

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