#include "geom/geom_provider.h"

#include "providers/cofi_tab_provider.h"
#include "commands/command_mode.h"
#include "commands/command_registry.h"
#include "ui/display.h"
#include "geom/geom_rule_sync.h"
#include "core/log/log.h"
#include "matching/match.h"
#include "matching/match_entry_config.h"
#include "rules/rules.h"
#include "rules/rules_config.h"
#include "ui/overlay_confirm.h"
#include "ui/overlay_pattern.h"
#include "core/selection/selection.h"
#include "ui/tab_switching.h"

#include <stdio.h>
#include <string.h>

static CofiTabProvider s_geom_provider;
static int s_geom_provider_id = -1;
static int s_pending_delete_match_id = 0;
void geom_on_query_changed(AppData *app, const char *query);

static MatchEntry *geom_match_entry_for_record(AppData *app, const LayoutRecord *record) {
    if (!app || !record) return NULL;
    int idx = match_entry_find_index_by_match_id(&app->matching, record->match_id);
    if (idx < 0 || idx >= app->matching.count) {
        log_warn("geom: missing MatchEntry for layout match_id=%d; skipping row", record->match_id);
        return NULL;
    }
    return &app->matching.entries[idx];
}

static LayoutRecord *geom_record_at_row(AppData *app, int raw_idx) {
    if (!app || raw_idx < 0 || raw_idx >= app->filtered_geom_count) return NULL;
    int layout_idx = app->filtered_geom[raw_idx];
    if (layout_idx < 0 || layout_idx >= app->layouts.count) return NULL;
    return &app->layouts.records[layout_idx];
}

static void format_geom(const LayoutRecord *record, char *out, size_t out_size) {
    if (!record || !out || out_size == 0) return;
    g_snprintf(out, out_size, "%dx%d+%d+%d",
               record->width, record->height, record->x, record->y);
}

static void format_state_flags(AppData *app, const LayoutRecord *record,
                               char *out, size_t out_size) {
    if (!record || !out || out_size == 0) return;
    Rule *rule = geom_find_owning_rule(app, record->match_id);
    char once_marker = rule && rule->once ? 'O' : '-';
    char new_marker = rule && rule->new_only ? 'N' : '-';
    g_snprintf(out, out_size, "d=%d %c%c%c%c%c %c%c",
               record->desktop,
               record->maximized_vert ? 'V' : '-',
               record->maximized_horz ? 'H' : '-',
               record->fullscreen ? 'F' : '-',
               record->restore_desktop ? 'L' : '-',
               record->disabled ? 'D' : '-',
               once_marker,
               new_marker);
}

static const char *geom_class_for_entry(const AppData *app, const MatchEntry *entry) {
    if (!app || !entry || !entry->assigned || entry->bound_x11_id == 0) return "-";
    for (int i = 0; i < app->window_count; i++) {
        if (app->windows[i].id == entry->bound_x11_id) {
            return app->windows[i].class_name[0] ? app->windows[i].class_name : "-";
        }
    }
    return "-";
}

static int geom_row_count(AppData *app) {
    if (!app) return 0;
    return app->filtered_geom_count;
}

static void geom_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    LayoutRecord *record = geom_record_at_row(app, raw_idx);
    if (!record) {
        out->cell_count = 0;
        out->row_flags = 0;
        return;
    }

    MatchEntry *entry = geom_match_entry_for_record(app, record);
    if (!entry) {
        out->cell_count = 0;
        out->row_flags = 0;
        return;
    }

    static char geometry[64];
    static char desktop_flags[32];
    format_geom(record, geometry, sizeof(geometry));
    format_state_flags(app, record, desktop_flags, sizeof(desktop_flags));

    const char *label = entry->original_title;
    const char *class_name = geom_class_for_entry(app, entry);
    out->cell_count = 5;
    out->cells[0].text = label;
    out->cells[0].width_hint = 25;
    out->cells[1].text = class_name;
    out->cells[1].width_hint = 18;
    out->cells[2].text = geometry;
    out->cells[2].width_hint = 18;
    out->cells[3].text = desktop_flags;
    out->cells[3].width_hint = 17;
    out->cells[4].text = entry->assigned ? "bound" : "unbound";
    out->cells[4].width_hint = 8;
    out->row_flags = COFI_ROW_ACTIONABLE;
}

