#include "apps_provider.h"

#include "apps.h"
#include "command_mode.h"
#include "command_registry.h"
#include "cofi_tab_provider.h"
#include "log.h"
#include "path_binaries.h"
#include "selection.h"
#include "tab_switching.h"

#include <gtk/gtk.h>

static CofiTabProvider s_apps_provider;
static int s_apps_provider_id = -1;

TabMode apps_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_apps_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

static int apps_row_count(AppData *app) {
    if (!app) return 0;
    int count = app->filtered_apps_count;
    if (path_binaries_is_scanning()) count++;
    if (app->filtered_apps_count == 0) count++;
    return count;
}

static AppEntry *app_at_row(AppData *app, int raw_idx) {
    if (!app || raw_idx < 0 || raw_idx >= app->filtered_apps_count) {
        return NULL;
    }
    return &app->filtered_apps[raw_idx];
}

static void apps_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    AppEntry *entry = app_at_row(app, raw_idx);
    if (entry) {
        out->cell_count = 2;
        out->cells[0].text = entry->name;
        out->cells[0].width_hint = 48;
        out->cells[1].text = entry->generic_name;
        out->cells[1].width_hint = 40;
        out->row_flags = COFI_ROW_ACTIONABLE;
        return;
    }

    out->cell_count = 1;
    out->cells[0].text = raw_idx == app->filtered_apps_count && path_binaries_is_scanning()
        ? "Scanning PATH..."
        : "No matching applications found";
    out->row_flags = 0;
}

static const char *apps_match_string(AppData *app, int raw_idx) {
    AppEntry *entry = app_at_row(app, raw_idx);
    return entry ? entry->name : "";
}

static const char *apps_row_identity(AppData *app, int raw_idx) {
    AppEntry *entry = app_at_row(app, raw_idx);
    static char identity[768];
    if (!entry) return "";
    if (entry->source_kind == APP_SOURCE_PATH) {
        g_snprintf(identity, sizeof(identity), "path:%s", entry->exec_path);
    } else if (entry->source_kind == APP_SOURCE_SYSTEM) {
        g_snprintf(identity, sizeof(identity), "system:%d", (int)entry->action_id);
    } else {
        const char *id = entry->info ? g_app_info_get_id(entry->info) : NULL;
        g_snprintf(identity, sizeof(identity), "desktop:%s", id ? id : entry->name);
    }
    return identity;
}

static void apps_on_enter(AppData *app) {
    if (!app || !app->entry) return;
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter applications...");
    apps_load();
    filter_apps(app, "");
}

static void apps_on_leave(AppData *app) {
    if (!app) return;
    app->apps_mode = APPS_MODE_DEFAULT;
}

static void apps_on_surface(AppData *app) {
    if (!app) return;
    app->apps_mode = APPS_MODE_DEFAULT;
}

static void apps_on_query_changed(AppData *app, const char *query) {
    filter_apps(app, query);
    reset_selection(app);
}

static CofiActionStatus apps_on_enter_pressed(AppData *app, int filtered_idx,
                                               int raw_idx,
                                               const char *entry_text,
                                               int modifier_state) {
    (void)filtered_idx;
    (void)entry_text;
    (void)modifier_state;
    AppEntry *entry = app_at_row(app, raw_idx);
    if (!entry) return COFI_NO_OP;
    log_info("USER: ENTER pressed -> Launching app '%s'", entry->name);
    apps_launch(entry);
    return COFI_HANDLED_HIDE;
}

static gboolean apps_command_handler(AppData *app,
                                     WindowInfo *window __attribute__((unused)),
                                     const char *args __attribute__((unused))) {
    if (!app) return FALSE;
    exit_command_mode(app);
    app->prefix_origin_tab = app->current_tab;
    apps_on_surface(app);
    surface_tab(app, apps_tab_mode());
    return FALSE;
}

void filter_apps(AppData *app, const char *filter) {
    if (!app) return;
    const char *query = filter ? filter : "";

    if (app->apps_mode == APPS_MODE_PATH) {
        path_binaries_ensure_loaded(app);
        path_binaries_filter(query, app->filtered_apps, &app->filtered_apps_count);
        return;
    }

    apps_filter(query, app->filtered_apps, &app->filtered_apps_count);
}

static const CommandSpec s_apps_command = {
    .primary = "apps",
    .aliases = {"applications", "app", NULL},
    .owner_provider_id = "apps",
    .handler = apps_command_handler,
    .description = "Switch to applications tab",
    .help_format = "apps, app, applications",
    .keeps_open_on_hotkey_auto = 1
};

void apps_provider_register(void) {
    cofi_init_provider_defaults(&s_apps_provider);
    s_apps_provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    s_apps_provider.id = "apps";
    s_apps_provider.display_name = "APPS";
    s_apps_provider.required = 0;
    s_apps_provider.hidden_by_default = 0;
    s_apps_provider.initial_selection_index = 0;
    s_apps_provider.row_count = apps_row_count;
    s_apps_provider.format_row = apps_format_row;
    s_apps_provider.match_string = apps_match_string;
    s_apps_provider.row_identity = apps_row_identity;
    s_apps_provider.on_enter = apps_on_enter;
    s_apps_provider.on_leave = apps_on_leave;
    s_apps_provider.on_surface = apps_on_surface;
    s_apps_provider.on_query_changed = apps_on_query_changed;
    s_apps_provider.on_enter_pressed = apps_on_enter_pressed;
    s_apps_provider_id = cofi_register_tab_provider(&s_apps_provider);
    if (s_apps_provider_id >= 0) {
        cofi_register_command(&s_apps_command);
    }
}
