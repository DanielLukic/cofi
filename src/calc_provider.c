#include "calc_mode.h"
#include "calc_provider.h"

#include "calc.h"
#include "cofi_tab_provider.h"
#include "app_data.h"
#include "display.h"
#include "log.h"
#include "prefix_tabs.h"
#include "selection.h"
#include "tab_switching.h"

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
    out->cells[0].width_hint = 20;
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
                                               const char *entry_text) {
    (void)filtered_idx;
    (void)raw_idx;
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

/* ---- provider registration ---- */

static const char *const s_calc_aliases[] = {"ca", NULL};
static CofiTabProvider s_calc_provider;

void calc_provider_register(void) {
    cofi_init_provider_defaults(&s_calc_provider);
    s_calc_provider.tab_mode     = TAB_CALC;
    s_calc_provider.id           = "calc";
    s_calc_provider.display_name = "CALC";
    s_calc_provider.primary_cmd  = "calc";
    s_calc_provider.aliases      = s_calc_aliases;
    s_calc_provider.prefix_char  = '=';
    s_calc_provider.modal_policy = COFI_MODAL_CLEAR_THEN_RETURN;
    s_calc_provider.row_count    = calc_row_count;
    s_calc_provider.format_row   = calc_format_row;
    s_calc_provider.match_string  = calc_match_string;
    s_calc_provider.row_identity  = calc_row_identity;
    s_calc_provider.on_enter_pressed = calc_on_enter_pressed;
    s_calc_provider.on_command_args  = calc_on_command_args;
    cofi_register_tab_provider(&s_calc_provider);
}

/* ---- mode lifecycle (moved from calc_mode.c) ---- */

void enter_calc_mode(AppData *app) {
    if (!app || !app->entry) return;

    app->command_mode.state = CMD_MODE_CALC;
    app->calc_mode.suppress_entry_change = TRUE;

    surface_tab(app, TAB_CALC);

    if (app->mode_indicator)
        gtk_label_set_text(GTK_LABEL(app->mode_indicator), "=");

    gtk_entry_set_text(GTK_ENTRY(app->entry), "");
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "expression");
    app->calc_mode.suppress_entry_change = FALSE;

    log_info("USER: Entered calc mode");
}

void exit_calc_mode(AppData *app) {
    if (!app || !app->entry) return;
    if (app->command_mode.state != CMD_MODE_CALC) return;

    TabMode origin = app->prefix_origin_tab;
    app->command_mode.state = CMD_MODE_NORMAL;
    app->tab_visibility[TAB_CALC] = TAB_VIS_HIDDEN;
    clear_prefix_tab_claim(app);

    if (app->mode_indicator)
        gtk_label_set_text(GTK_LABEL(app->mode_indicator), ">");

    app->calc_mode.suppress_entry_change = TRUE;
    switch_to_tab(app, origin);
    app->calc_mode.suppress_entry_change = FALSE;

    log_info("USER: Exited calc mode");
}

gboolean handle_calc_key(GdkEventKey *event, AppData *app) {
    if (!app || app->command_mode.state != CMD_MODE_CALC) return FALSE;

    switch (event->keyval) {
        case GDK_KEY_Escape: {
            const char *text = gtk_entry_get_text(GTK_ENTRY(app->entry));
            if (text[0] != '\0') {
                app->calc_mode.suppress_entry_change = TRUE;
                gtk_entry_set_text(GTK_ENTRY(app->entry), "");
                app->calc_mode.suppress_entry_change = FALSE;
                update_display(app);
                return TRUE;
            }
            exit_calc_mode(app);
            return TRUE;
        }
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter: {
            const char *text = gtk_entry_get_text(GTK_ENTRY(app->entry));
            if (text[0] == '\0') return TRUE;
            calc_on_enter_pressed(app, 0, 0, text);
            app->calc_mode.suppress_entry_change = TRUE;
            gtk_entry_set_text(GTK_ENTRY(app->entry), "");
            app->calc_mode.suppress_entry_change = FALSE;
            if (app->calc_mode.count > 0)
                app->selection.calc_index = app->calc_mode.count - 1;
            update_scroll_position(app);
            update_display(app);
            return TRUE;
        }
        case GDK_KEY_Tab:
        case GDK_KEY_ISO_Left_Tab:
            exit_calc_mode(app);
            return FALSE;
        default:
            return FALSE;
    }
}
