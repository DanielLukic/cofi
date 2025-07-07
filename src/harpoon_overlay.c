#include "harpoon_overlay.h"
#include "app_data.h"
#include "log.h"
#include "utils.h"
#include "harpoon_config.h"
#include <gtk/gtk.h>

// Forward declarations
extern void unassign_slot(HarpoonManager *harpoon, int slot); // From harpoon.c
extern void save_harpoon_slots(const HarpoonManager *harpoon); // From harpoon_config.c

// Helper function to focus the harpoon edit entry
static gboolean focus_harpoon_edit_entry(gpointer user_data) {
    AppData *app = (AppData *)user_data;
    
    if (app->current_overlay == OVERLAY_HARPOON_EDIT) {
        GtkWidget *entry = g_object_get_data(G_OBJECT(app->dialog_container), "edit-entry");
        if (entry && GTK_IS_ENTRY(entry)) {
            gtk_widget_grab_focus(entry);
            // Set cursor position to end without selection
            gtk_editable_set_position(GTK_EDITABLE(entry), -1);
            gtk_editable_select_region(GTK_EDITABLE(entry), -1, -1);
        }
    }
    
    return G_SOURCE_REMOVE; // Run only once
}

// Create harpoon delete overlay content
void create_harpoon_delete_overlay_content(GtkWidget *parent_container, AppData *app, int slot_index) {
    HarpoonSlot *slot = &app->harpoon.slots[slot_index];
    
    // Header
    GtkWidget *header_label = gtk_label_new(NULL);
    gtk_widget_set_halign(header_label, GTK_ALIGN_CENTER);
    
    char *header_markup = g_strdup_printf("<b>Delete Harpoon Assignment?</b>");
    gtk_label_set_markup(GTK_LABEL(header_label), header_markup);
    g_free(header_markup);
    
    gtk_box_pack_start(GTK_BOX(parent_container), header_label, FALSE, FALSE, 10);
    
    // Separator
    GtkWidget *separator1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(parent_container), separator1, FALSE, FALSE, 10);
    
    // Slot info
    char slot_name[4];
    if (slot_index < 10) {
        snprintf(slot_name, sizeof(slot_name), "%d", slot_index);
    } else {
        snprintf(slot_name, sizeof(slot_name), "%c", 'a' + (slot_index - 10));
    }
    
    char *escaped_title = g_markup_escape_text(slot->title, -1);
    char *slot_info = g_strdup_printf(
        "<b>Slot:</b> %s\n"
        "<b>Window:</b> %s\n"
        "<b>Class:</b> %s",
        slot_name, escaped_title, slot->class_name);
    
    GtkWidget *info_label = gtk_label_new(NULL);
    gtk_widget_set_halign(info_label, GTK_ALIGN_CENTER);
    gtk_label_set_markup(GTK_LABEL(info_label), slot_info);
    gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
    
    g_free(escaped_title);
    g_free(slot_info);
    
    gtk_box_pack_start(GTK_BOX(parent_container), info_label, FALSE, FALSE, 10);
    
    // Separator
    GtkWidget *separator2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(parent_container), separator2, FALSE, FALSE, 10);
    
    // Instructions
    GtkWidget *instructions = gtk_label_new("[Press Y or Ctrl+D to confirm, N or Esc to cancel]");
    gtk_widget_set_halign(instructions, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(parent_container), instructions, FALSE, FALSE, 10);
}

// Create harpoon edit overlay content
void create_harpoon_edit_overlay_content(GtkWidget *parent_container, AppData *app, int slot_index) {
    HarpoonSlot *slot = &app->harpoon.slots[slot_index];
    
    // Header
    char slot_name[4];
    if (slot_index < 10) {
        snprintf(slot_name, sizeof(slot_name), "%d", slot_index);
    } else {
        snprintf(slot_name, sizeof(slot_name), "%c", 'a' + (slot_index - 10));
    }
    
    char *header = g_strdup_printf("<b>Edit Harpoon Slot: %s</b>", slot_name);
    GtkWidget *header_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header_label), header);
    gtk_widget_set_halign(header_label, GTK_ALIGN_CENTER);
    g_free(header);
    
    gtk_box_pack_start(GTK_BOX(parent_container), header_label, FALSE, FALSE, 10);
    
    // Separator
    GtkWidget *separator1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(parent_container), separator1, FALSE, FALSE, 10);
    
    // Entry widget
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), slot->title);
    gtk_entry_set_max_length(GTK_ENTRY(entry), MAX_TITLE_LEN - 1);
    gtk_widget_set_size_request(entry, 400, -1);
    
    // Store entry reference for key handler
    g_object_set_data(G_OBJECT(parent_container), "edit-entry", entry);
    
    gtk_box_pack_start(GTK_BOX(parent_container), entry, FALSE, FALSE, 20);
    
    // Separator
    GtkWidget *separator2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(parent_container), separator2, FALSE, FALSE, 10);
    
    // Instructions
    GtkWidget *instructions = gtk_label_new("Press Enter to save, Escape to cancel");
    gtk_widget_set_halign(instructions, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(parent_container), instructions, FALSE, FALSE, 10);
    
    // Focus will be handled by the idle callback to ensure proper widget realization
}

