#include "command_mode.h"
#include "command_api.h"
#include "display.h"
#include "log.h"
#include "run_mode.h"
#include "selection.h"
#include "dynamic_display.h"

#include <string.h>
#include <stdlib.h>
#include <limits.h>

extern void hide_window(AppData *app);

static struct {
    char history[10][256];
    int history_count;
    gboolean initialized;
} g_command_history = { .initialized = FALSE };

static void initialize_global_history(void) {
    if (g_command_history.initialized) {
        return;
    }

    g_command_history.history_count = 0;
    for (int i = 0; i < 10; i++) {
        g_command_history.history[i][0] = '\0';
    }
    g_command_history.initialized = TRUE;
    log_debug("Initialized global command history");
}

static void add_to_history(CommandMode *cmd, const char *command) {
    if (!command || command[0] == '\0') {
        return;
    }

    initialize_global_history();
    if (g_command_history.history_count > 0 &&
        strcmp(g_command_history.history[0], command) == 0) {
        return;
    }

    for (int i = 9; i > 0; i--) {
        strcpy(g_command_history.history[i], g_command_history.history[i - 1]);
    }

    strncpy(g_command_history.history[0], command, 255);
    g_command_history.history[0][255] = '\0';
    if (g_command_history.history_count < 10) {
        g_command_history.history_count++;
    }

    if (!cmd) {
        return;
    }

    cmd->history_count = g_command_history.history_count;
    for (int i = 0; i < g_command_history.history_count; i++) {
        strcpy(cmd->history[i], g_command_history.history[i]);
    }
}

