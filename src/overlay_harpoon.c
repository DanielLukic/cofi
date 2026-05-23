#include "overlay_harpoon.h"

#include "display.h"
#include "gtk_utils.h"
#include "harpoon_config.h"
#include "harpoon_provider.h"
#include "log.h"
#include "match_entry.h"
#include "match_entry_config.h"
#include "matching_gc.h"
#include "overlay_confirm.h"
#include "overlay_manager.h"
#include "utils.h"

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

static int s_pending_delete_slot = -1;

static gboolean focus_harpoon_edit_entry(gpointer user_data) {
    AppData *app = (AppData *)user_data;

    if (app->current_overlay == OVERLAY_HARPOON_EDIT) {
        GtkWidget *entry = g_object_get_data(G_OBJECT(app->dialog_container), "edit-entry");
        if (entry && GTK_IS_ENTRY(entry)) {
            gtk_widget_grab_focus(entry);
            gtk_editable_set_position(GTK_EDITABLE(entry), -1);
            gtk_editable_select_region(GTK_EDITABLE(entry), -1, -1);
        }
    }

    return G_SOURCE_REMOVE;
}

void create_harpoon_edit_overlay_content(GtkWidget *parent_container,
                                         AppData *app,
                                         int slot_index) {
    char slot_name[4];
    if (slot_index < 10) {
        snprintf(slot_name, sizeof(slot_name), "%d", slot_index);
    } else {
        snprintf(slot_name, sizeof(slot_name), "%c", 'a' + (slot_index - 10));
    }

    char *header = g_strdup_printf("<b>Edit Harpoon Slot: %s</b>", slot_name);
    GtkWidget *header_label = create_markup_label(header, TRUE);
    g_free(header);
    gtk_box_pack_start(GTK_BOX(parent_container), header_label, FALSE, FALSE, 10);

    GtkWidget *separator1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(parent_container), separator1, FALSE, FALSE, 10);

    GtkWidget *entry = gtk_entry_new();
    MatchEntry *match_entry = slot_entry(app, slot_index);
    gtk_entry_set_text(GTK_ENTRY(entry),
                       match_entry ? match_entry->original_title : "");
    gtk_entry_set_max_length(GTK_ENTRY(entry), MAX_TITLE_LEN - 1);
    gtk_widget_set_size_request(entry, 400, -1);
    g_object_set_data(G_OBJECT(parent_container), "edit-entry", entry);
    gtk_box_pack_start(GTK_BOX(parent_container), entry, FALSE, FALSE, 20);

    GtkWidget *separator2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(parent_container), separator2, FALSE, FALSE, 10);

    GtkWidget *instructions = gtk_label_new("Pattern (* wildcard, . single-char) — Enter to save, Esc to cancel");
    gtk_widget_set_halign(instructions, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(parent_container), instructions, FALSE, FALSE, 10);
}

static void perform_harpoon_delete(AppData *app) {
    int slot_index = s_pending_delete_slot;
    s_pending_delete_slot = -1;
    if (slot_index < 0 || slot_index >= MAX_HARPOON_SLOTS) {
        return;
    }

    unassign_slot(&app->harpoon, slot_index);
    matching_run_gc(app);
    save_harpoon_slots(&app->harpoon);
    log_info("USER: Deleted harpoon assignment for slot %d", slot_index);
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
    const char *class_name = entry ? entry->class_name : "(missing)";
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

gboolean handle_harpoon_edit_key_press(AppData *app, GdkEventKey *event) {
    GtkWidget *entry = g_object_get_data(G_OBJECT(app->dialog_container), "edit-entry");

    if (event->keyval == GDK_KEY_Tab || event->keyval == GDK_KEY_ISO_Left_Tab) {
        return TRUE;
    }

    if (event->keyval != GDK_KEY_Return) {
        return FALSE;
    }

    const char *new_title = gtk_entry_get_text(GTK_ENTRY(entry));
    int slot_index = app->harpoon_edit.editing_slot;

    MatchEntry *entry_match = slot_entry(app, slot_index);
    if (!entry_match) {
        hide_overlay(app);
        update_display(app);
        return TRUE;
    }
    safe_string_copy(entry_match->original_title, new_title, MAX_TITLE_LEN);
    entry_match->match_mode = TITLE_MATCH_MODE_GLOB;
    save_match_entries(&app->matching);
    save_harpoon_slots(&app->harpoon);

    log_info("USER: Edited harpoon slot %d title to: %s", slot_index, new_title);

    hide_overlay(app);

    if (app->current_tab == harpoon_tab_mode()) {
        const char *filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
        gtk_entry_set_text(GTK_ENTRY(app->entry), "");
        gtk_entry_set_text(GTK_ENTRY(app->entry), filter);
    }
    update_display(app);

    return TRUE;
}

void focus_harpoon_edit_entry_delayed(AppData *app) {
    g_idle_add((GSourceFunc)focus_harpoon_edit_entry, app);
}
