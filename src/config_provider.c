#include "config_provider.h"

#include "cofi_tab_provider.h"
#include "config.h"
#include "match.h"
#include "selection.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

static int config_row_count(AppData *app) {
    if (!app) return 0;
    return app->filtered_config_count > 0 ? app->filtered_config_count : 1;
}

static ConfigEntry *config_at_row(AppData *app, int raw_idx) {
    if (!app || raw_idx < 0 || raw_idx >= app->filtered_config_count) {
        return NULL;
    }
    return &app->filtered_config[raw_idx];
}

static void config_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    ConfigEntry *entry = config_at_row(app, raw_idx);
    if (!entry) {
        out->cell_count = 1;
        out->cells[0].text = "No matching config options found";
        out->row_flags = 0;
        return;
    }

    out->cell_count = 2;
    out->cells[0].text = entry->key;
    out->cells[0].width_hint = 32;
    out->cells[1].text = entry->value;
    out->cells[1].width_hint = 60;
    out->row_flags = COFI_ROW_ACTIONABLE;
}

static const char *config_match_string(AppData *app, int raw_idx) {
    ConfigEntry *entry = config_at_row(app, raw_idx);
    static char searchable[256];
    if (!entry) return "";
    g_snprintf(searchable, sizeof(searchable), "%s %s",
               entry->key, entry->value);
    return searchable;
}

static const char *config_row_identity(AppData *app, int raw_idx) {
    ConfigEntry *entry = config_at_row(app, raw_idx);
    static char identity[96];
    if (!entry) return "";
    g_snprintf(identity, sizeof(identity), "config:%s", entry->key);
    return identity;
}

static void config_on_enter(AppData *app) {
    if (!app || !app->entry) return;
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry),
                                   "Type to filter config options...");
    filter_config(app, "");
}

static void config_on_query_changed(AppData *app, const char *query) {
    filter_config(app, query);
    reset_selection(app);
}

int config_entry_allows_edit(const ConfigEntry *entry) {
    return entry && (entry->type == CONFIG_TYPE_INT ||
                     entry->type == CONFIG_TYPE_STRING);
}

static const char *config_shortcut_hint(AppData *app) {
    ConfigEntry *entry = config_selected_entry(app);
    if (!entry) {
        return "Shortcuts: Ctrl+T=Toggle bool/enum  Ctrl+E=Edit value";
    }
    if (entry->type == CONFIG_TYPE_BOOL || entry->type == CONFIG_TYPE_ENUM) {
        return "Shortcuts: Ctrl+T=Cycle value";
    }
    return "Shortcuts: Ctrl+E=Edit value";
}

void filter_config(AppData *app, const char *filter) {
    if (!app) return;

    ConfigEntry all_entries[MAX_CONFIG_ENTRIES];
    int all_count = 0;
    build_config_entries(&app->config, all_entries, &all_count);

    app->filtered_config_count = 0;

    if (!filter || !*filter) {
        for (int i = 0; i < all_count; i++) {
            app->filtered_config[app->filtered_config_count++] = all_entries[i];
        }
        return;
    }

    for (int i = 0; i < all_count; i++) {
        char searchable[256];
        snprintf(searchable, sizeof(searchable), "%s %s",
                 all_entries[i].key, all_entries[i].value);
        if (has_match(filter, searchable)) {
            app->filtered_config[app->filtered_config_count++] = all_entries[i];
        }
    }
}

ConfigEntry *config_selected_entry(AppData *app) {
    if (!app || app->filtered_config_count <= 0) return NULL;

    int idx = app->selection.provider_index;
    if (idx < 0) idx = 0;
    if (idx >= app->filtered_config_count) idx = app->filtered_config_count - 1;
    app->selection.provider_index = idx;

    return &app->filtered_config[idx];
}

void config_select_key(AppData *app, const char *key) {
    if (!app || !key || key[0] == '\0') return;

    for (int i = 0; i < app->filtered_config_count; i++) {
        if (strcmp(app->filtered_config[i].key, key) == 0) {
            app->selection.provider_index = i;
            return;
        }
    }
    app->selection.provider_index = 0;
}

static CofiTabProvider s_config_provider;

void config_provider_register(void) {
    cofi_init_provider_defaults(&s_config_provider);
    s_config_provider.tab_mode = TAB_CONFIG;
    s_config_provider.id = "config";
    s_config_provider.display_name = "CONFIG";
    s_config_provider.primary_cmd = "config";
    s_config_provider.hidden_by_default = 1;
    s_config_provider.initial_selection_index = 0;
    s_config_provider.row_count = config_row_count;
    s_config_provider.format_row = config_format_row;
    s_config_provider.match_string = config_match_string;
    s_config_provider.row_identity = config_row_identity;
    s_config_provider.on_enter = config_on_enter;
    s_config_provider.on_query_changed = config_on_query_changed;
    s_config_provider.get_shortcut_hint = config_shortcut_hint;
    cofi_register_tab_provider(&s_config_provider);
}
