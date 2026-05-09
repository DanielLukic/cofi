#include "prefix_tabs.h"

#include "cofi_modal.h"
#include "cofi_tab_provider.h"
#include "command_mode.h"
#include "display.h"
#include "log.h"
#include "selection.h"
#include "tab_switching.h"

#include <gtk/gtk.h>

static gboolean get_tab_claim(char prefix, TabMode *target_tab) {
    if (!target_tab) {
        return FALSE;
    }

    switch (prefix) {
        case '$':
        case '\\':
            *target_tab = TAB_APPS;
            return TRUE;
        case '>':
            *target_tab = TAB_WINDOWS;
            return TRUE;
        default:
            return FALSE;
    }
}

void clear_prefix_tab_claim(AppData *app) {
    if (!app) {
        return;
    }

    app->active_prefix_claim = '\0';
}

gboolean cofi_is_prefix_char(char c) {
    if (c == ':') return TRUE;
    if (cofi_get_provider_for_prefix(c) != NULL) return TRUE;
    TabMode dummy;
    return get_tab_claim(c, &dummy);
}

void cofi_dispatch_prefix(AppData *app, char c) {
    if (!app) return;

    /* : → command mode */
    if (c == ':') {
        app->prefix_origin_tab = app->current_tab;
        app->active_prefix_claim = ':';
        enter_command_mode(app);
        return;
    }

    /* set origin unconditionally before any tier */
    app->prefix_origin_tab = app->current_tab;
    app->active_prefix_claim = c;

    /* provider prefix → modal (e.g. !, =) */
    const CofiTabProvider *provider = cofi_get_provider_for_prefix(c);
    if (provider) {
        cofi_enter_modal(app, provider);
        return;
    }

    /* tab claim → tab switch (e.g. $, \\, >) */
    TabMode claimed_tab;
    if (get_tab_claim(c, &claimed_tab)) {
        if (claimed_tab == TAB_APPS)
            app->apps_mode = (c == '$') ? APPS_MODE_PATH : APPS_MODE_DEFAULT;
        if (app->current_tab == claimed_tab) {
            /* same-tab toggle: clear entry, reset selection, refresh */
            app->suppress_entry_change = TRUE;
            gtk_entry_set_text(GTK_ENTRY(app->entry), "");
            app->suppress_entry_change = FALSE;
            if (claimed_tab == TAB_APPS) {
                filter_apps(app, "");
            }
            reset_selection(app);
            update_display(app);
        } else {
            app->suppress_entry_change = TRUE;
            switch_to_tab(app, claimed_tab);
            app->suppress_entry_change = FALSE;
        }
        if (app->mode_indicator)
            gtk_label_set_text(GTK_LABEL(app->mode_indicator), (char[2]){c, '\0'});
        return;
    }
}

void apply_prefix_tab_claim(AppData *app, const char *entry_text) {
    if (!app || !entry_text || app->command_mode.state != CMD_MODE_NORMAL) {
        return;
    }

    if (entry_text[0] == '\0') {
        if (app->active_prefix_claim != '\0') {
            app->suppress_entry_change = TRUE;
            switch_to_tab(app, app->prefix_origin_tab);
            app->suppress_entry_change = FALSE;
            clear_prefix_tab_claim(app);
        }
        return;
    }

    if (cofi_is_prefix_char(entry_text[0])) {
        if (entry_text[0] == ':') {
            /* paste path: strip ':' and set remainder as command text */
            const char *rest = entry_text + 1;
            if (app->active_prefix_claim == '\0') {
                app->prefix_origin_tab = app->current_tab;
                app->active_prefix_claim = ':';
            }
            enter_command_mode(app);
            if (app->entry)
                gtk_entry_set_text(GTK_ENTRY(app->entry), rest);
            return;
        }
        if (app->active_prefix_claim == '\0') {
            app->prefix_origin_tab = app->current_tab;
            app->active_prefix_claim = entry_text[0];
        }
        cofi_dispatch_prefix(app, entry_text[0]);
        return;
    }

    clear_prefix_tab_claim(app);
}
