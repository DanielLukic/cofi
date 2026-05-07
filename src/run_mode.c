#include "run_mode.h"

#include <string.h>

#include "display.h"
#include "log.h"
#include "detach_launch.h"
#include "prefix_tabs.h"
#include "selection.h"
#include "tab_switching.h"

extern void hide_window(AppData *app);

static void set_run_entry_text(AppData *app, const char *text) {
    if (!app || !app->entry) {
        return;
    }

    app->run_mode.suppress_entry_change = TRUE;
    gtk_entry_set_text(GTK_ENTRY(app->entry), text);
    gtk_editable_set_position(GTK_EDITABLE(app->entry), -1);
    app->run_mode.suppress_entry_change = FALSE;
}


void init_run_mode(RunMode *run_mode) {
    if (!run_mode) {
        return;
    }

    memset(run_mode, 0, sizeof(*run_mode));
    run_mode->history_index = -1;
}

gboolean extract_run_command(const char *entry_text, char *command_out, size_t command_size) {
    if (!command_out || command_size == 0) {
        return FALSE;
    }

    command_out[0] = '\0';
    if (!entry_text) {
        return FALSE;
    }

    const char *command = entry_text;
    if (command[0] == '!') {
        command++;
    }

    while (*command && g_ascii_isspace(*command)) {
        command++;
    }

    g_strlcpy(command_out, command, command_size);
    g_strstrip(command_out);
    return command_out[0] != '\0';
}

void add_run_history_entry(RunMode *run_mode, const char *command) {
    if (!run_mode || !command || command[0] == '\0') {
        return;
    }

    if (run_mode->history_count > 0 &&
        strcmp(run_mode->history[0], command) == 0) {
        return;
    }

    int cap = RUN_HISTORY_CAP;
    for (int i = (cap - 1 < run_mode->history_count ? cap - 1 : run_mode->history_count); i > 0; i--) {
        strcpy(run_mode->history[i], run_mode->history[i - 1]);
    }

    g_strlcpy(run_mode->history[0], command, sizeof(run_mode->history[0]));
    if (run_mode->history_count < RUN_HISTORY_CAP) {
        run_mode->history_count++;
    }
    run_mode->history_index = -1;
}

gboolean browse_run_history(RunMode *run_mode, int direction, char *entry_text_out, size_t entry_text_size) {
    if (!run_mode || !entry_text_out || entry_text_size == 0) {
        return FALSE;
    }

    if (direction < 0) {
        if (run_mode->history_count == 0) {
            return FALSE;
        }
        if (run_mode->history_index == -1) {
            run_mode->history_index = 0;
        } else if (run_mode->history_index < run_mode->history_count - 1) {
            run_mode->history_index++;
        }
    } else if (direction > 0) {
        if (run_mode->history_index > 0) {
            run_mode->history_index--;
        } else if (run_mode->history_index == 0) {
            run_mode->history_index = -1;
        } else {
            return FALSE;
        }
    } else {
        return FALSE;
    }

    if (run_mode->history_index >= 0) {
        g_strlcpy(entry_text_out,
                  run_mode->history[run_mode->history_index],
                  entry_text_size);
    } else {
        g_strlcpy(entry_text_out, "", entry_text_size);
    }
    return TRUE;
}

void enter_run_mode(AppData *app, const char *prefill_command) {
    if (!app || !app->entry) {
        return;
    }

    app->command_mode.state = CMD_MODE_RUN;
    app->run_mode.history_index = -1;

    if (app->mode_indicator) {
        gtk_label_set_text(GTK_LABEL(app->mode_indicator), "!");
    }

    if (prefill_command && prefill_command[0] != '\0') {
        const char *stripped = prefill_command;
        if (stripped[0] == '!') {
            stripped++;
            while (*stripped == ' ') stripped++;
        }
        set_run_entry_text(app, stripped);
    } else {
        set_run_entry_text(app, "");
    }

    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "command");
    surface_tab(app, TAB_RUN);

    log_info("USER: Entered run mode");
}

void exit_run_mode(AppData *app) {
    if (!app || !app->entry) {
        return;
    }

    if (app->command_mode.state == CMD_MODE_NORMAL) {
        return;
    }

    TabMode origin = app->prefix_origin_tab;
    gboolean should_close = app->run_mode.close_on_exit;

    app->command_mode.state = CMD_MODE_NORMAL;
    app->run_mode.history_index = -1;
    app->run_mode.close_on_exit = FALSE;
    app->tab_visibility[TAB_RUN] = TAB_VIS_HIDDEN;
    clear_prefix_tab_claim(app);

    if (should_close) {
        log_info("USER: Exited run mode (started with --run, closing window)");
        hide_window(app);
        return;
    }

    if (app->mode_indicator) {
        gtk_label_set_text(GTK_LABEL(app->mode_indicator), ">");
    }

    set_run_entry_text(app, "");
    switch_to_tab(app, origin);
    update_display(app);
    log_info("USER: Exited run mode");
}

void handle_run_entry_changed(GtkEntry *entry, AppData *app) {
    if (!app || app->command_mode.state != CMD_MODE_RUN || app->run_mode.suppress_entry_change) {
        return;
    }

    const char *text = gtk_entry_get_text(entry);
    if (!text || text[0] == '\0' || strcmp(text, "!") == 0) {
        exit_run_mode(app);
        return;
    }
}

gboolean handle_run_key(GdkEventKey *event, AppData *app) {
    if (!app || app->command_mode.state != CMD_MODE_RUN) {
        return FALSE;
    }

    switch (event->keyval) {
        case GDK_KEY_Escape: {
            const char *text = gtk_entry_get_text(GTK_ENTRY(app->entry));
            if (text[0] != '\0') {
                app->run_mode.suppress_entry_change = TRUE;
                gtk_entry_set_text(GTK_ENTRY(app->entry), "");
                app->run_mode.suppress_entry_change = FALSE;
                update_display(app);
                return TRUE;
            }
            exit_run_mode(app);
            return TRUE;
        }

        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter: {
            const char *text = gtk_entry_get_text(GTK_ENTRY(app->entry));
            if (text[0] != '\0') {
                char command[256];
                if (extract_run_command(text, command, sizeof(command))) {
                    if (detach_launch_shell(command)) {
                        add_run_history_entry(&app->run_mode, command);
                        reset_selection(app);
                        update_scroll_position(app);
                        update_display(app);
                        hide_window(app);
                    }
                }
            } else if (app->run_mode.history_count > 0) {
                int sel = app->selection.run_index;
                if (sel >= 0 && sel < app->run_mode.history_count) {
                    detach_launch_shell(app->run_mode.history[sel]);
                    hide_window(app);
                }
            }
            return TRUE;
        }

        case GDK_KEY_Up:
            move_selection_up(app);
            return TRUE;

        case GDK_KEY_Down:
            move_selection_down(app);
            return TRUE;

        case GDK_KEY_Tab:
        case GDK_KEY_ISO_Left_Tab:
            exit_run_mode(app);
            return FALSE;

        default:
            return FALSE;
    }
}
