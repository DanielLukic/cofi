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
#include "app_data.h"
#include "hotkey_config.h"
#include "types.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

extern void hide_window(AppData *app);
extern void dispatch_hotkey_mode(AppData *app, ShowMode mode);

static struct {
    char history[10][256];
    int history_count;
    gboolean initialized;
} g_command_history = { .initialized = FALSE };

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

static void clear_command_line(AppData *app) {
    if (!app || !app->entry) return;
    
    gtk_entry_set_text(GTK_ENTRY(app->entry), "");
    app->command_mode.history_index = -1;
}

static int clamp_int(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static int count_lines_in_text(const char *text) {
    if (!text || text[0] == '\0') {
        return 1;
    }

    int lines = 1;
    for (const char *p = text; *p; p++) {
        if (*p == '\n') {
            lines++;
        }
    }
    return lines;
}

static int split_lines_in_place(char *text, char **lines, int max_lines) {
    if (!text || !lines || max_lines <= 0) {
        return 0;
    }

    int count = 0;
    lines[count++] = text;
    for (char *p = text; *p && count < max_lines; p++) {
        if (*p == '\n') {
            *p = '\0';
            lines[count++] = p + 1;
        }
    }

    return count;
}

static char *build_help_page_text(AppData *app, char **lines, int total_lines, int visible_lines, int scroll_offset) {
    (void)app;
    if (!lines || total_lines <= 0 || visible_lines <= 0) {
        return NULL;
    }

    int max_offset = (total_lines > visible_lines) ? (total_lines - visible_lines) : 0;
    int start = clamp_int(scroll_offset, 0, max_offset);
    int end = start + visible_lines;
    if (end > total_lines) end = total_lines;

    GString *rendered = g_string_new(NULL);
    for (int i = start; i < end; i++) {
        g_string_append(rendered, lines[i]);
        g_string_append_c(rendered, '\n');
    }

    // Help scrollbar: top-down, no inversion needed
    overlay_scrollbar(rendered, total_lines, visible_lines, start);

    return g_string_free(rendered, FALSE);
}

static void render_help_page(AppData *app, int requested_offset) {
    if (!app || !app->textbuffer) {
        return;
    }

    char *help_text = generate_command_help_text(HELP_FORMAT_GUI);
    if (!help_text) {
        return;
    }

    int total_lines = count_lines_in_text(help_text);
    int visible_lines = get_max_display_lines_dynamic(app);
    if (visible_lines < 1) {
        visible_lines = 1;
    }

    int max_offset = (total_lines > visible_lines) ? (total_lines - visible_lines) : 0;
    int clamped_offset = clamp_int(requested_offset, 0, max_offset);
    app->command_mode.help_scroll_offset = clamped_offset;

    char **lines = malloc(sizeof(char *) * total_lines);
    if (!lines) {
        gtk_text_buffer_set_text(app->textbuffer, help_text, -1);
        free(help_text);
        return;
    }

    int split_count = split_lines_in_place(help_text, lines, total_lines);
    char *rendered = build_help_page_text(app, lines, split_count, visible_lines, clamped_offset);

    if (rendered) {
        gtk_text_buffer_set_text(app->textbuffer, rendered, -1);
        free(rendered);
    } else {
        gtk_text_buffer_set_text(app->textbuffer, help_text, -1);
    }

    free(lines);
    free(help_text);
}

static gboolean handle_help_navigation_key(GdkEventKey *event, AppData *app) {
    int offset = app->command_mode.help_scroll_offset;
    int page = get_max_display_lines_dynamic(app);
    if (page < 1) {
        page = 1;
    }

    switch (event->keyval) {
        case GDK_KEY_Up:
            render_help_page(app, offset - 1);
            return TRUE;
        case GDK_KEY_Down:
            render_help_page(app, offset + 1);
            return TRUE;
        case GDK_KEY_Page_Up:
            render_help_page(app, offset - page);
            return TRUE;
        case GDK_KEY_Page_Down:
            render_help_page(app, offset + page);
            return TRUE;
        case GDK_KEY_Home:
            render_help_page(app, 0);
            return TRUE;
        case GDK_KEY_End:
            render_help_page(app, INT_MAX);
            return TRUE;
        default:
            return FALSE;
    }
}

void init_command_mode(CommandMode *cmd) {
    if (!cmd) return;

    cmd->state = CMD_MODE_NORMAL;
    cmd->command_buffer[0] = '\0';
    cmd->cursor_pos = 0;
    cmd->showing_help = FALSE;
    cmd->help_scroll_offset = 0;
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
    app->command_mode.help_scroll_offset = 0;
    
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
    app->command_mode.help_scroll_offset = 0;
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
    
    // If help/config is being shown, handle navigation keys and Esc
    if (app->command_mode.showing_help) {
        if (handle_help_navigation_key(event, app)) {
            return TRUE;
        }
        // All other keys fall through to normal command mode handling
        // Display stays visible until Esc (exit_command_mode clears it)
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

        case GDK_KEY_Tab:
        case GDK_KEY_ISO_Left_Tab:
            // Fall through to tab switching, stay in command mode
            return FALSE;

        default:
            // Let the entry widget handle all other keys naturally
            return FALSE; // Let GTK handle the key event
    }
}

static const CommandDef* find_command(const char *cmd_name) {
    for (int i = 0; COMMAND_DEFINITIONS[i].primary != NULL; i++) {
        if (strcmp(cmd_name, COMMAND_DEFINITIONS[i].primary) == 0)
            return &COMMAND_DEFINITIONS[i];
        for (int j = 0; j < 5 && COMMAND_DEFINITIONS[i].aliases[j] != NULL; j++) {
            if (strcmp(cmd_name, COMMAND_DEFINITIONS[i].aliases[j]) == 0)
                return &COMMAND_DEFINITIONS[i];
        }
    }
    return NULL;
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

static gboolean execute_single_command_with_window(const char *command, AppData *app, WindowInfo *window) {
    char cmd_name[128] = {0};
    char arg[256] = {0};

    if (!parse_command_and_arg(command, cmd_name, arg, sizeof(cmd_name), sizeof(arg))) {
        return FALSE;
    }

    if (cmd_name[0] == '\0') {
        return TRUE;
    }

    const CommandDef *cmd = find_command(cmd_name);
    if (!cmd) {
        log_warn("Unknown command: '%s'. Type 'help' for available commands.", cmd_name);
        return FALSE;
    }

    return cmd->handler(app, window, arg);
}

static gboolean execute_command_impl(const char *command, AppData *app, WindowInfo *window) {
    char local[512] = {0};
    strncpy(local, command, sizeof(local) - 1);
    trim_whitespace_in_place(local);

    if (local[0] == '\0') return TRUE;

    char *saveptr = NULL;
    char *token = strtok_r(local, ",", &saveptr);
    while (token) {
        trim_whitespace_in_place(token);
        if (token[0] != '\0') {
            if (!execute_single_command_with_window(token, app, window)) {
                return FALSE;
            }
        }
        token = strtok_r(NULL, ",", &saveptr);
    }
    return TRUE;
}

gboolean execute_command(const char *command, AppData *app) {
    if (!command || !app) return FALSE;
    log_info("USER: Executing command: '%s'", command);
    return execute_command_impl(command, app, get_selected_window(app));
}

gboolean execute_command_with_window(const char *command, AppData *app, WindowInfo *window) {
    if (!command || !app) return FALSE;
    log_info("HOTKEY: Executing command: '%s'", command);
    return execute_command_impl(command, app, window);
}

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
            activate_commanded_window(app, window);
            return TRUE;
        } else {
            log_warn("Invalid workspace target: %s", args);
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
        int target = resolve_workspace_from_arg(app->display, args, app->config.workspaces_per_row);
        if (target >= 0) {
            log_info("USER: Switching to workspace %d", target + 1);
            switch_to_desktop(app->display, target);
            return TRUE;
        } else {
            log_warn("Invalid workspace target: %s", args);
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
            apply_tiling(app->display, window->id, option, app->config.tile_columns);
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

gboolean cmd_move_all_to_workspace(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    if (!app || !app->display) {
        log_warn("Cannot move windows: display not available");
        return FALSE;
    }
    
    // Get current workspace
    int current_workspace = get_current_desktop(app->display);
    
    // Clear the windows to move list
    app->windows_to_move_count = 0;
    
    // Collect all windows on current workspace that are:
    // - Not special windows (type != "Special")
    // - Not sticky (not on all desktops)
    // - On the current workspace
    int normal_window_count = 0;
    for (int i = 0; i < app->window_count; i++) {
        WindowInfo *win = &app->windows[i];
        
        // Skip special windows
        if (strcmp(win->type, "Special") == 0) {
            log_debug("Skipping special window: %s", win->title);
            continue;
        }
        
        // Skip windows not on current workspace (-1 means sticky/all desktops)
        if (win->desktop != current_workspace) {
            if (win->desktop == -1 || win->desktop == 0xFFFFFFFF) {
                log_debug("Skipping sticky window: %s", win->title);
            }
            continue;
        }
        
        // Check if window is sticky via _NET_WM_STATE_STICKY
        if (get_window_state(app->display, win->id, "_NET_WM_STATE_STICKY")) {
            log_debug("Skipping sticky window (state check): %s", win->title);
            continue;
        }
        
        // Add to list of windows to move
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
    
    log_info("USER: Found %d windows to move from workspace %d", normal_window_count, current_workspace + 1);
    
    // If args provided, move directly. Otherwise show overlay
    if (strlen(args) > 0) {
        int target = resolve_workspace_from_arg(app->display, args, app->config.workspaces_per_row);
        if (target >= 0) {
            for (int i = 0; i < app->windows_to_move_count; i++)
                move_window_to_desktop(app->display, app->windows_to_move[i], target);
            switch_to_desktop(app->display, target);
            log_info("USER: Moved %d windows from workspace %d to %d",
                     app->windows_to_move_count, current_workspace + 1, target + 1);
            return TRUE;
        } else {
            log_warn("Invalid workspace target: %s", args);
        }
        return FALSE;
    } else {
        // Show workspace selection overlay
        show_workspace_move_all_overlay((struct AppData *)app);
        return FALSE; // Don't exit command mode, overlay is shown
    }
}

// Swap window positions and sizes
gboolean cmd_swap_windows(AppData *app, WindowInfo *window, const char *args __attribute__((unused))) {
    if (!window) {
        log_warn("No window selected for swap");
        return FALSE;
    }
    
    // We need at least 2 windows to swap
    if (app->filtered_count < 2) {
        log_warn("Need at least 2 windows to swap (have %d)", app->filtered_count);
        return FALSE;
    }
    
    // Get the two windows to swap (selected and the first in list)
    WindowInfo *window1 = window;  // Currently selected window
    WindowInfo *window2 = NULL;
    
    // Find the other window to swap with (first non-selected window)
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
    
    // Get current geometries (these will be the actual maximized sizes if maximized)
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
    
    // Get maximization states
    gboolean win1_max_vert = get_window_state(app->display, window1->id, "_NET_WM_STATE_MAXIMIZED_VERT");
    gboolean win1_max_horz = get_window_state(app->display, window1->id, "_NET_WM_STATE_MAXIMIZED_HORZ");
    gboolean win2_max_vert = get_window_state(app->display, window2->id, "_NET_WM_STATE_MAXIMIZED_VERT");
    gboolean win2_max_horz = get_window_state(app->display, window2->id, "_NET_WM_STATE_MAXIMIZED_HORZ");
    
    log_debug("Window 1 max state: vert=%d, horz=%d", win1_max_vert, win1_max_horz);
    log_debug("Window 2 max state: vert=%d, horz=%d", win2_max_vert, win2_max_horz);
    
    // Prepare atoms for state changes
    Atom net_wm_state = XInternAtom(app->display, "_NET_WM_STATE", False);
    Atom net_wm_state_maximized_vert = XInternAtom(app->display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    Atom net_wm_state_maximized_horz = XInternAtom(app->display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    
    // Step 1: Force unmaximize both windows completely
    if (win1_max_vert || win1_max_horz) {
        XEvent event;
        memset(&event, 0, sizeof(event));
        event.type = ClientMessage;
        event.xclient.window = window1->id;
        event.xclient.message_type = net_wm_state;
        event.xclient.format = 32;
        event.xclient.data.l[0] = 0; // _NET_WM_STATE_REMOVE
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
        event.xclient.data.l[0] = 0; // _NET_WM_STATE_REMOVE
        event.xclient.data.l[1] = net_wm_state_maximized_vert;
        event.xclient.data.l[2] = net_wm_state_maximized_horz;
        
        XSendEvent(app->display, DefaultRootWindow(app->display), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &event);
    }
    
    XFlush(app->display);
    usleep(100000); // Give WM more time to process unmaximize
    
    // Step 2: Apply the swapped geometries
    XMoveResizeWindow(app->display, window1->id, x2, y2, w2, h2);
    XMoveResizeWindow(app->display, window2->id, x1, y1, w1, h1);
    XFlush(app->display);
    usleep(50000); // Small delay
    
    // Step 3: Apply maximization states (swapped)
    if (win2_max_vert || win2_max_horz) {
        XEvent event;
        memset(&event, 0, sizeof(event));
        event.type = ClientMessage;
        event.xclient.window = window1->id;
        event.xclient.message_type = net_wm_state;
        event.xclient.format = 32;
        event.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
        
        // Add states one at a time for better compatibility
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
        event.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
        
        // Add states one at a time for better compatibility
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
    usleep(50000); // Let WM process maximization
    
    // Step 4: Re-apply the exact sizes to override WM's default maximize behavior
    // This ensures we get the exact swapped sizes, not the WM's idea of maximized
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

    // Split args into key and value at first [ =]+ separator
    char key[64] = {0};
    const char *value = "";
    const char *sep = args;
    while (*sep && *sep != ' ' && *sep != '=') sep++;
    size_t key_len = (size_t)(sep - args);
    if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
    memcpy(key, args, key_len);
    while (*sep == ' ' || *sep == '=') sep++;
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
        // Switch to Config tab to show the change
        exit_command_mode(app);
        switch_to_tab(app, TAB_CONFIG);
    } else {
        char msg[512];
        snprintf(msg, sizeof(msg), "Error: %s\n\nType :config to see available keys.", err);
        show_error_in_display(app, msg);
    }
    return FALSE;
}

gboolean cmd_show_config(AppData *app, WindowInfo *window __attribute__((unused)), const char *args __attribute__((unused))) {
    exit_command_mode(app);
    switch_to_tab(app, TAB_CONFIG);
    return FALSE;
}

gboolean cmd_show(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    ShowMode mode = SHOW_MODE_WINDOWS;

    if (args && args[0] != '\0') {
        if (strcmp(args, "windows") == 0)         mode = SHOW_MODE_WINDOWS;
        else if (strcmp(args, "command") == 0)     mode = SHOW_MODE_COMMAND;
        else if (strcmp(args, "workspaces") == 0)  mode = SHOW_MODE_WORKSPACES;
        else if (strcmp(args, "harpoon") == 0)     mode = SHOW_MODE_HARPOON;
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

    // Exit command mode first, then dispatch
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
        log_info("Hotkey bound: %s → %s", key, cmd);
    } else if (action == 2) {
        if (remove_hotkey_binding(&app->hotkey_config, key)) {
            save_hotkey_config(&app->hotkey_config);
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

gboolean cmd_assign_slots(AppData *app, WindowInfo *window __attribute__((unused)), const char *args __attribute__((unused))) {
    assign_workspace_slots(app);
    hide_window(app);
    log_info("Assigned workspace slots via command mode");
    return TRUE;
}

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
    strcat(help_text, "Available commands:\n");
    strcat(help_text, "\n");
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
    strcat(help_text, "  Chain multiple commands with commas (e.g., 'tc,vm' or 'cw2,tc')\n");
    strcat(help_text, "  Direct tiling: 'tr4' (right 75%), 'tl2' (left 50%), 'tc1' (center 33%)");

    return help_text;
}

void show_help_commands(AppData *app) {
    if (!app || !app->textbuffer) return;

    app->command_mode.showing_help = TRUE;
    app->command_mode.help_scroll_offset = 0;
    render_help_page(app, 0);
    log_debug("Showing command help");
}
