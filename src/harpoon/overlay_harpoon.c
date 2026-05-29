#include "harpoon/overlay_harpoon.h"

#include "ui/display.h"
#include "harpoon/harpoon_config.h"
#include "harpoon/harpoon_provider.h"
#include "core/selection/selection.h"
#include "core/log/log.h"
#include "matching/match_entry.h"
#include "matching/match_entry_config.h"
#include "ui/overlay_confirm.h"

extern void unassign_slot(HarpoonManager *harpoon, int slot);
extern void save_harpoon_slots(const HarpoonManager *harpoon);

static MatchEntry *slot_entry(AppData *app, int slot_index) {
    if (!app || slot_index < 0 || slot_index >= MAX_HARPOON_SLOTS) return NULL;
    HarpoonSlot *slot = &app->harpoon.slots[slot_index];
    if (!slot->assigned || slot->match_id <= 0) return NULL;
    int idx = match_entry_find_index_by_match_id(&app->matching, slot->match_id);
    if (idx < 0 || idx >= app->matching.count) return NULL;
    return &app->matching.entries[idx];
}

static const char *slot_entry_class_name(AppData *app, const MatchEntry *entry) {
    if (!app || !entry || !entry->assigned || entry->bound_x11_id == 0) return "(missing)";
    for (int i = 0; i < app->window_count; i++) {
        if (app->windows[i].id == entry->bound_x11_id) {
            return app->windows[i].class_name[0] ? app->windows[i].class_name : "(missing)";
        }
    }
    return "(missing)";
}

static int s_pending_delete_slot = -1;

static gboolean focus_harpoon_edit_entry(gpointer user_data) {
    AppData *app = (AppData *)user_data;
    GtkWidget *entry = g_object_get_data(G_OBJECT(app->dialog_container), "edit-entry");
    if (entry && GTK_IS_ENTRY(entry)) {
        gtk_widget_grab_focus(entry);
        gtk_editable_set_position(GTK_EDITABLE(entry), -1);
        gtk_editable_select_region(GTK_EDITABLE(entry), -1, -1);
    }

    return G_SOURCE_REMOVE;
}

static void perform_harpoon_delete(AppData *app) {
    int slot_index = s_pending_delete_slot;
    s_pending_delete_slot = -1;
    if (slot_index < 0 || slot_index >= MAX_HARPOON_SLOTS) {
        return;
    }

    int match_id = app->harpoon.slots[slot_index].match_id;
    unassign_slot(&app->harpoon, slot_index);
    match_entry_delete_by_match_id(&app->matching, match_id);
    save_match_entries(&app->matching);
    save_harpoon_slots(&app->harpoon);
    log_info("USER: Deleted harpoon assignment for slot %d", slot_index);
    const char *query = app->entry ? gtk_entry_get_text(GTK_ENTRY(app->entry)) : "";
    preserve_selection(app);
    filter_harpoon(app, query);
    restore_selection(app);
    update_display(app);
}

void show_harpoon_delete_confirm(AppData *app, int slot_index) {
    s_pending_delete_slot = slot_index;

    char slot_name[4];
    if (slot_index < 10) {
        g_snprintf(slot_name, sizeof(slot_name), "%d", slot_index);
    } else {
        g_snprintf(slot_name, sizeof(slot_name), "%c", 'a' + (slot_index - 10));
    }

    MatchEntry *entry = slot_entry(app, slot_index);
    const char *title = entry ? entry->original_title : "(missing)";
    const char *class_name = entry ? slot_entry_class_name(app, entry) : "(missing)";
    char *escaped_title = g_markup_escape_text(title, -1);
    char *escaped_class = g_markup_escape_text(class_name, -1);

    char info[1024];
    g_snprintf(info, sizeof(info),
               "<b>Slot:</b> %s\n"
               "<b>Window:</b> %s\n"
               "<b>Class:</b> %s",
               slot_name, escaped_title, escaped_class);

    g_free(escaped_title);
    g_free(escaped_class);

    show_confirm_overlay(app, "Delete Harpoon Assignment?", info, perform_harpoon_delete);
}

void focus_edit_entry_delayed(AppData *app) {
    g_idle_add((GSourceFunc)focus_harpoon_edit_entry, app);
}