// Handle key press events for harpoon delete overlay
gboolean handle_harpoon_delete_key_press(AppData *app, GdkEventKey *event) {
    // Handle Y or Ctrl+D to confirm
    if (event->keyval == GDK_KEY_y || event->keyval == GDK_KEY_Y ||
        ((event->state & GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_d || event->keyval == GDK_KEY_D))) {
        
        int slot_index = app->harpoon_delete.delete_slot;
        
        log_debug("=== EXECUTING HARPOON DELETE ===");
        log_debug("Deleting harpoon assignment for slot %d", slot_index);
        
        // Remove the harpoon assignment
        unassign_slot(&app->harpoon, slot_index);
        save_harpoon_slots(&app->harpoon);
        
        log_info("USER: Deleted harpoon assignment for slot %d", slot_index);
        
        // Clear delete state
        app->harpoon_delete.pending_delete = FALSE;
        app->harpoon_delete.delete_slot = -1;
        
        // Update the harpoon display
        // Since filter_harpoon is static in main.c, we'll trigger filtering by
        // simulating entry change which will call the proper filtering
        if (app->current_tab == TAB_HARPOON) {
            const char *current_text = gtk_entry_get_text(GTK_ENTRY(app->entry));
            // Force a refresh by clearing and re-setting the text
            gtk_entry_set_text(GTK_ENTRY(app->entry), "");
            gtk_entry_set_text(GTK_ENTRY(app->entry), current_text);
        }
        
        return TRUE;
    }
    
    // Handle N to cancel
    if (event->keyval == GDK_KEY_n || event->keyval == GDK_KEY_N) {
        log_debug("User cancelled harpoon delete");
        
        // Clear delete state
        app->harpoon_delete.pending_delete = FALSE;
        app->harpoon_delete.delete_slot = -1;
        
        return TRUE;
    }
    
    return FALSE; // Let Escape be handled by main overlay handler
}

// Handle key press in edit overlay
gboolean handle_harpoon_edit_key_press(AppData *app, GdkEventKey *event) {
    GtkWidget *entry = g_object_get_data(G_OBJECT(app->dialog_container), "edit-entry");
    
    // Block Tab key to prevent focus escaping
    if (event->keyval == GDK_KEY_Tab || event->keyval == GDK_KEY_ISO_Left_Tab) {
        return TRUE; // Consume the event
    }
    
    if (event->keyval == GDK_KEY_Return) {
        // Save the edited title
        const char *new_title = gtk_entry_get_text(GTK_ENTRY(entry));
        int slot_index = app->harpoon_edit.editing_slot;
        
        // Update slot title directly (no wildcard conversion needed here)
        safe_string_copy(app->harpoon.slots[slot_index].title, new_title, MAX_TITLE_LEN);
        save_harpoon_slots(&app->harpoon);
        
        log_info("USER: Edited harpoon slot %d title to: %s", slot_index, new_title);
        
        // Update display
        if (app->current_tab == TAB_HARPOON) {
            const char *filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
            gtk_entry_set_text(GTK_ENTRY(app->entry), "");
            gtk_entry_set_text(GTK_ENTRY(app->entry), filter);
        }
        
        return TRUE;
    }
    
    // Let GTK Entry handle all other keys
    return FALSE;
}

// Focus harpoon edit entry with delay (called from overlay_manager)
void focus_harpoon_edit_entry_delayed(AppData *app) {
    g_idle_add((GSourceFunc)focus_harpoon_edit_entry, app);
}