#include "calc_mode.h"

#include "calc.h"
#include "display.h"
#include "log.h"
#include "prefix_tabs.h"
#include "selection.h"
#include "tab_switching.h"

void enter_calc_mode(AppData *app) {
    if (!app || !app->entry) return;

    app->command_mode.state = CMD_MODE_CALC;
    app->calc_mode.suppress_entry_change = TRUE;

    surface_tab(app, TAB_CALC);

    if (app->mode_indicator) {
        gtk_label_set_text(GTK_LABEL(app->mode_indicator), "=");
    }

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

    if (app->mode_indicator) {
        gtk_label_set_text(GTK_LABEL(app->mode_indicator), ">");
    }

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
            char result[CALC_RESULT_LEN];
            gboolean ok = calc_eval(&app->calc_mode, text, result);
            if (ok) {
                GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
                gtk_clipboard_set_text(clipboard, app->calc_mode.last_result, -1);
                log_info("calc: copied '%s' to clipboard", app->calc_mode.last_result);
            }
            app->calc_mode.suppress_entry_change = TRUE;
            gtk_entry_set_text(GTK_ENTRY(app->entry), "");
            app->calc_mode.suppress_entry_change = FALSE;
            reset_selection(app);
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
