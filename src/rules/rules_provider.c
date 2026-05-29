#include "rules/rules_provider.h"

#include "providers/cofi_tab_provider.h"
#include "commands/command_mode.h"
#include "commands/command_registry.h"
#include "matching/match.h"
#include "ui/overlay_manager.h"
#include "matching/overlay_pattern.h"
#include "rules/rules_replay.h"
#include "core/selection/selection.h"
#include "ui/tab_switching.h"
#include "ui/dynamic_display.h"
#include "matching/match_entry.h"

#include <stdio.h>
#include <string.h>

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

static const MatchEntry *entry_for_rule(const AppData *app, const Rule *rule) {
    if (!app || !rule || rule->match_id <= 0) {
        return NULL;
    }
    int idx = match_entry_find_index_by_match_id(&app->matching, rule->match_id);
    if (idx < 0) {
        return NULL;
    }
    return &app->matching.entries[idx];
}

static bool rule_applied_window_is_live(const AppData *app, const Rule *rule) {
    if (!app || !rule || rule->applied == 0) return false;
    for (int i = 0; i < app->window_count; i++) {
        if (app->windows[i].id == rule->applied) return true;
    }
    return false;
}

static void format_rule_flags(AppData *app, const Rule *rule, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!rule) {
        g_strlcpy(out, "-", out_size);
        return;
    }

    if (rule->once) {
        if (rule_applied_window_is_live(app, rule)) {
            g_strlcat(out, "Ø", out_size);
        } else {
            g_strlcat(out, "O", out_size);
        }
    }
    if (rule->new_only) {
        g_strlcat(out, "N", out_size);
    }
    if (out[0] == '\0') {
        g_strlcpy(out, "-", out_size);
    }
}

static void rules_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    Rule *rule = rule_at_row(app, raw_idx);
    if (!rule) {
        out->cell_count = 1;
        out->cells[0].text = "No rules found";
        out->row_flags = 0;
        return;
    }

    bool show_tags = app && app->config.rules_show_all_tags;
    out->cell_count = show_tags ? 4 : 3;
    const MatchEntry *entry = entry_for_rule(app, rule);
    static char pattern_buf[MAX_RULES][MAX_TITLE_LEN + 32];
    static char flags_buf[MAX_RULES][8];
    int row = raw_idx;
    if (row < 0) row = 0;
    if (row >= MAX_RULES) row = MAX_RULES - 1;
    format_rule_flags(app, rule, flags_buf[row], sizeof(flags_buf[row]));
    if (entry) {
        snprintf(pattern_buf[row], sizeof(pattern_buf[row]), "%s",
                 entry->original_title);
    } else {
        snprintf(pattern_buf[row], sizeof(pattern_buf[row]), "%s (orphan)", rule->pattern);
    }
    out->cells[0].text = flags_buf[row];
    out->cells[0].width_hint = 6;
    out->cells[1].text = pattern_buf[row];
    out->cells[1].width_hint = 40;
    out->cells[2].text = rule->commands;
    out->cells[2].width_hint = 64;
    if (show_tags) {
        /* selection "> " + flags + pattern + tag + 3 cell separators */
        const int fixed_total = 2 + 6 + 40 + 12 + 3;
        int command_w = get_display_columns(app) - fixed_total;
        if (command_w < 20) {
            command_w = 20;
        }
        out->cells[2].width_hint = command_w;
        out->cells[3].text = rule->tag[0] ? rule->tag : "-";
        out->cells[3].width_hint = 12;
    }
    out->row_flags = COFI_ROW_ACTIONABLE;
}

static const char *rules_match_string(AppData *app, int raw_idx) {
    Rule *rule = rule_at_row(app, raw_idx);
    static char searchable[600];
    if (!rule) return "";
    const MatchEntry *entry = entry_for_rule(app, rule);
    const char *pattern = entry ? entry->original_title : rule->pattern;
    snprintf(searchable, sizeof(searchable), "%s %s",
             pattern, rule->commands);
    return searchable;
}

static const char *rules_row_identity(AppData *app, int raw_idx) {
    Rule *rule = rule_at_row(app, raw_idx);
    static char identity[700];
    if (!rule) return "";
    snprintf(identity, sizeof(identity), "rule:%d:%s",
             rule->match_id, rule->commands);
    return identity;
}

