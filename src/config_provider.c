#include "config_provider.h"

#include "cofi_tab_provider.h"
#include "command_mode.h"
#include "command_registry.h"
#include "config.h"
#include "display.h"
#include "log.h"
#include "match.h"
#include "overlay_manager.h"
#include "selection.h"
#include "tab_switching.h"

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

static int config_entry_opens_provider_list(const ConfigEntry *entry) {
    return entry && entry->type == CONFIG_TYPE_PROVIDER_LIST;
}

static const char *config_shortcut_hint(AppData *app) {
    ConfigEntry *entry = config_selected_entry(app);
    if (!entry) {
        return "Shortcuts: Ctrl+T=Toggle bool/enum  Ctrl+E=Edit value/provider list";
    }
    if (config_entry_opens_provider_list(entry))
        return "Shortcuts: Ctrl+E/Ctrl+T=Edit provider list";
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
static int s_config_provider_id = -1;

TabMode config_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_config_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

gboolean handle_config_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != config_tab_mode()) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_t && (event->state & GDK_CONTROL_MASK)) {
        ConfigEntry *entry = config_selected_entry(app);
        if (entry) {
            if (config_entry_opens_provider_list(entry)) {
                show_overlay(app, OVERLAY_PROVIDER_ENABLEMENT, NULL);
                return TRUE;
            }

            const char *new_value = NULL;
            if (entry->type == CONFIG_TYPE_BOOL) {
                new_value = (strcmp(entry->value, "true") == 0) ? "false" : "true";
            } else if (entry->type == CONFIG_TYPE_ENUM) {
                new_value = get_next_enum_value(entry->key, entry->value);
            }

            if (new_value) {
                char selected_key[sizeof(entry->key)];
                g_strlcpy(selected_key, entry->key, sizeof(selected_key));
                char err_buf[128];
                if (apply_config_setting(&app->config, selected_key, new_value, err_buf, sizeof(err_buf))) {
                    save_config(&app->config);
                    const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
                    filter_config(app, current_filter);
                    config_select_key(app, selected_key);
                    update_display(app);
                    log_info("USER: Cycled config '%s' to %s", selected_key, new_value);
                } else {
                    log_error("Failed to cycle config '%s': %s", selected_key, err_buf);
                }
                return TRUE;
            }
        }
    }

    if (event->keyval == GDK_KEY_e && (event->state & GDK_CONTROL_MASK)) {
        ConfigEntry *entry = config_selected_entry(app);
        if (config_entry_opens_provider_list(entry)) {
            show_overlay(app, OVERLAY_PROVIDER_ENABLEMENT, NULL);
            return TRUE;
        }
        if (config_entry_allows_edit(entry)) {
            show_overlay(app, OVERLAY_CONFIG_EDIT, NULL);
            return TRUE;
        }
    }

    return FALSE;
}

static gboolean config_command_handler(AppData *app,
                                       WindowInfo *window __attribute__((unused)),
                                       const char *args __attribute__((unused))) {
    if (!app) return FALSE;

    exit_command_mode(app);
    app->prefix_origin_tab = app->current_tab;
    surface_tab(app, config_tab_mode());
    return FALSE;
}

static const CommandSpec s_config_command = {
    .primary = "config",
    .aliases = {"conf", "cfg", NULL},
    .owner_provider_id = "config",
    .handler = config_command_handler,
    .description = "Show current configuration",
    .help_format = "config, conf",
    .keeps_open_on_hotkey_auto = 1
};

void config_provider_register(void) {
    cofi_init_provider_defaults(&s_config_provider);
    s_config_provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    s_config_provider.id = "config";
    s_config_provider.display_name = "CONFIG";
    s_config_provider.required = 1;
    s_config_provider.hidden_by_default = 1;
    s_config_provider.initial_selection_index = 0;
    s_config_provider.row_count = config_row_count;
    s_config_provider.format_row = config_format_row;
    s_config_provider.match_string = config_match_string;
    s_config_provider.row_identity = config_row_identity;
    s_config_provider.on_enter = config_on_enter;
    s_config_provider.on_query_changed = config_on_query_changed;
    s_config_provider.handle_key = handle_config_tab_keys;
    s_config_provider.get_shortcut_hint = config_shortcut_hint;
    s_config_provider_id = cofi_register_tab_provider(&s_config_provider);
    if (s_config_provider_id >= 0) {
        cofi_register_command(&s_config_command);
    }
}
