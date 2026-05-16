#include "hotkeys_provider.h"

#include "cofi_tab_provider.h"
#include "hotkey_config.h"
#include "log.h"
#include "match.h"
#include "selection.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

static int hotkeys_row_count(AppData *app) {
    if (!app) return 0;
    return app->filtered_hotkeys_count > 0 ? app->filtered_hotkeys_count : 1;
}

static HotkeyBinding *hotkey_at_row(AppData *app, int raw_idx) {
    if (!app || raw_idx < 0 || raw_idx >= app->filtered_hotkeys_count) {
        return NULL;
    }
    return &app->filtered_hotkeys[raw_idx];
}

static void hotkeys_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    HotkeyBinding *binding = hotkey_at_row(app, raw_idx);
    if (!binding) {
        out->cell_count = 1;
        out->cells[0].text = "No hotkey bindings found";
        out->row_flags = 0;
        return;
    }

    out->cell_count = 2;
    out->cells[0].text = binding->key;
    out->cells[0].width_hint = 24;
    out->cells[1].text = binding->command;
    out->cells[1].width_hint = 70;
    out->row_flags = COFI_ROW_ACTIONABLE;
}

static const char *hotkeys_match_string(AppData *app, int raw_idx) {
    HotkeyBinding *binding = hotkey_at_row(app, raw_idx);
    static char searchable[384];
    if (!binding) return "";
    g_snprintf(searchable, sizeof(searchable), "%s %s",
               binding->key, binding->command);
    return searchable;
}

static const char *hotkeys_row_identity(AppData *app, int raw_idx) {
    HotkeyBinding *binding = hotkey_at_row(app, raw_idx);
    static char identity[96];
    if (!binding) return "";
    g_snprintf(identity, sizeof(identity), "hotkey:%s", binding->key);
    return identity;
}

static void hotkeys_on_enter(AppData *app) {
    if (!app || !app->entry) return;
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry),
                                   "Type to filter hotkey bindings...");
    filter_hotkeys(app, "");
}

static void hotkeys_on_query_changed(AppData *app, const char *query) {
    filter_hotkeys(app, query);
    reset_selection(app);
}

static const char *hotkeys_shortcut_hint(AppData *app) {
    if (!app || app->filtered_hotkeys_count == 0) {
        return "Shortcuts: Ctrl+A=Add binding";
    }
    return "Shortcuts: Ctrl+A=Add binding  Ctrl+B=Rebind key  Ctrl+E=Edit command  Ctrl+D=Delete binding";
}

void filter_hotkeys(AppData *app, const char *filter) {
    if (!app) return;
    app->filtered_hotkeys_count = 0;

    if (!filter || !*filter) {
        for (int i = 0; i < app->hotkey_config.count; i++) {
            app->filtered_hotkeys[app->filtered_hotkeys_count] = app->hotkey_config.bindings[i];
            app->filtered_hotkeys_indices[app->filtered_hotkeys_count] = i;
            app->filtered_hotkeys_count++;
        }
        return;
    }

    for (int i = 0; i < app->hotkey_config.count; i++) {
        char searchable[512];
        snprintf(searchable, sizeof(searchable), "%s %s",
                 app->hotkey_config.bindings[i].key,
                 app->hotkey_config.bindings[i].command);
        if (has_match(filter, searchable)) {
            app->filtered_hotkeys[app->filtered_hotkeys_count] = app->hotkey_config.bindings[i];
            app->filtered_hotkeys_indices[app->filtered_hotkeys_count] = i;
            app->filtered_hotkeys_count++;
        }
    }
}

HotkeyBinding *hotkeys_selected_binding(AppData *app, int *master_idx_out) {
    if (master_idx_out) *master_idx_out = -1;
    if (!app || app->filtered_hotkeys_count <= 0) return NULL;

    int idx = app->selection.provider_index;
    if (idx < 0) idx = 0;
    if (idx >= app->filtered_hotkeys_count) idx = app->filtered_hotkeys_count - 1;
    app->selection.provider_index = idx;

    if (master_idx_out) {
        *master_idx_out = app->filtered_hotkeys_indices[idx];
    }
    return &app->filtered_hotkeys[idx];
}

void hotkeys_select_key(AppData *app, const char *key) {
    if (!app || !key || key[0] == '\0') return;

    for (int i = 0; i < app->filtered_hotkeys_count; i++) {
        if (strcmp(app->filtered_hotkeys[i].key, key) == 0) {
            app->selection.provider_index = i;
            return;
        }
    }
    app->selection.provider_index = 0;
}

static CofiTabProvider s_hotkeys_provider;

void hotkeys_provider_register(void) {
    cofi_init_provider_defaults(&s_hotkeys_provider);
    s_hotkeys_provider.tab_mode = TAB_HOTKEYS;
    s_hotkeys_provider.id = "hotkeys";
    s_hotkeys_provider.display_name = "HOTKEYS";
    s_hotkeys_provider.primary_cmd = "hotkeys";
    s_hotkeys_provider.hidden_by_default = 0;
    s_hotkeys_provider.initial_selection_index = 0;
    s_hotkeys_provider.row_count = hotkeys_row_count;
    s_hotkeys_provider.format_row = hotkeys_format_row;
    s_hotkeys_provider.match_string = hotkeys_match_string;
    s_hotkeys_provider.row_identity = hotkeys_row_identity;
    s_hotkeys_provider.on_enter = hotkeys_on_enter;
    s_hotkeys_provider.on_query_changed = hotkeys_on_query_changed;
    s_hotkeys_provider.get_shortcut_hint = hotkeys_shortcut_hint;
    cofi_register_tab_provider(&s_hotkeys_provider);
}
