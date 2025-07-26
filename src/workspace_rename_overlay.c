#include "workspace_rename_overlay.h"
#include "app_data.h"
#include "log.h"
#include "x11_utils.h"
#include <gtk/gtk.h>
#include <string.h>

// Helper function to focus the workspace rename entry
static gboolean focus_workspace_rename_entry(gpointer user_data) {
    AppData *app = (AppData *)user_data;
    
    if (app->current_overlay == OVERLAY_WORKSPACE_RENAME) {
        GtkWidget *entry = g_object_get_data(G_OBJECT(app->dialog_container), "rename-entry");
        if (entry && GTK_IS_ENTRY(entry)) {
            gtk_widget_grab_focus(entry);
            // Set cursor position to end without selection
            gtk_editable_set_position(GTK_EDITABLE(entry), -1);
            gtk_editable_select_region(GTK_EDITABLE(entry), -1, -1);
        }
    }
    
    return G_SOURCE_REMOVE; // Run only once
}

// Create workspace rename overlay content
void create_workspace_rename_overlay_content(GtkWidget *parent_container, AppData *app, int workspace_index) {
    // Header
    char *header_text = g_strdup_printf("<b>Rename Workspace %d</b>", workspace_index);
    GtkWidget *header_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header_label), header_text);
    gtk_widget_set_halign(header_label, GTK_ALIGN_CENTER);
    g_free(header_text);
    
    gtk_box_pack_start(GTK_BOX(parent_container), header_label, FALSE, FALSE, 10);
    
    // Separator
    GtkWidget *separator1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(parent_container), separator1, FALSE, FALSE, 10);
    
    // Current name info
    int desktop_count = 0;
    char **desktop_names = get_desktop_names(app->display, &desktop_count);
    char *current_name = "Unnamed";
    
    if (desktop_names && workspace_index < desktop_count && desktop_names[workspace_index]) {
        current_name = desktop_names[workspace_index];
    }
    
    char *info_text = g_strdup_printf("<b>Current name:</b> %s", current_name);
    GtkWidget *info_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(info_label), info_text);
    gtk_widget_set_halign(info_label, GTK_ALIGN_CENTER);
    g_free(info_text);
    
    gtk_box_pack_start(GTK_BOX(parent_container), info_label, FALSE, FALSE, 10);
    
    // Entry field
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), current_name);
    gtk_entry_set_max_length(GTK_ENTRY(entry), 64);
    gtk_widget_set_size_request(entry, 300, -1);
    
    // Store workspace index and entry widget for later use
    g_object_set_data(G_OBJECT(parent_container), "workspace-index", GINT_TO_POINTER(workspace_index));
    g_object_set_data(G_OBJECT(parent_container), "rename-entry", entry);
    
    gtk_box_pack_start(GTK_BOX(parent_container), entry, FALSE, FALSE, 20);
    
    // Separator
    GtkWidget *separator2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(parent_container), separator2, FALSE, FALSE, 10);
    
    // Instructions
    GtkWidget *instructions = gtk_label_new("[Enter to save, Escape to cancel]");
    gtk_widget_set_halign(instructions, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(parent_container), instructions, FALSE, FALSE, 10);
    
    // Free desktop names if allocated
    if (desktop_names) {
        for (int i = 0; i < desktop_count; i++) {
            if (desktop_names[i]) {
                XFree(desktop_names[i]);
            }
        }
        free(desktop_names);
    }
    
    // Schedule focus for the entry widget
    g_idle_add(focus_workspace_rename_entry, app);
}

// Handle key press in workspace rename overlay
gboolean handle_workspace_rename_key_press(AppData *app, guint keyval) {
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        // Get the entry widget and workspace index
        GtkWidget *entry = g_object_get_data(G_OBJECT(app->dialog_container), "rename-entry");
        gpointer index_ptr = g_object_get_data(G_OBJECT(app->dialog_container), "workspace-index");
        
        if (!entry || !index_ptr) {
            log_debug("Missing entry widget or workspace index");
            return TRUE;
        }
        
        int workspace_index = GPOINTER_TO_INT(index_ptr);
        const char *new_name = gtk_entry_get_text(GTK_ENTRY(entry));
        
        if (!new_name || strlen(new_name) == 0) {
            log_debug("Empty workspace name provided");
            return TRUE;
        }
        
        // Get current desktop names
        int desktop_count = get_number_of_desktops(app->display);
        char **desktop_names = get_desktop_names(app->display, &desktop_count);
        
        if (!desktop_names) {
            // Create empty array if none exists
            desktop_names = calloc(desktop_count, sizeof(char*));
            for (int i = 0; i < desktop_count; i++) {
                char default_name[32];
                snprintf(default_name, sizeof(default_name), "Desktop %d", i + 1);
                desktop_names[i] = strdup(default_name);
            }
        }
        
        // Update the specific workspace name
        if (workspace_index < desktop_count) {
            if (desktop_names[workspace_index]) {
                XFree(desktop_names[workspace_index]);
            }
            desktop_names[workspace_index] = strdup(new_name);
            
            // Set the new names
            if (set_desktop_names(app->display, desktop_names, desktop_count) == COFI_SUCCESS) {
                log_info("Set workspace %d name to: %s", workspace_index, new_name);
            } else {
                log_error("Failed to set workspace names");
            }
        }
        
        // Cleanup
        for (int i = 0; i < desktop_count; i++) {
            if (desktop_names[i]) {
                free(desktop_names[i]);
            }
        }
        free(desktop_names);
        
        return TRUE; // Return TRUE to indicate overlay should be hidden
    }
    
    return FALSE; // Let other handlers process this key
}