static const char *geom_match_string(AppData *app, int raw_idx) {
    static char searchable[1024];
    LayoutRecord *record = geom_record_at_row(app, raw_idx);
    MatchEntry *entry = geom_match_entry_for_record(app, record);
    if (!record || !entry) return "";

    char geometry[64];
    format_geom(record, geometry, sizeof(geometry));
    const char *label = entry->original_title;
    g_snprintf(searchable, sizeof(searchable), "%s %s %s",
               label, geom_class_for_entry(app, entry), geometry);
    return searchable;
}

static const char *geom_row_identity(AppData *app, int raw_idx) {
    static char identity[64];
    LayoutRecord *record = geom_record_at_row(app, raw_idx);
    if (!record) return "";
    g_snprintf(identity, sizeof(identity), "geom:%d", record->match_id);
    return identity;
}

static void geom_on_enter(AppData *app) {
    if (!app || !app->entry) return;
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "search saved layouts");
    geom_on_query_changed(app, "");
}

void geom_on_query_changed(AppData *app, const char *query) {
    if (!app) return;
    app->filtered_geom_count = 0;

    for (int i = 0; i < app->layouts.count && app->filtered_geom_count < MAX_WINDOWS; i++) {
        LayoutRecord *record = &app->layouts.records[i];
        MatchEntry *entry = geom_match_entry_for_record(app, record);
        if (!entry) continue;

        char searchable[1024];
        char geometry[64];
        const char *label = entry->original_title;
        format_geom(record, geometry, sizeof(geometry));
        g_snprintf(searchable, sizeof(searchable), "%s %s %s",
                   label, geom_class_for_entry(app, entry), geometry);
        if (!query || !query[0] || has_match(query, searchable)) {
            app->filtered_geom[app->filtered_geom_count++] = i;
        }
    }
    reset_selection(app);
}

static LayoutRecord *geom_selected_record(AppData *app) {
    if (!app || app->filtered_geom_count <= 0) return NULL;
    int idx = app->selection.provider_index;
    if (idx < 0) idx = 0;
    if (idx >= app->filtered_geom_count) idx = app->filtered_geom_count - 1;
    app->selection.provider_index = idx;
    return geom_record_at_row(app, idx);
}

static void geom_delete_confirmed(AppData *app) {
    int match_id = s_pending_delete_match_id;
    s_pending_delete_match_id = 0;
    if (!app || match_id <= 0) return;

    if (!layout_store_clear(&app->layouts, match_id)) return;
    if (!layout_store_save(&app->layouts)) {
        log_warn("geom: layout save failed after delete; skipping rule sync");
        return;
    }
    geom_rule_remove_for_match_id(app, match_id);
    match_entry_delete_by_match_id(&app->matching, match_id);
    save_match_entries(&app->matching);
    const char *query = "";
    if (app->entry) {
        query = gtk_entry_get_text(GTK_ENTRY(app->entry));
    }
    geom_on_query_changed(app, query);
    if (app->filtered_geom_count <= 0) {
        app->selection.provider_index = 0;
    } else if (app->selection.provider_index >= app->filtered_geom_count) {
        app->selection.provider_index = app->filtered_geom_count - 1;
    } else if (app->selection.provider_index < 0) {
        app->selection.provider_index = 0;
    }
    update_display(app);
}

static void show_geom_delete_confirm(AppData *app, const LayoutRecord *record) {
    if (!app || !record) return;
    s_pending_delete_match_id = record->match_id;
    char info[256];
    g_snprintf(info, sizeof(info), "Delete saved layout for match #%d?", record->match_id);
    show_confirm_overlay(app, "Delete Saved Layout?", info, geom_delete_confirmed);
}

static gboolean geom_toggle_restore_desktop(AppData *app) {
    LayoutRecord *record = geom_selected_record(app);
    if (!record) return FALSE;
    record->restore_desktop = !record->restore_desktop;
    layout_store_save(&app->layouts);
    update_display(app);
    return TRUE;
}

static gboolean geom_toggle_rule_flag(AppData *app, LayoutRecord *record,
                                      void (*toggle)(Rule *)) {
    Rule *rule = geom_find_owning_rule(app, record->match_id);
    if (!rule) {
        log_warn("geom: no owning rule for match_id=%d; toggle ignored", record->match_id);
        return FALSE;
    }
    toggle(rule);
    save_rules_config(&app->rules_config, &app->matching);
    update_display(app);
    return TRUE;
}