static void format_rules_context(const Rule *rule, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!rule) return;
    const size_t max_cmd = 60;
    size_t len = strlen(rule->commands);
    if (len <= max_cmd) {
        g_snprintf(out, out_size, "Commands: %s", rule->commands);
        return;
    }
    char truncated[MAX_COMMANDS_LEN];
    g_strlcpy(truncated, rule->commands, sizeof(truncated));
    if (max_cmd + 1 < sizeof(truncated)) {
        truncated[max_cmd] = '\0';
    }
    g_snprintf(out, out_size, "Commands: %s...", truncated);
}

void filter_rules(AppData *app, const char *filter) {
    if (!app) return;

    app->filtered_rules_count = 0;

    if (!filter || !*filter) {
        for (int i = 0; i < app->rules_config.count; i++) {
            Rule *rule = &app->rules_config.rules[i];
            if (rule->tag[0] != '\0' && !app->config.rules_show_all_tags) continue;
            app->filtered_rules[app->filtered_rules_count] =
                *rule;
            app->filtered_rule_indices[app->filtered_rules_count] = i;
            app->filtered_rules_count++;
        }
        return;
    }

    for (int i = 0; i < app->rules_config.count; i++) {
        Rule *rule = &app->rules_config.rules[i];
        if (rule->tag[0] != '\0' && !app->config.rules_show_all_tags) continue;
        char searchable[600];
        const MatchEntry *entry = entry_for_rule(app, rule);
        const char *pattern = entry ? entry->original_title : rule->pattern;
        snprintf(searchable, sizeof(searchable), "%s %s",
                 pattern,
                 rule->commands);
        if (has_match(filter, searchable)) {
            app->filtered_rules[app->filtered_rules_count] =
                *rule;
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
        (event->keyval == GDK_KEY_p || event->keyval == GDK_KEY_P)) {
        Rule *selected = rules_selected_rule(app);
        int match_id = selected_match_id_for_pattern_edit(app);
        char context[96];
        format_rules_context(selected, context, sizeof(context));
        show_pattern_edit_overlay(app, match_id, context);
        return TRUE;
    }

    if ((event->state & GDK_CONTROL_MASK) &&
        (event->keyval == GDK_KEY_d || event->keyval == GDK_KEY_D)) {
        int rule_index = rules_selected_config_index(app);
        if (rule_index < 0) {
            return FALSE;
        }
        show_rule_delete_overlay(app, rule_index);
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

    if ((event->state & GDK_CONTROL_MASK) &&
        (event->keyval == GDK_KEY_o || event->keyval == GDK_KEY_O)) {
        int rule_index = rules_selected_config_index(app);
        if (rule_index < 0 || rule_index >= app->rules_config.count) {
            return FALSE;
        }
        rule_toggle_once(&app->rules_config.rules[rule_index]);
        save_rules_config(&app->rules_config, &app->matching);
        const char *query = app->entry ? gtk_entry_get_text(GTK_ENTRY(app->entry)) : "";
        preserve_selection(app);
        rules_on_query_changed(app, query);
        restore_selection(app);
        update_display(app);
        return TRUE;
    }

    if ((event->state & GDK_CONTROL_MASK) &&
        (event->keyval == GDK_KEY_n || event->keyval == GDK_KEY_N)) {
        int rule_index = rules_selected_config_index(app);
        if (rule_index < 0 || rule_index >= app->rules_config.count) {
            return FALSE;
        }
        rule_toggle_new_only(&app->rules_config.rules[rule_index]);
        save_rules_config(&app->rules_config, &app->matching);
        const char *query = app->entry ? gtk_entry_get_text(GTK_ENTRY(app->entry)) : "";
        preserve_selection(app);
        rules_on_query_changed(app, query);
        restore_selection(app);
        update_display(app);
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
        "Shortcuts: Ctrl+A=Add  Ctrl+E=Edit commands  Ctrl+P=Edit pattern  Ctrl+D=Delete  Ctrl+O=once  Ctrl+N=new  Ctrl+X=Replay rule  Ctrl+Shift+X=Replay all";
    s_rules_provider_id = cofi_register_tab_provider(&s_rules_provider);
    if (s_rules_provider_id >= 0) {
        cofi_register_command(&s_rules_command);
    }
}
