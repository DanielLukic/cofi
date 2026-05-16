#include "rules_provider.h"

#include "cofi_tab_provider.h"
#include "key_handler_tabs.h"
#include "match.h"
#include "selection.h"

#include <gtk/gtk.h>
#include <stdio.h>

static int rules_row_count(AppData *app) {
    if (!app) return 0;
    return app->filtered_rules_count > 0 ? app->filtered_rules_count : 1;
}

static Rule *rule_at_row(AppData *app, int raw_idx) {
    if (!app || raw_idx < 0 || raw_idx >= app->filtered_rules_count) {
        return NULL;
    }
    return &app->filtered_rules[raw_idx];
}

static void rules_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    Rule *rule = rule_at_row(app, raw_idx);
    if (!rule) {
        out->cell_count = 1;
        out->cells[0].text = "No rules found";
        out->row_flags = 0;
        return;
    }

    out->cell_count = 2;
    out->cells[0].text = rule->pattern;
    out->cells[0].width_hint = 40;
    out->cells[1].text = rule->commands;
    out->cells[1].width_hint = 64;
    out->row_flags = COFI_ROW_ACTIONABLE;
}

static const char *rules_match_string(AppData *app, int raw_idx) {
    Rule *rule = rule_at_row(app, raw_idx);
    static char searchable[600];
    if (!rule) return "";
    snprintf(searchable, sizeof(searchable), "%s %s",
             rule->pattern, rule->commands);
    return searchable;
}

static const char *rules_row_identity(AppData *app, int raw_idx) {
    Rule *rule = rule_at_row(app, raw_idx);
    static char identity[700];
    if (!rule) return "";
    snprintf(identity, sizeof(identity), "rule:%s:%s",
             rule->pattern, rule->commands);
    return identity;
}

void filter_rules(AppData *app, const char *filter) {
    if (!app) return;

    app->filtered_rules_count = 0;

    if (!filter || !*filter) {
        for (int i = 0; i < app->rules_config.count; i++) {
            app->filtered_rules[app->filtered_rules_count] =
                app->rules_config.rules[i];
            app->filtered_rule_indices[app->filtered_rules_count] = i;
            app->filtered_rules_count++;
        }
        return;
    }

    for (int i = 0; i < app->rules_config.count; i++) {
        char searchable[600];
        snprintf(searchable, sizeof(searchable), "%s %s",
                 app->rules_config.rules[i].pattern,
                 app->rules_config.rules[i].commands);
        if (has_match(filter, searchable)) {
            app->filtered_rules[app->filtered_rules_count] =
                app->rules_config.rules[i];
            app->filtered_rule_indices[app->filtered_rules_count] = i;
            app->filtered_rules_count++;
        }
    }
}

static void rules_on_enter(AppData *app) {
    if (!app) return;
    if (app->entry) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry),
                                       "Type to filter rules...");
    }
    filter_rules(app, "");
}

static void rules_on_query_changed(AppData *app, const char *query) {
    filter_rules(app, query);
    reset_selection(app);
}

Rule *rules_selected_rule(AppData *app) {
    if (!app || app->filtered_rules_count <= 0) return NULL;

    int idx = app->selection.provider_index;
    if (idx < 0) idx = 0;
    if (idx >= app->filtered_rules_count) idx = app->filtered_rules_count - 1;
    app->selection.provider_index = idx;

    return &app->filtered_rules[idx];
}

int rules_selected_config_index(AppData *app) {
    if (!rules_selected_rule(app)) return -1;
    return app->filtered_rule_indices[app->selection.provider_index];
}

void rules_select_config_index(AppData *app, int config_index) {
    if (!app || config_index < 0) return;

    for (int i = 0; i < app->filtered_rules_count; i++) {
        if (app->filtered_rule_indices[i] == config_index) {
            app->selection.provider_index = i;
            return;
        }
    }
    app->selection.provider_index = 0;
}

static CofiTabProvider s_rules_provider;
static const char *const s_rules_aliases[] = {"rl", NULL};

void rules_provider_register(void) {
    cofi_init_provider_defaults(&s_rules_provider);
    s_rules_provider.tab_mode = TAB_RULES;
    s_rules_provider.id = "rules";
    s_rules_provider.display_name = "RULES";
    s_rules_provider.primary_cmd = "rules";
    s_rules_provider.aliases = s_rules_aliases;
    s_rules_provider.hidden_by_default = 1;
    s_rules_provider.initial_selection_index = 0;
    s_rules_provider.row_count = rules_row_count;
    s_rules_provider.format_row = rules_format_row;
    s_rules_provider.match_string = rules_match_string;
    s_rules_provider.row_identity = rules_row_identity;
    s_rules_provider.on_enter = rules_on_enter;
    s_rules_provider.on_query_changed = rules_on_query_changed;
    s_rules_provider.handle_key = handle_rules_tab_keys;
    s_rules_provider.shortcut_hint =
        "Shortcuts: Ctrl+A=Add  Ctrl+E=Edit  Ctrl+D=Delete  Ctrl+X=Replay rule  Ctrl+Shift+X=Replay all";
    cofi_register_tab_provider(&s_rules_provider);
}