static void clear_command_line(AppData *app) {
    if (!app || !app->entry) {
        return;
    }

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

static char *build_help_page_text(AppData *app, char **lines, int total_lines,
                                  int visible_lines, int scroll_offset) {
    if (!app || !lines || total_lines <= 0 || visible_lines <= 0) {
        return NULL;
    }

    int max_offset = (total_lines > visible_lines) ? (total_lines - visible_lines) : 0;
    int start = clamp_int(scroll_offset, 0, max_offset);
    int end = start + visible_lines;
    if (end > total_lines) {
        end = total_lines;
    }

    GString *rendered = g_string_new(NULL);
    for (int i = start; i < end; i++) {
        g_string_append(rendered, lines[i]);
        g_string_append_c(rendered, '\n');
    }

    overlay_scrollbar(rendered, total_lines, visible_lines, start, get_display_columns(app));
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
    if (!cmd) {
        return;
    }

    cmd->state = CMD_MODE_NORMAL;
    cmd->command_buffer[0] = '\0';
    cmd->cursor_pos = 0;
    cmd->showing_help = FALSE;
    cmd->help_scroll_offset = 0;
    cmd->history_index = -1;
    cmd->close_on_exit = FALSE;

    initialize_global_history();
    cmd->history_count = g_command_history.history_count;
    for (int i = 0; i < g_command_history.history_count; i++) {
        strcpy(cmd->history[i], g_command_history.history[i]);
    }
    for (int i = g_command_history.history_count; i < 10; i++) {
        cmd->history[i][0] = '\0';
    }
}

void enter_command_mode(AppData *app) {
    if (!app || !app->entry) {
        return;
    }

    app->command_mode.state = CMD_MODE_COMMAND;
    app->command_mode.command_buffer[0] = '\0';
    app->command_mode.cursor_pos = 0;
    app->command_mode.help_scroll_offset = 0;

    if (app->current_tab == TAB_WINDOWS && app->filtered_count > 0 &&
        app->command_target_id != 0) {
        for (int i = 0; i < app->filtered_count; i++) {
            if ((Window)app->filtered[i].id == app->command_target_id) {
                app->selection.window_index = i;
                app->selection.selected_window_id = app->filtered[i].id;
                update_display(app);
                log_debug("Command mode: selected window 0x%lx at index %d by pre-focus ID",
                          app->command_target_id, i);
                break;
            }
        }
    } else if (app->current_tab == TAB_WINDOWS && app->filtered_count > 0 &&
               app->selection.window_index == 1) {
        app->selection.window_index = 0;
        app->selection.selected_window_id = app->filtered[0].id;
        update_display(app);
        log_debug("Command mode: reset selection from alt-tab default (index 1) to index 0");
    }

    if (app->mode_indicator) {
        gtk_label_set_text(GTK_LABEL(app->mode_indicator), ":");
    }

    gtk_entry_set_text(GTK_ENTRY(app->entry), "");
    log_info("USER: Entered command mode");
}

void exit_command_mode(AppData *app) {
    if (!app || !app->entry) {
        return;
    }

    gboolean should_close = app->command_mode.close_on_exit;

    app->command_mode.state = CMD_MODE_NORMAL;
    app->command_mode.command_buffer[0] = '\0';
    app->command_mode.cursor_pos = 0;
    app->command_mode.showing_help = FALSE;
    app->command_mode.help_scroll_offset = 0;
    app->command_mode.history_index = -1;
    app->command_mode.close_on_exit = FALSE;
    app->command_target_id = 0;

    if (should_close) {
        log_info("USER: Exited command mode (started with --command, closing window)");
        hide_window(app);
        return;
    }

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

    if (app->command_mode.showing_help && handle_help_navigation_key(event, app)) {
        return TRUE;
    }

    switch (event->keyval) {
        case GDK_KEY_Escape:
            exit_command_mode(app);
            return TRUE;

        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter: {
            const char *command = gtk_entry_get_text(GTK_ENTRY(app->entry));
            if (command && command[0] != '\0') {
                add_to_history(&app->command_mode, command);
            }

            gboolean should_exit = execute_command(command, app);
            if (should_exit) {
                exit_command_mode(app);
            } else {
                clear_command_line(app);
            }
            return TRUE;
        }

        case GDK_KEY_u:
            if (event->state & GDK_CONTROL_MASK) {
                clear_command_line(app);
                return TRUE;
            }
            return FALSE;

        case GDK_KEY_j:
            if (event->state & GDK_CONTROL_MASK) {
                move_selection_down(app);
                return TRUE;
            }
            return FALSE;

        case GDK_KEY_k:
            if (event->state & GDK_CONTROL_MASK) {
                move_selection_up(app);
                return TRUE;
            }
            return FALSE;

        case GDK_KEY_Up:
            if (app->command_mode.history_count > 0) {
                if (app->command_mode.history_index == -1) {
                    app->command_mode.history_index = 0;
                } else if (app->command_mode.history_index < app->command_mode.history_count - 1) {
                    app->command_mode.history_index++;
                }

                gtk_entry_set_text(GTK_ENTRY(app->entry),
                        app->command_mode.history[app->command_mode.history_index]);
                gtk_editable_set_position(GTK_EDITABLE(app->entry), -1);
            }
            return TRUE;

        case GDK_KEY_Down:
            if (app->command_mode.history_index > 0) {
                app->command_mode.history_index--;
                gtk_entry_set_text(GTK_ENTRY(app->entry),
                        app->command_mode.history[app->command_mode.history_index]);
                gtk_editable_set_position(GTK_EDITABLE(app->entry), -1);
            } else if (app->command_mode.history_index == 0) {
                app->command_mode.history_index = -1;
                clear_command_line(app);
            }
            return TRUE;

        case GDK_KEY_colon:
            return TRUE;

        case GDK_KEY_exclam:
            enter_run_mode(app, NULL);
            return TRUE;

        case GDK_KEY_Tab:
        case GDK_KEY_ISO_Left_Tab:
            return FALSE;

        default:
            return FALSE;
    }
}

void show_help_commands(AppData *app) {
    if (!app || !app->textbuffer) {
        return;
    }

    app->command_mode.showing_help = TRUE;
    app->command_mode.help_scroll_offset = 0;
    render_help_page(app, 0);
    log_debug("Showing command help");
}
