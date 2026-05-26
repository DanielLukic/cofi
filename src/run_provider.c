#include "run_provider.h"

#include "app_data.h"
#include "command_mode.h"
#include "command_registry.h"
#include "cofi_tab_provider.h"
#include "cofi_modal.h"
#include "daemon_socket.h"
#include "detach_launch.h"
#include "log.h"
#include "run_mode.h"
#include "types.h"
#include "window_lifecycle.h"

#include <gtk/gtk.h>
#include <string.h>

static int run_row_count(AppData *app) {
    return app->run_mode.history_count;
}

static void run_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    if (raw_idx < 0 || raw_idx >= app->run_mode.history_count) {
        out->cell_count = 1;
        out->cells[0].text = "";
        return;
    }
    out->cell_count = 1;
    out->cells[0].text = app->run_mode.history[raw_idx];
    out->row_flags = COFI_ROW_ACTIONABLE;
}

static const char *run_match_string(AppData *app, int raw_idx) {
    if (raw_idx < 0 || raw_idx >= app->run_mode.history_count) return "";
    return app->run_mode.history[raw_idx];
}

static const char *run_row_identity(AppData *app, int raw_idx) {
    if (raw_idx < 0 || raw_idx >= app->run_mode.history_count) return "";
    return app->run_mode.history[raw_idx];
}

static void run_on_selection_changed(AppData *app, int filtered_idx) {
    if (!app || !app->entry) return;
    if (filtered_idx < 0 || filtered_idx >= app->run_mode.history_count) {
        app->suppress_entry_change = TRUE;
        gtk_entry_set_text(GTK_ENTRY(app->entry), "");
        app->suppress_entry_change = FALSE;
        return;
    }
    app->suppress_entry_change = TRUE;
    gtk_entry_set_text(GTK_ENTRY(app->entry), app->run_mode.history[filtered_idx]);
    gtk_editable_set_position(GTK_EDITABLE(app->entry), -1);
    app->suppress_entry_change = FALSE;
}

static CofiActionStatus run_on_enter_pressed(AppData *app, int filtered_idx,
                                              int raw_idx,
                                              const char *entry_text,
                                              int modifier_state) {
    (void)filtered_idx;
    (void)modifier_state;
    if (entry_text && entry_text[0] != '\0') {
        char command[256];
        if (extract_run_command(entry_text, command, sizeof(command))) {
            if (detach_launch_shell(command)) {
                add_run_history_entry(&app->run_mode, command);
                log_info("USER: run: launched '%s'", command);
                return COFI_HANDLED_HIDE;
            }
        }
        return COFI_NO_OP;
    }
    if (raw_idx >= 0 && raw_idx < app->run_mode.history_count) {
        if (detach_launch_shell(app->run_mode.history[raw_idx])) {
            log_info("USER: run: re-launched history[%d]", raw_idx);
            return COFI_HANDLED_HIDE;
        }
    }
    return COFI_NO_OP;
}

static CofiActionStatus run_on_command_args(AppData *app, const char *args) {
    if (!args || args[0] == '\0') return COFI_NO_OP;
    char command[256];
    if (extract_run_command(args, command, sizeof(command))) {
        if (detach_launch_shell(command)) {
            add_run_history_entry(&app->run_mode, command);
            return COFI_HANDLED_HIDE;
        }
    }
    return COFI_NO_OP;
}

static void run_on_enter(AppData *app) {
    if (!app || !app->entry) return;
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "command");
}

static CofiTabProvider s_run_provider;
static int s_run_provider_id = -1;

static const CofiTabProvider *run_registered_provider(void) {
    return cofi_get_provider(s_run_provider_id);
}

static gboolean run_command_handler(AppData *app,
                                    WindowInfo *window __attribute__((unused)),
                                    const char *args) {
    const CofiTabProvider *provider = run_registered_provider();
    if (!provider) return FALSE;

    exit_command_mode(app);

    if (args && args[0] != '\0') {
        CofiActionStatus status = run_on_command_args(app, args);
        if (status == COFI_HANDLED_HIDE) {
            hide_window(app);
        }
        return FALSE;
    }

    if (app) {
        app->prefix_origin_tab = app->current_tab;
        app->active_prefix_claim = '!';
    }
    cofi_enter_modal(app, provider);
    return FALSE;
}

static const CommandSpec s_run_command = {
    .primary = "run",
    .aliases = {"r", NULL},
    .owner_provider_id = "run",
    .handler = run_command_handler,
    .description = "Switch to run mode",
    .help_format = "run, r",
    .keeps_open_on_hotkey_auto = 1
};

void run_provider_register(void) {
    cofi_init_provider_defaults(&s_run_provider);
    s_run_provider_id = -1;
    s_run_provider.tab_mode                = COFI_PROVIDER_DYNAMIC_TAB;
    s_run_provider.id                      = "run";
    s_run_provider.display_name            = "RUN";
    s_run_provider.shortcut_hint           = NULL;
    s_run_provider.prefix_char             = '!';
    s_run_provider.delegate_opcode         = COFI_OPCODE_RUN;
    s_run_provider.hotkey_mode_claim       = COFI_PROVIDER_HOTKEY_MODE(SHOW_MODE_RUN);
    s_run_provider.required                = 0;
    s_run_provider.modal_policy            = COFI_MODAL_CLEAR_THEN_RETURN;
    s_run_provider.hidden_by_default       = 1;
    s_run_provider.initial_selection_index = 0;
    s_run_provider.on_enter                = run_on_enter;
    s_run_provider.row_count               = run_row_count;
    s_run_provider.format_row              = run_format_row;
    s_run_provider.match_string            = run_match_string;
    s_run_provider.row_identity            = run_row_identity;
    s_run_provider.on_selection_changed    = run_on_selection_changed;
    s_run_provider.on_enter_pressed        = run_on_enter_pressed;
    s_run_provider.on_command_args         = run_on_command_args;
    s_run_provider_id = cofi_register_tab_provider(&s_run_provider);
    if (s_run_provider_id >= 0) {
        cofi_register_command(&s_run_command);
    }
}
