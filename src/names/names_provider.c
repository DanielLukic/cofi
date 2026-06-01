#include "names/names_provider.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

#include "commands/command_mode.h"
#include "commands/command_registry.h"
#include "core/log/log.h"
#include "core/selection/selection.h"
#include "daemon/daemon_socket.h"
#include "matching/match.h"
#include "matching/match_entry.h"
#include "ui/overlay_pattern.h"
#include "names/overlay_names.h"
#include "providers/cofi_tab_provider.h"
#include "ui/dynamic_display.h"
#include "ui/overlay_dispatch.h"
#include "ui/tab_switching.h"

static CofiTabProvider s_names_provider;
static int s_names_provider_id = -1;

static MatchEntry *entry_for_record(AppData *app, const NameRecord *record) {
    if (!app || !record) return NULL;
    int idx = match_entry_find_index_by_match_id(&app->matching, record->match_id);
    return idx >= 0 ? &app->matching.entries[idx] : NULL;
}

static int names_row_count(AppData *app) {
    if (!app) return 0;
    return app->filtered_names_count > 0 ? app->filtered_names_count : 1;
}

static NameRecord *name_at_row(AppData *app, int raw_idx) {
    if (!app || raw_idx < 0 || raw_idx >= app->filtered_names_count) return NULL;
    return &app->filtered_names[raw_idx];
}

static void names_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    NameRecord *record = name_at_row(app, raw_idx);
    if (!record) {
        out->cell_count = 1;
        out->cells[0].text = "No named windows found";
        out->row_flags = 0;
        return;
    }

    MatchEntry *entry = entry_for_record(app, record);
    static char window_id[32];
    if (entry && entry->assigned) {
        snprintf(window_id, sizeof(window_id), "0x%lx", entry->bound_x11_id);
    } else {
        g_strlcpy(window_id, "* NONE *", sizeof(window_id));
    }

    const int name_w = 16;
    const int class_w = 16;
    const int instance_w = 14;
    const int type_w = 8;
    const int id_w = 14;
    const int cell_count = 6;
    const int fixed_total = name_w + class_w + instance_w + type_w + id_w
                            + (cell_count - 1) + 2;
    int title_w = get_display_columns(app) - fixed_total;
    if (title_w < 20) title_w = 20;

    out->cell_count = cell_count;
    out->cells[0].text = record->custom_name;
    out->cells[0].width_hint = name_w;
    out->cells[1].text = entry ? entry->original_title : "(missing)";
    out->cells[1].width_hint = title_w;
    out->cells[2].text = entry ? entry->class_name : "";
    out->cells[2].width_hint = class_w;
    out->cells[3].text = entry ? entry->instance : "";
    out->cells[3].width_hint = instance_w;
    out->cells[4].text = entry ? entry->type : "";
    out->cells[4].width_hint = type_w;
    out->cells[5].text = window_id;
    out->cells[5].width_hint = id_w;
    out->row_flags = COFI_ROW_ACTIONABLE;
}

static const char *names_match_string(AppData *app, int raw_idx) {
    NameRecord *record = name_at_row(app, raw_idx);
    MatchEntry *entry = entry_for_record(app, record);
    static char searchable[1024];
    if (!record) return "";
    g_snprintf(searchable, sizeof(searchable), "%s %s %s %s %s",
               record->custom_name,
               entry ? entry->original_title : "",
               entry ? entry->class_name : "",
               entry ? entry->instance : "",
               entry ? entry->type : "");
    return searchable;
}

static const char *names_row_identity(AppData *app, int raw_idx) {
    NameRecord *record = name_at_row(app, raw_idx);
    static char identity[64];
    if (!record) return "";
    g_snprintf(identity, sizeof(identity), "name:%d", record->match_id);
    return identity;
}

static void filter_names(AppData *app, const char *filter) {
    if (!app) return;
    app->filtered_names_count = 0;

    for (int i = 0; i < app->names.count && app->filtered_names_count < MAX_WINDOWS; i++) {
        NameRecord *record = &app->names.records[i];
        MatchEntry *entry = entry_for_record(app, record);
        char searchable[1024];
        snprintf(searchable, sizeof(searchable), "%s %s %s %s %s",
                 record->custom_name,
                 entry ? entry->original_title : "",
                 entry ? entry->class_name : "",
                 entry ? entry->instance : "",
                 entry ? entry->type : "");

        if (!filter || filter[0] == '\0' || has_match(filter, searchable)) {
            int out = app->filtered_names_count++;
            app->filtered_names[out] = *record;
            app->filtered_names_indices[out] = i;
        }
    }
}

