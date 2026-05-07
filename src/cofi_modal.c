#include "cofi_modal.h"
#include "app_data.h"
#include "display.h"
#include "prefix_tabs.h"
#include "selection.h"
#include "tab_switching.h"
#include "log.h"

#include <gtk/gtk.h>

void cofi_enter_modal(AppData *app, const CofiTabProvider *provider) {
    if (!app || !app->entry || !provider) return;

    app->command_mode.state = CMD_MODE_MODAL;
    /* spike: suppress_entry_change lives in calc_mode; generic modal uses it as a shared mutex */
    app->suppress_entry_change = TRUE;
    surface_tab(app, (TabMode)provider->tab_mode);

    if (app->mode_indicator) {
        char indicator[2] = {provider->prefix_char ? provider->prefix_char : '>', '\0'};
        gtk_label_set_text(GTK_LABEL(app->mode_indicator), indicator);
    }

    gtk_entry_set_text(GTK_ENTRY(app->entry), "");
    app->suppress_entry_change = FALSE;

    log_info("USER: Entered modal (%s)", provider->id ? provider->id : "?");
}

void cofi_exit_modal(AppData *app) {
    if (!app || !app->entry) return;
    if (app->command_mode.state != CMD_MODE_MODAL) return;

    TabMode origin = app->prefix_origin_tab;
    const CofiTabProvider *p = cofi_get_provider_for_tab(app->current_tab);

    app->command_mode.state = CMD_MODE_NORMAL;
    if (p)
        app->tab_visibility[(TabMode)p->tab_mode] = TAB_VIS_HIDDEN;
    clear_prefix_tab_claim(app);

    if (app->mode_indicator)
        gtk_label_set_text(GTK_LABEL(app->mode_indicator), ">");

    app->suppress_entry_change = TRUE;
    switch_to_tab(app, origin);
    app->suppress_entry_change = FALSE;

    log_info("USER: Exited modal");
}

gboolean cofi_handle_modal_key(AppData *app, GdkEventKey *event) {
    if (!app || app->command_mode.state != CMD_MODE_MODAL) return FALSE;
    const CofiTabProvider *p = cofi_get_provider_for_tab(app->current_tab);
    if (!p) return FALSE;

    switch (event->keyval) {
        case GDK_KEY_Escape: {
            const char *text = gtk_entry_get_text(GTK_ENTRY(app->entry));
            if (p->modal_policy == COFI_MODAL_CLEAR_THEN_RETURN && text[0] != '\0') {
                app->suppress_entry_change = TRUE;
                gtk_entry_set_text(GTK_ENTRY(app->entry), "");
                app->suppress_entry_change = FALSE;
                update_display(app);
                return TRUE;
            }
            cofi_exit_modal(app);
            return TRUE;
        }
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter: {
            const char *text = gtk_entry_get_text(GTK_ENTRY(app->entry));
            if (!text || text[0] == '\0') return TRUE;
            if (p->on_enter_pressed)
                p->on_enter_pressed(app, 0, 0, text);
            app->suppress_entry_change = TRUE;
            gtk_entry_set_text(GTK_ENTRY(app->entry), "");
            app->suppress_entry_change = FALSE;
            int count = p->row_count ? p->row_count(app) : 0;
            app->selection.provider_index = p->initial_selection_index;
            if (count > 0 && app->selection.provider_index >= count) {
                app->selection.provider_index = count - 1;
            } else if (app->selection.provider_index < 0 || count <= 0) {
                app->selection.provider_index = 0;
            }
            update_scroll_position(app);
            update_display(app);
            return TRUE;
        }
        case GDK_KEY_Tab:
        case GDK_KEY_ISO_Left_Tab:
            cofi_exit_modal(app);
            return FALSE;
        default:
            return FALSE;
    }
}
