#include "names_provider.h"

#include "cofi_tab_provider.h"
#include "command_mode.h"
#include "command_registry.h"
#include "filter_names.h"
#include "log.h"
#include "match.h"
#include "named_window.h"
#include "overlay_manager.h"
#include "selection.h"
#include "tab_switching.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

static int names_row_count(AppData *app) {
    if (!app) return 0;
    return app->filtered_names_count > 0 ? app->filtered_names_count : 1;
}

static NamedWindow *name_at_row(AppData *app, int raw_idx) {
    if (!app || raw_idx < 0 || raw_idx >= app->filtered_names_count) {
        return NULL;
    }
    return &app->filtered_names[raw_idx];
}

static void names_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    NamedWindow *named = name_at_row(app, raw_idx);
    if (!named) {
        out->cell_count = 1;
        out->cells[0].text = "No named windows found";
        out->row_flags = 0;
        return;
    }

    static char window_id[32];
    if (named->assigned) {
        snprintf(window_id, sizeof(window_id), "0x%lx", named->id);
    } else {
        g_strlcpy(window_id, "* NONE *", sizeof(window_id));
    }

    out->cell_count = 4;
    out->cells[0].text = named->custom_name;
    out->cells[0].width_hint = 20;
    out->cells[1].text = named->original_title;
    out->cells[1].width_hint = 45;
    out->cells[2].text = named->class_name;
    out->cells[2].width_hint = 18;
    out->cells[3].text = window_id;
    out->row_flags = COFI_ROW_ACTIONABLE;
}

static const char *names_match_string(AppData *app, int raw_idx) {
    NamedWindow *entry = name_at_row(app, raw_idx);
    static char searchable[1024];
    if (!entry) return "";
    g_snprintf(searchable, sizeof(searchable), "%s %s %s %s",
               entry->custom_name, entry->original_title,
               entry->class_name, entry->instance);
    return searchable;
}

static const char *names_row_identity(AppData *app, int raw_idx) {
    NamedWindow *entry = name_at_row(app, raw_idx);
    static char identity[256];
    if (!entry) return "";
    g_snprintf(identity, sizeof(identity), "name:%s:%lx",
               entry->custom_name, entry->id);
    return identity;
}

static void names_on_enter(AppData *app) {
    if (!app || !app->entry) return;
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry),
                                   "Type to filter named windows...");
    filter_names(app, "");
}

static void names_on_query_changed(AppData *app, const char *query) {
    filter_names(app, query);
    reset_selection(app);
}

NamedWindow *names_selected_entry(AppData *app) {
    if (!app || app->filtered_names_count <= 0) return NULL;

    int idx = app->selection.provider_index;
    if (idx < 0) idx = 0;
    if (idx >= app->filtered_names_count) idx = app->filtered_names_count - 1;
    app->selection.provider_index = idx;

    return &app->filtered_names[idx];
}

int names_selected_manager_index(AppData *app) {
    NamedWindow *named = names_selected_entry(app);
    if (!app || !named) return -1;

    int manager_index = -1;
    if (named->id != 0) {
        manager_index = find_named_window_index(&app->names, named->id);
    }
    if (manager_index < 0) {
        manager_index = find_named_window_by_name(&app->names, named->custom_name);
    }
    return manager_index;
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
    if (app->current_tab != TAB_NAMES) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_e && (event->state & GDK_CONTROL_MASK)) {
        if (!names_selected_entry(app)) {
            return FALSE;
        }
        show_name_edit_overlay(app);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_d && (event->state & GDK_CONTROL_MASK)) {
        NamedWindow *named = names_selected_entry(app);
        if (!named) {
            log_debug("Names Ctrl+D ignored: no rows to delete");
            return FALSE;
        }

        int manager_index = names_selected_manager_index(app);
        log_info("Names Ctrl+D: showing delete confirm for '%s' (mgr_idx=%d, sel=%d/%d)",
                 named->custom_name, manager_index,
                 app->selection.provider_index, app->filtered_names_count);
        show_name_delete_overlay(app, named->custom_name, manager_index);
        return TRUE;
    }

    return FALSE;
}

static CofiTabProvider s_names_provider;

static gboolean names_command_handler(AppData *app,
                                      WindowInfo *window __attribute__((unused)),
                                      const char *args __attribute__((unused))) {
    if (!app) return FALSE;

    exit_command_mode(app);
    app->prefix_origin_tab = app->current_tab;
    surface_tab(app, (TabMode)s_names_provider.tab_mode);
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
    s_names_provider.tab_mode = TAB_NAMES;
    s_names_provider.id = "names";
    s_names_provider.display_name = "NAMES";
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
    s_names_provider.shortcut_hint = "Shortcuts: Ctrl+E=Edit name  Ctrl+D=Delete name";
    if (cofi_register_tab_provider(&s_names_provider) >= 0) {
        cofi_register_command(&s_names_command);
    }
}