static void names_on_enter(AppData *app) {
    if (!app || !app->entry) return;
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry),
                                   "Type to filter named windows...");
    filter_names(app, "");
}

void names_on_query_changed(AppData *app, const char *query) {
    filter_names(app, query);
    reset_selection(app);
}

NameRecord *names_selected_record(AppData *app) {
    if (!app || app->filtered_names_count <= 0) return NULL;
    int idx = app->selection.provider_index;
    if (idx < 0) idx = 0;
    if (idx >= app->filtered_names_count) idx = app->filtered_names_count - 1;
    app->selection.provider_index = idx;
    return &app->filtered_names[idx];
}

int names_selected_store_index(AppData *app) {
    if (!app || !names_selected_record(app)) return -1;
    return app->filtered_names_indices[app->selection.provider_index];
}

void names_select_custom_name(AppData *app, const char *custom_name) {
    if (!app || !custom_name || custom_name[0] == '\0') return;
    for (int i = 0; i < app->filtered_names_count; i++) {
        if (strcmp(app->filtered_names[i].custom_name, custom_name) == 0) {
            app->selection.provider_index = i;
            return;
        }
    }
    app->selection.provider_index = 0;
}

gboolean handle_names_tab_keys(GdkEventKey *event, AppData *app) {
    if (!app || app->current_tab != names_tab_mode()) return FALSE;

    if (event->keyval == GDK_KEY_e && (event->state & GDK_CONTROL_MASK)) {
        if (!names_selected_record(app)) return FALSE;
        show_name_edit_overlay(app);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_d && (event->state & GDK_CONTROL_MASK)) {
        NameRecord *record = names_selected_record(app);
        if (!record) {
            log_debug("Names Ctrl+D ignored: no rows to delete");
            return FALSE;
        }
        show_name_delete_overlay(app, record->custom_name, record->match_id);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_p && (event->state & GDK_CONTROL_MASK)) {
        NameRecord *record = names_selected_record(app);
        if (!record) return FALSE;

        char context[160];
        g_snprintf(context, sizeof(context), "Name: %s", record->custom_name);
        return show_pattern_edit_overlay(app, record->match_id, context);
    }

    return FALSE;
}

TabMode names_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_names_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

static gboolean names_command_handler(AppData *app,
                                      WindowInfo *window __attribute__((unused)),
                                      const char *args __attribute__((unused))) {
    if (!app) return FALSE;
    exit_command_mode(app);
    app->prefix_origin_tab = app->current_tab;
    surface_tab(app, names_tab_mode());
    return FALSE;
}

static const CommandSpec s_names_command = {
    .primary = "names",
    .aliases = {"nm", NULL},
    .owner_provider_id = "names",
    .handler = names_command_handler,
    .description = "Switch to Names tab",
    .help_format = "names, nm",
    .keeps_open_on_hotkey_auto = 1
};

void names_provider_register(void) {
    cofi_init_provider_defaults(&s_names_provider);
    s_names_provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    s_names_provider.id = "names";
    s_names_provider.display_name = "NAMES";
    s_names_provider.delegate_opcode = COFI_OPCODE_NAMES;
    s_names_provider.required = 0;
    s_names_provider.hidden_by_default = 1;
    s_names_provider.initial_selection_index = 0;
    s_names_provider.row_count = names_row_count;
    s_names_provider.format_row = names_format_row;
    s_names_provider.match_string = names_match_string;
    s_names_provider.row_identity = names_row_identity;
    s_names_provider.on_enter = names_on_enter;
    s_names_provider.on_query_changed = names_on_query_changed;
    s_names_provider.handle_key = handle_names_tab_keys;
    s_names_provider.shortcut_hint = "Shortcuts: Ctrl+E=Edit name  Ctrl+P=Edit pattern  Ctrl+D=Delete";
    s_names_provider_id = cofi_register_tab_provider(&s_names_provider);
    if (s_names_provider_id >= 0) {
        cofi_register_command(&s_names_command);
    }
}
