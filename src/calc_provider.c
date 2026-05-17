#include "calc_provider.h"

#include "calc.h"
#include "command_mode.h"
#include "command_registry.h"
#include "cofi_tab_provider.h"
#include "cofi_modal.h"
#include "app_data.h"
#include "log.h"

#include <gtk/gtk.h>
#include <string.h>

/* ---- adapter functions ---- */

static int calc_row_count(AppData *app) {
    return app->calc_mode.count;
}

static void calc_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    CalcEntry *e = &app->calc_mode.entries[raw_idx];
    static char expr_buf[CALC_EXPR_LEN + 4];
    snprintf(expr_buf, sizeof(expr_buf), "= %s", e->expr);

    out->cell_count = 2;
    out->cells[0].text = e->result;
    out->cells[0].width_hint = 24;
    out->cells[0].align = 0;   /* left, matches original %-20s */
    out->cells[1].text = expr_buf;
    out->cells[1].width_hint = 0;
    out->cells[1].align = 0;
    out->row_flags = COFI_ROW_ACTIONABLE;
}

static const char *calc_match_string(AppData *app, int raw_idx) {
    return app->calc_mode.entries[raw_idx].expr;
}

static const char *calc_row_identity(AppData *app, int raw_idx) {
    return app->calc_mode.entries[raw_idx].expr;
}

static CofiActionStatus calc_on_enter_pressed(AppData *app, int filtered_idx,
                                               int raw_idx,
                                               const char *entry_text,
                                               int modifier_state) {
    (void)filtered_idx;
    (void)raw_idx;
    (void)modifier_state;
    if (!entry_text || entry_text[0] == '\0') return COFI_NO_OP;

    char result[CALC_RESULT_LEN];
    gboolean ok = calc_eval(&app->calc_mode, entry_text, result);
    if (ok) {
        GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        gtk_clipboard_set_text(clipboard, app->calc_mode.last_result, -1);
        log_info("calc: copied '%s' to clipboard", app->calc_mode.last_result);
    }
    return COFI_HANDLED_KEEP;
}

static CofiActionStatus calc_on_command_args(AppData *app, const char *args) {
    if (!args || args[0] == '\0') return COFI_NO_OP;
    char result[CALC_RESULT_LEN];
    calc_eval(&app->calc_mode, args, result);
    return COFI_HANDLED_KEEP;
}

/* ---- on_enter: sets placeholder text for the calc tab ---- */

static void calc_on_enter(AppData *app) {
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "expression");
}

/* ---- provider registration ---- */

static CofiTabProvider s_calc_provider;

static gboolean calc_command_handler(AppData *app,
                                     WindowInfo *window __attribute__((unused)),
                                     const char *args) {
    exit_command_mode(app);

    if (app) {
        app->prefix_origin_tab = app->current_tab;
        app->active_prefix_claim = '=';
    }
    cofi_enter_modal(app, &s_calc_provider);

    if (args && args[0] != '\0') {
        calc_on_command_args(app, args);
    }
    return FALSE;
}

static const CommandSpec s_calc_command = {
    .primary = "calc",
    .aliases = {"ca", NULL},
    .owner_provider_id = "calc",
    .handler = calc_command_handler,
    .description = "Switch to calculator",
    .help_format = "calc, ca",
    .keeps_open_on_hotkey_auto = 1
};

void calc_provider_register(void) {
    cofi_init_provider_defaults(&s_calc_provider);
    s_calc_provider.tab_mode     = TAB_CALC;
    s_calc_provider.id           = "calc";
    s_calc_provider.display_name = "CALC";
    s_calc_provider.prefix_char  = '=';
    s_calc_provider.required     = 0;
    s_calc_provider.modal_policy = COFI_MODAL_CLEAR_THEN_RETURN;
    s_calc_provider.on_enter          = calc_on_enter;
    s_calc_provider.row_count         = calc_row_count;
    s_calc_provider.format_row        = calc_format_row;
    s_calc_provider.match_string      = calc_match_string;
    s_calc_provider.row_identity      = calc_row_identity;
    s_calc_provider.on_enter_pressed  = calc_on_enter_pressed;
    s_calc_provider.on_command_args   = calc_on_command_args;
    if (cofi_register_tab_provider(&s_calc_provider) >= 0) {
        cofi_register_command(&s_calc_command);
    }
}
