#include "run_mode.h"

#include <gio/gio.h>
#include <string.h>

#include "display.h"
#include "log.h"

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

static gboolean launch_detached_shell_command(const char *command) {
    const char *shell = g_getenv("SHELL");
    if (!shell || shell[0] == '\0') {
        shell = "/bin/sh";
    }

    const char *argv[] = { shell, "-c", command, NULL };
    GError *error = NULL;
    GSubprocessLauncher *launcher = g_subprocess_launcher_new(
        (GSubprocessFlags)(G_SUBPROCESS_FLAGS_STDIN_INHERIT |
                           G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                           G_SUBPROCESS_FLAGS_STDERR_SILENCE));

    GSubprocess *process = g_subprocess_launcher_spawnv(launcher, argv, &error);
    g_object_unref(launcher);

    if (!process) {
        log_error("Failed to launch run command '%s': %s",
                  command, error ? error->message : "unknown error");
        g_clear_error(&error);
        return FALSE;
    }

    g_object_unref(process);
    log_info("USER: Launched run command '%s'", command);
    return TRUE;
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

    for (int i = 9; i > 0; i--) {
        strcpy(run_mode->history[i], run_mode->history[i - 1]);
    }

    g_strlcpy(run_mode->history[0], command, sizeof(run_mode->history[0]));
    if (run_mode->history_count < 10) {
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
        g_snprintf(entry_text_out, entry_text_size, "!%s",
                   run_mode->history[run_mode->history_index]);
    } else {
        g_strlcpy(entry_text_out, "!", entry_text_size);
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
        char entry_text[257];
        g_snprintf(entry_text, sizeof(entry_text), "!%s", prefill_command);
        set_run_entry_text(app, entry_text);
    } else {
        set_run_entry_text(app, "!");
    }

    log_info("USER: Entered run mode");
}

void exit_run_mode(AppData *app) {
    if (!app || !app->entry) {
        return;
    }

    gboolean should_close = app->run_mode.close_on_exit;

    app->command_mode.state = CMD_MODE_NORMAL;
    app->run_mode.history_index = -1;
    app->run_mode.close_on_exit = FALSE;

    if (should_close) {
        log_info("USER: Exited run mode (started with --run, closing window)");
        hide_window(app);
        return;
    }

    if (app->mode_indicator) {
        gtk_label_set_text(GTK_LABEL(app->mode_indicator), ">");
    }

    set_run_entry_text(app, "");
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

    if (text[0] != '!') {
        char entry_text[257];
        g_snprintf(entry_text, sizeof(entry_text), "!%s", text);
        set_run_entry_text(app, entry_text);
    }
}

gboolean handle_run_key(GdkEventKey *event, AppData *app) {
    if (!app || app->command_mode.state != CMD_MODE_RUN) {
        return FALSE;
    }

    switch (event->keyval) {
        case GDK_KEY_Escape:
            exit_run_mode(app);
            return TRUE;

        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter: {
            char command[256];
            if (!extract_run_command(gtk_entry_get_text(GTK_ENTRY(app->entry)),
                                     command, sizeof(command))) {
                return TRUE;
            }

            if (launch_detached_shell_command(command)) {
                add_run_history_entry(&app->run_mode, command);
                hide_window(app);
            }
            return TRUE;
        }

        case GDK_KEY_Up: {
            char entry_text[257];
            if (browse_run_history(&app->run_mode, -1, entry_text, sizeof(entry_text))) {
                set_run_entry_text(app, entry_text);
            }
            return TRUE;
        }

        case GDK_KEY_Down: {
            char entry_text[257];
            if (browse_run_history(&app->run_mode, 1, entry_text, sizeof(entry_text))) {
                set_run_entry_text(app, entry_text);
            }
            return TRUE;
        }

        case GDK_KEY_Tab:
        case GDK_KEY_ISO_Left_Tab:
            return FALSE;

        default:
            return FALSE;
    }
}
