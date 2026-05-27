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
#include "ui/overlay_manager.h"
#include "matching/overlay_pattern.h"
#include "core/selection/selection.h"
#include "ui/tab_switching.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

static int matching_row_count(AppData *app) {
    if (!app) return 0;
    return app->filtered_matching_count > 0 ? app->filtered_matching_count : 1;
}

static MatchEntry *name_at_row(AppData *app, int raw_idx) {
    if (!app || raw_idx < 0 || raw_idx >= app->filtered_matching_count) {
        return NULL;
    }
    return &app->filtered_matching[raw_idx];
}

static void matching_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    MatchEntry *named = name_at_row(app, raw_idx);
    if (!named) {
        out->cell_count = 1;
        out->cells[0].text = "No named windows found";
        out->row_flags = 0;
        return;
    }

    static char window_id[32];
    if (named->assigned) {
        snprintf(window_id, sizeof(window_id), "0x%lx", named->bound_x11_id);
    } else {
        g_strlcpy(window_id, "* NONE *", sizeof(window_id));
    }

    const int label_w = 16;
    const int class_w = 16;
    const int instance_w = 14;
    const int type_w = 8;
    const int id_w = 14;
    const int cell_count = 6;
    const int separators = cell_count - 1;
    const int selection_overhead = 2;  // "> " / "  "
    const int fixed_total = label_w + class_w + instance_w + type_w + id_w
                            + separators + selection_overhead;
    int target = get_display_columns(app);
    int title_w = target - fixed_total;
    if (title_w < 20) {
        title_w = 20;
    }

    out->cell_count = cell_count;
    out->cells[0].text = named->original_title;
    out->cells[0].width_hint = title_w;
    out->cells[1].text = named->custom_name;
    out->cells[1].width_hint = label_w;
    out->cells[2].text = named->class_name;
    out->cells[2].width_hint = class_w;
    out->cells[3].text = named->instance;
    out->cells[3].width_hint = instance_w;
    out->cells[4].text = named->type;
    out->cells[4].width_hint = type_w;
    out->cells[5].text = window_id;
    out->cells[5].width_hint = id_w;
    out->row_flags = COFI_ROW_ACTIONABLE;
}

static const char *matching_match_string(AppData *app, int raw_idx) {
    MatchEntry *entry = name_at_row(app, raw_idx);
    static char searchable[1024];
    if (!entry) return "";
    g_snprintf(searchable, sizeof(searchable), "%s %s %s %s %s",
               entry->custom_name, entry->original_title,
               entry->class_name, entry->instance, entry->type);
    return searchable;
}

static const char *matching_row_identity(AppData *app, int raw_idx) {
    MatchEntry *entry = name_at_row(app, raw_idx);
    static char identity[256];
    if (!entry) return "";
    g_snprintf(identity, sizeof(identity), "match:%d", entry->match_id);
    return identity;
}

static void matching_on_enter(AppData *app) {
    if (!app || !app->entry) return;
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry),
                                   "Type to filter named windows...");
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

void matching_select_custom_name(AppData *app, const char *custom_name) {
    if (!app || !custom_name || custom_name[0] == '\0') return;

    for (int i = 0; i < app->filtered_matching_count; i++) {
        if (strcmp(app->filtered_matching[i].custom_name, custom_name) == 0) {
            app->selection.provider_index = i;
            return;
        }
    }
    app->selection.provider_index = 0;
}

gboolean handle_matching_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != matching_tab_mode()) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_e && (event->state & GDK_CONTROL_MASK)) {
        if (!matching_selected_entry(app)) {
            return FALSE;
        }
        show_name_edit_overlay(app);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_p && (event->state & GDK_CONTROL_MASK)) {
        MatchEntry *selected = matching_selected_entry(app);
        int match_id = selected_match_id_for_pattern_edit(app);
        char context[96];
        g_snprintf(context, sizeof(context), "Label: %s",
                   (selected && selected->custom_name[0]) ? selected->custom_name : "(none)");
        show_pattern_edit_overlay(app, match_id, context);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_d && (event->state & GDK_CONTROL_MASK)) {
        MatchEntry *named = matching_selected_entry(app);
        if (!named) {
            log_debug("Matching Ctrl+D ignored: no rows to delete");
            return FALSE;
        }

        int manager_index = matching_selected_manager_index(app);
        log_info("Matching Ctrl+D: showing delete confirm for '%s' (mgr_idx=%d, sel=%d/%d)",
                 named->custom_name, manager_index,
                 app->selection.provider_index, app->filtered_matching_count);
        show_name_delete_overlay(app, named->custom_name, manager_index);
        return TRUE;
    }

    return FALSE;
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
    .aliases = {"m", "names", "nm", NULL},
    .owner_provider_id = "matching",
    .handler = matching_command_handler,
    .description = "Switch to Matching tab",
    .help_format = "matching, m, names, nm",
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
    s_matching_provider.handle_key = handle_matching_tab_keys;
    s_matching_provider.shortcut_hint = "Shortcuts: Ctrl+E=Edit name  Ctrl+P=Edit pattern  Ctrl+D=Delete";
    s_matching_provider_id = cofi_register_tab_provider(&s_matching_provider);
    if (s_matching_provider_id >= 0) {
        cofi_register_command(&s_matching_command);
    }
}
