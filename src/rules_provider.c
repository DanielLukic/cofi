#include "rules_provider.h"

#include "cofi_tab_provider.h"
#include "command_mode.h"
#include "command_registry.h"
#include "match.h"
#include "overlay_manager.h"
#include "rules_replay.h"
#include "selection.h"
#include "tab_switching.h"

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
static int s_rules_provider_id = -1;

TabMode rules_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_rules_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

gboolean handle_rules_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != rules_tab_mode()) {
        return FALSE;
    }

    if ((event->state & GDK_CONTROL_MASK) &&
        (event->keyval == GDK_KEY_a || event->keyval == GDK_KEY_A)) {
        show_overlay(app, OVERLAY_RULE_ADD, NULL);
        return TRUE;
    }

    if ((event->state & GDK_CONTROL_MASK) &&
        (event->keyval == GDK_KEY_e || event->keyval == GDK_KEY_E)) {
        if (!rules_selected_rule(app)) {
            return FALSE;
        }
        show_overlay(app, OVERLAY_RULE_EDIT, NULL);
        return TRUE;
    }

    if ((event->state & GDK_CONTROL_MASK) &&
        (event->keyval == GDK_KEY_d || event->keyval == GDK_KEY_D)) {
        int rule_index = rules_selected_config_index(app);
        if (rule_index < 0) {
            return FALSE;
        }
        app->rules_delete.pending_delete = TRUE;
        app->rules_delete.rule_index = rule_index;
        show_overlay(app, OVERLAY_RULE_DELETE, NULL);
        return TRUE;
    }

    if ((event->state & GDK_CONTROL_MASK) &&
        (event->state & GDK_SHIFT_MASK) &&
        (event->keyval == GDK_KEY_x || event->keyval == GDK_KEY_X)) {
        replay_all_rules_against_open_windows(app);
        return TRUE;
    }

    if ((event->state & GDK_CONTROL_MASK) &&
        (event->keyval == GDK_KEY_x || event->keyval == GDK_KEY_X)) {
        if (!rules_selected_rule(app)) {
            return FALSE;
        }
        replay_selected_filtered_rule(app);
        return TRUE;
    }

    return FALSE;
}

static gboolean rules_command_handler(AppData *app,
                                      WindowInfo *window __attribute__((unused)),
                                      const char *args __attribute__((unused))) {
    if (!app) return FALSE;

    exit_command_mode(app);
    app->prefix_origin_tab = app->current_tab;
    surface_tab(app, rules_tab_mode());
    return FALSE;
}

static const CommandSpec s_rules_command = {
    .primary = "rules",
    .aliases = {"rs", NULL},
    .owner_provider_id = "rules",
    .handler = rules_command_handler,
    .description = "Switch to Rules tab",
    .help_format = "rules, rs",
    .keeps_open_on_hotkey_auto = 1
};

void rules_provider_register(void) {
    cofi_init_provider_defaults(&s_rules_provider);
    s_rules_provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    s_rules_provider.id = "rules";
    s_rules_provider.display_name = "RULES";
    s_rules_provider.required = 0;
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
    s_rules_provider_id = cofi_register_tab_provider(&s_rules_provider);
    if (s_rules_provider_id >= 0) {
        cofi_register_command(&s_rules_command);
    }
}
