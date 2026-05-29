#include "matching/matching_provider.h"

#include "providers/cofi_tab_provider.h"
#include "commands/command_mode.h"
#include "commands/command_registry.h"
#include "daemon/daemon_socket.h"
#include "ui/dynamic_display.h"
#include "matching/filter_matching.h"
#include "core/log/log.h"
#include "matching/match.h"
#include "matching/match_entry.h"
#include "core/selection/selection.h"
#include "ui/tab_switching.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

static int matching_row_count(AppData *app) {
    if (!app) return 0;
    return app->filtered_matching_count > 0 ? app->filtered_matching_count : 1;
}

static MatchEntry *entry_at_row(AppData *app, int raw_idx) {
    if (!app || raw_idx < 0 || raw_idx >= app->filtered_matching_count) {
        return NULL;
    }
    return &app->filtered_matching[raw_idx];
}

static void matching_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    MatchEntry *entry = entry_at_row(app, raw_idx);
    if (!entry) {
        out->cell_count = 1;
        out->cells[0].text = "No match entries found";
        out->row_flags = 0;
        return;
    }

    static char window_id[32];
    if (entry->assigned) {
        snprintf(window_id, sizeof(window_id), "0x%lx", entry->bound_x11_id);
    } else {
        g_strlcpy(window_id, "* NONE *", sizeof(window_id));
    }

    const int class_w = 16;
    const int instance_w = 14;
    const int type_w = 8;
    const int id_w = 14;
    const int cell_count = 5;
    const int separators = cell_count - 1;
    const int selection_overhead = 2;  // "> " / "  "
    const int fixed_total = class_w + instance_w + type_w + id_w
                            + separators + selection_overhead;
    int target = get_display_columns(app);
    int title_w = target - fixed_total;
    if (title_w < 20) {
        title_w = 20;
    }

    out->cell_count = cell_count;
    out->cells[0].text = entry->original_title;
    out->cells[0].width_hint = title_w;
    out->cells[1].text = entry->class_name;
    out->cells[1].width_hint = class_w;
    out->cells[2].text = entry->instance;
    out->cells[2].width_hint = instance_w;
    out->cells[3].text = entry->type;
    out->cells[3].width_hint = type_w;
    out->cells[4].text = window_id;
    out->cells[4].width_hint = id_w;
    out->row_flags = 0;
}

static const char *matching_match_string(AppData *app, int raw_idx) {
    MatchEntry *entry = entry_at_row(app, raw_idx);
    static char searchable[1024];
    if (!entry) return "";
    g_snprintf(searchable, sizeof(searchable), "%s %s %s %s",
               entry->original_title,
               entry->class_name, entry->instance, entry->type);
    return searchable;
}

static const char *matching_row_identity(AppData *app, int raw_idx) {
    MatchEntry *entry = entry_at_row(app, raw_idx);
    static char identity[256];
    if (!entry) return "";
    g_snprintf(identity, sizeof(identity), "match:%d", entry->match_id);
    return identity;
}

static void matching_on_enter(AppData *app) {
    if (!app || !app->entry) return;
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry),
                                   "Type to filter match entries...");
    filter_matching(app, "");
}

static void matching_on_query_changed(AppData *app, const char *query) {
    filter_matching(app, query);
    reset_selection(app);
}

MatchEntry *matching_selected_entry(AppData *app) {
    if (!app || app->filtered_matching_count <= 0) return NULL;

    int idx = app->selection.provider_index;
    if (idx < 0) idx = 0;
    if (idx >= app->filtered_matching_count) idx = app->filtered_matching_count - 1;
    app->selection.provider_index = idx;

    return &app->filtered_matching[idx];
}

int matching_selected_manager_index(AppData *app) {
    MatchEntry *named = matching_selected_entry(app);
    if (!app || !named) return -1;
    return match_entry_find_index_by_match_id(&app->matching, named->match_id);
}

static CofiTabProvider s_matching_provider;
static int s_matching_provider_id = -1;

TabMode matching_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_matching_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

static gboolean matching_command_handler(AppData *app,
                                         WindowInfo *window __attribute__((unused)),
                                         const char *args __attribute__((unused))) {
    if (!app) return FALSE;

    exit_command_mode(app);
    app->prefix_origin_tab = app->current_tab;
    surface_tab(app, matching_tab_mode());
    return FALSE;
}

static const CommandSpec s_matching_command = {
    .primary = "matching",
    .aliases = {"m", NULL},
    .owner_provider_id = "matching",
    .handler = matching_command_handler,
    .description = "Switch to Matching tab",
    .help_format = "matching, m",
    .keeps_open_on_hotkey_auto = 1
};

void matching_provider_register(void) {
    cofi_init_provider_defaults(&s_matching_provider);
    s_matching_provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    s_matching_provider.id = "matching";
    s_matching_provider.display_name = "MATCHING";
    s_matching_provider.delegate_opcode = COFI_OPCODE_MATCHING;
    s_matching_provider.required = 0;
    s_matching_provider.hidden_by_default = 1;
    s_matching_provider.initial_selection_index = 0;
    s_matching_provider.row_count = matching_row_count;
    s_matching_provider.format_row = matching_format_row;
    s_matching_provider.match_string = matching_match_string;
    s_matching_provider.row_identity = matching_row_identity;
    s_matching_provider.on_enter = matching_on_enter;
    s_matching_provider.on_query_changed = matching_on_query_changed;
    s_matching_provider.shortcut_hint = NULL;
    s_matching_provider_id = cofi_register_tab_provider(&s_matching_provider);
    if (s_matching_provider_id >= 0) {
        cofi_register_command(&s_matching_command);
    }
}