static gboolean geom_toggle_disabled(AppData *app) {
    LayoutRecord *record = geom_selected_record(app);
    if (!record) return FALSE;
    int match_id = record->match_id;
    record->disabled = !record->disabled;
    if (!layout_store_save(&app->layouts)) {
        log_warn("geom: layout save failed after toggle; skipping rule sync");
        return FALSE;
    }
    geom_rule_sync_for_layout(app, match_id);
    update_display(app);
    return TRUE;
}

gboolean handle_geom_tab_keys(GdkEventKey *event, AppData *app) {
    if (!event || !app || app->current_tab != geom_tab_mode()) return FALSE;

    LayoutRecord *record = geom_selected_record(app);
    if (!record) return FALSE;

    if ((event->keyval == GDK_KEY_d && (event->state & GDK_CONTROL_MASK)) ||
        event->keyval == GDK_KEY_Delete) {
        show_geom_delete_confirm(app, record);
        return TRUE;
    }
    if (event->keyval == GDK_KEY_l && (event->state & GDK_CONTROL_MASK)) {
        return geom_toggle_restore_desktop(app);
    }
    if (event->keyval == GDK_KEY_t && (event->state & GDK_CONTROL_MASK)) {
        return geom_toggle_disabled(app);
    }
    if (event->keyval == GDK_KEY_o && (event->state & GDK_CONTROL_MASK)) {
        return geom_toggle_rule_flag(app, record, rule_toggle_once);
    }
    if (event->keyval == GDK_KEY_n && (event->state & GDK_CONTROL_MASK)) {
        return geom_toggle_rule_flag(app, record, rule_toggle_new_only);
    }
    if (event->keyval == GDK_KEY_p && (event->state & GDK_CONTROL_MASK)) {
        int match_id = selected_match_id_for_pattern_edit(app);
        char geom[64];
        char context[96];
        format_geom(record, geom, sizeof(geom));
        g_snprintf(context, sizeof(context), "Layout: %s%s",
                   geom, record->disabled ? " (disabled)" : "");
        show_pattern_edit_overlay(app, match_id, context);
        return TRUE;
    }
    return FALSE;
}

static gboolean geom_command_handler(AppData *app,
                                     WindowInfo *window __attribute__((unused)),
                                     const char *args __attribute__((unused))) {
    if (!app) return FALSE;
    exit_command_mode(app);
    app->prefix_origin_tab = app->current_tab;
    surface_tab(app, geom_tab_mode());
    return FALSE;
}

static const CommandSpec s_geom_command = {
    .primary = "geom",
    .aliases = {"layouts", NULL},
    .owner_provider_id = "geom",
    .handler = geom_command_handler,
    .description = "Switch to Layouts tab",
    .help_format = "geom, layouts",
    .keeps_open_on_hotkey_auto = 1
};

TabMode geom_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_geom_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

void geom_provider_register(void) {
    cofi_init_provider_defaults(&s_geom_provider);
    s_geom_provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    s_geom_provider.id = "geom";
    s_geom_provider.display_name = "LAYOUTS";
    s_geom_provider.required = 0;
    s_geom_provider.hidden_by_default = 1;
    s_geom_provider.modal_policy = COFI_MODAL_HIDE_ON_ESC;
    s_geom_provider.row_count = geom_row_count;
    s_geom_provider.format_row = geom_format_row;
    s_geom_provider.match_string = geom_match_string;
    s_geom_provider.row_identity = geom_row_identity;
    s_geom_provider.on_enter = geom_on_enter;
    s_geom_provider.on_query_changed = geom_on_query_changed;
    s_geom_provider.handle_key = handle_geom_tab_keys;
    s_geom_provider.shortcut_hint =
        "Ctrl+D=Delete  Ctrl+L=Lock workspace  Ctrl+T=Toggle enable  Ctrl+P=Edit pattern  Ctrl+O=Once  Ctrl+N=New-only";
    s_geom_provider_id = cofi_register_tab_provider(&s_geom_provider);
    if (s_geom_provider_id >= 0) {
        cofi_register_command(&s_geom_command);
    }
}
