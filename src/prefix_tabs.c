#include "prefix_tabs.h"

#include "calc_mode.h"
#include "command_mode.h"
#include "run_mode.h"

static gboolean get_tab_claim(char prefix, TabMode *target_tab) {
    if (!target_tab) {
        return FALSE;
    }

    switch (prefix) {
        case '$':
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

void apply_prefix_tab_claim(AppData *app, const char *entry_text) {
    if (!app || !entry_text || app->command_mode.state != CMD_MODE_NORMAL) {
        return;
    }

    if (entry_text[0] == '\0') {
        if (app->active_prefix_claim != '\0') {
            app->current_tab = app->prefix_origin_tab;
            clear_prefix_tab_claim(app);
        }
        return;
    }

    if (entry_text[0] == ':') {
        if (app->active_prefix_claim == '\0') {
            app->prefix_origin_tab = app->current_tab;
            app->active_prefix_claim = ':';
        }
        enter_command_mode(app);
        if (entry_text[1] != '\0') {
            gtk_entry_set_text(GTK_ENTRY(app->entry), entry_text + 1);
        }
        return;
    }

    if (entry_text[0] == '=') {
        if (app->active_prefix_claim == '\0') {
            app->prefix_origin_tab = app->current_tab;
            app->active_prefix_claim = '=';
        }
        enter_calc_mode(app);
        return;
    }

    if (entry_text[0] == '!') {
        if (app->active_prefix_claim == '\0') {
            app->prefix_origin_tab = app->current_tab;
            app->active_prefix_claim = '!';
        }
        enter_run_mode(app, entry_text);
        return;
    }

    TabMode claimed_tab;
    if (get_tab_claim(entry_text[0], &claimed_tab)) {
        if (app->active_prefix_claim == '\0') {
            app->prefix_origin_tab = app->current_tab;
        }
        app->active_prefix_claim = entry_text[0];
        app->current_tab = claimed_tab;
        return;
    }

    clear_prefix_tab_claim(app);
}
