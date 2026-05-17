#include "harpoon_provider.h"

#include "command_mode.h"
#include "command_registry.h"
#include "cofi_tab_provider.h"
#include "daemon_socket.h"
#include "log.h"
#include "match.h"
#include "overlay_manager.h"
#include "selection.h"
#include "tab_switching.h"

#include <stdio.h>
#include <string.h>

static void format_slot_key(int slot_idx, char *out, size_t out_size) {
    if (!out || out_size == 0) return;

    if (slot_idx < 10) {
        snprintf(out, out_size, "%d", slot_idx);
    } else {
        snprintf(out, out_size, "%c", 'a' + (slot_idx - 10));
    }
}

static CofiTabProvider s_harpoon_provider;
static int s_harpoon_provider_id = -1;

TabMode harpoon_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_harpoon_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

static gboolean harpoon_command_handler(AppData *app,
                                        WindowInfo *window __attribute__((unused)),
                                        const char *args __attribute__((unused))) {
    if (!app) return FALSE;
    exit_command_mode(app);
    app->prefix_origin_tab = app->current_tab;
    surface_tab(app, harpoon_tab_mode());
    return FALSE;
}

static const CommandSpec s_harpoon_command = {
    .primary = "harpoon",
    .aliases = {"hp", NULL},
    .owner_provider_id = "harpoon",
    .handler = harpoon_command_handler,
    .description = "Switch to Harpoon tab",
    .help_format = "harpoon, hp",
    .keeps_open_on_hotkey_auto = 1
};

void filter_harpoon(AppData *app, const char *filter) {
    if (!app) return;

    app->filtered_harpoon_count = 0;

    for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
        HarpoonSlot *slot = &app->harpoon.slots[i];
        if (!slot->assigned) continue;

        char slot_key[4];
        char searchable[1024];
        format_slot_key(i, slot_key, sizeof(slot_key));
        snprintf(searchable, sizeof(searchable), "%s %s %s %s",
                 slot_key, slot->title, slot->class_name, slot->instance);

        if (!filter || !*filter || has_match(filter, searchable)) {
            app->filtered_harpoon[app->filtered_harpoon_count] = *slot;
            app->filtered_harpoon_indices[app->filtered_harpoon_count] = i;
            app->filtered_harpoon_count++;
        }
    }
}

static int harpoon_row_count(AppData *app) {
    if (!app) return 0;
    return app->filtered_harpoon_count > 0 ? app->filtered_harpoon_count : 1;
}

static HarpoonSlot *harpoon_slot_at_row(AppData *app, int raw_idx,
                                        int *actual_slot_out) {
    if (actual_slot_out) *actual_slot_out = -1;
    if (!app || raw_idx < 0 || raw_idx >= app->filtered_harpoon_count) {
        return NULL;
    }

    if (actual_slot_out) {
        *actual_slot_out = app->filtered_harpoon_indices[raw_idx];
    }
    return &app->filtered_harpoon[raw_idx];
}

HarpoonSlot *harpoon_selected_slot(AppData *app, int *actual_slot_out) {
    if (!app || app->filtered_harpoon_count <= 0) {
        if (actual_slot_out) *actual_slot_out = -1;
        return NULL;
    }

    int idx = app->selection.provider_index;
    if (idx < 0) idx = 0;
    if (idx >= app->filtered_harpoon_count) {
        idx = app->filtered_harpoon_count - 1;
    }
    app->selection.provider_index = idx;

    return harpoon_slot_at_row(app, idx, actual_slot_out);
}

static void harpoon_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    int actual_slot = -1;
    HarpoonSlot *slot = harpoon_slot_at_row(app, raw_idx, &actual_slot);
    if (!slot) {
        out->cell_count = 1;
        out->cells[0].text = "No harpoon slots found";
        return;
    }

    static char slot_key[4];
    format_slot_key(actual_slot, slot_key, sizeof(slot_key));

    out->cell_count = 5;
    out->cells[0].text = slot_key;
    out->cells[0].width_hint = 4;
    out->cells[1].text = slot->title;
    out->cells[1].width_hint = 55;
    out->cells[2].text = slot->class_name;
    out->cells[2].width_hint = 18;
    out->cells[3].text = slot->instance;
    out->cells[3].width_hint = 20;
    out->cells[4].text = slot->type;
    out->cells[4].width_hint = 8;
    out->row_flags = COFI_ROW_ACTIONABLE;
}

static const char *harpoon_match_string(AppData *app, int raw_idx) {
    int actual_slot = -1;
    HarpoonSlot *slot = harpoon_slot_at_row(app, raw_idx, &actual_slot);
    static char searchable[1024];
    char slot_key[4];
    if (!slot) return "";

    format_slot_key(actual_slot, slot_key, sizeof(slot_key));
    snprintf(searchable, sizeof(searchable), "%s %s %s %s",
             slot_key, slot->title, slot->class_name, slot->instance);
    return searchable;
}

static const char *harpoon_row_identity(AppData *app, int raw_idx) {
    static char identity[32];
    int actual_slot = -1;
    harpoon_slot_at_row(app, raw_idx, &actual_slot);
    snprintf(identity, sizeof(identity), "harpoon:%d", actual_slot);
    return identity;
}

static void harpoon_on_enter(AppData *app) {
    if (!app || !app->entry) return;
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry),
                                   "Type to filter harpoon slots...");
    filter_harpoon(app, "");
}

static void harpoon_on_query_changed(AppData *app, const char *query) {
    filter_harpoon(app, query);
    reset_selection(app);
}

gboolean handle_harpoon_tab_keys(GdkEventKey *event, AppData *app) {
    if (!app || app->current_tab != harpoon_tab_mode()) {
        return FALSE;
    }

    int actual_slot = -1;
    HarpoonSlot *slot = harpoon_selected_slot(app, &actual_slot);
    if (!slot || !slot->assigned) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_d && (event->state & GDK_CONTROL_MASK)) {
        show_harpoon_delete_overlay(app, actual_slot);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_e && (event->state & GDK_CONTROL_MASK)) {
        show_harpoon_edit_overlay(app, actual_slot);
        return TRUE;
    }

    return FALSE;
}

void harpoon_provider_register(void) {
    cofi_init_provider_defaults(&s_harpoon_provider);
    s_harpoon_provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    s_harpoon_provider.id = "harpoon";
    s_harpoon_provider.display_name = "HARPOON";
    s_harpoon_provider.delegate_opcode = COFI_OPCODE_HARPOON;
    s_harpoon_provider.required = 0;
    s_harpoon_provider.hidden_by_default = 1;
    s_harpoon_provider.row_count = harpoon_row_count;
    s_harpoon_provider.format_row = harpoon_format_row;
    s_harpoon_provider.match_string = harpoon_match_string;
    s_harpoon_provider.row_identity = harpoon_row_identity;
    s_harpoon_provider.on_enter = harpoon_on_enter;
    s_harpoon_provider.on_query_changed = harpoon_on_query_changed;
    s_harpoon_provider.handle_key = handle_harpoon_tab_keys;
    s_harpoon_provider.shortcut_hint =
        "Shortcuts: Ctrl+E=Edit pattern  Ctrl+D=Delete  (patterns: * = any, . = single char)";
    s_harpoon_provider_id = cofi_register_tab_provider(&s_harpoon_provider);
    if (s_harpoon_provider_id >= 0) {
        cofi_register_command(&s_harpoon_command);
    }
}
