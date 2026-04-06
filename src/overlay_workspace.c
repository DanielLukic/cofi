#include "overlay_workspace.h"

#include <string.h>

#include "gtk_utils.h"
#include "log.h"
#include "x11_utils.h"

static gboolean focus_workspace_rename_entry(gpointer user_data) {
    AppData *app = (AppData *)user_data;

    if (app->current_overlay != OVERLAY_WORKSPACE_RENAME) {
        return G_SOURCE_REMOVE;
    }

    GtkWidget *entry = g_object_get_data(G_OBJECT(app->dialog_container), "rename-entry");
    if (entry && GTK_IS_ENTRY(entry)) {
        gtk_widget_grab_focus(entry);
        gtk_editable_set_position(GTK_EDITABLE(entry), -1);
        gtk_editable_select_region(GTK_EDITABLE(entry), -1, -1);
    }

    return G_SOURCE_REMOVE;
}

void create_workspace_rename_overlay_content(GtkWidget *parent_container,
                                             AppData *app,
                                             int workspace_index) {
    char *header_text = g_strdup_printf("<b>Rename Workspace %d</b>", workspace_index);
    GtkWidget *header_label = create_markup_label(header_text, TRUE);
    g_free(header_text);
    gtk_box_pack_start(GTK_BOX(parent_container), header_label, FALSE, FALSE, 10);

    add_horizontal_separator(parent_container);

    int desktop_count = 0;
    char **desktop_names = get_desktop_names(app->display, &desktop_count);
    char *current_name = "Unnamed";

    if (desktop_names && workspace_index < desktop_count && desktop_names[workspace_index]) {
        current_name = desktop_names[workspace_index];
    }

    char *info_text = g_strdup_printf("<b>Current name:</b> %s", current_name);
    GtkWidget *info_label = create_markup_label(info_text, TRUE);
    g_free(info_text);
    gtk_box_pack_start(GTK_BOX(parent_container), info_label, FALSE, FALSE, 10);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), current_name);
    gtk_entry_set_max_length(GTK_ENTRY(entry), 64);
    gtk_widget_set_size_request(entry, 300, -1);

    g_object_set_data(G_OBJECT(parent_container), "workspace-index", GINT_TO_POINTER(workspace_index));
    g_object_set_data(G_OBJECT(parent_container), "rename-entry", entry);

    gtk_box_pack_start(GTK_BOX(parent_container), entry, FALSE, FALSE, 20);
    add_horizontal_separator(parent_container);

    GtkWidget *instructions = create_centered_label("[Enter to save, Escape to cancel]");
    gtk_box_pack_start(GTK_BOX(parent_container), instructions, FALSE, FALSE, 10);

    if (desktop_names) {
        for (int i = 0; i < desktop_count; i++) {
            if (desktop_names[i]) {
                XFree(desktop_names[i]);
            }
        }
        free(desktop_names);
    }

    g_idle_add(focus_workspace_rename_entry, app);
}

gboolean handle_workspace_rename_key_press(AppData *app, guint keyval) {
    if (keyval != GDK_KEY_Return && keyval != GDK_KEY_KP_Enter) {
        return FALSE;
    }

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

    int desktop_count = get_number_of_desktops(app->display);
    char **desktop_names = get_desktop_names(app->display, &desktop_count);
    if (!desktop_names) {
        desktop_names = calloc(desktop_count, sizeof(char *));
        for (int i = 0; i < desktop_count; i++) {
            char default_name[32];
            snprintf(default_name, sizeof(default_name), "Desktop %d", i + 1);
            desktop_names[i] = strdup(default_name);
        }
    }

    if (workspace_index < desktop_count) {
        if (desktop_names[workspace_index]) {
            XFree(desktop_names[workspace_index]);
        }
        desktop_names[workspace_index] = strdup(new_name);

        if (set_desktop_names(app->display, desktop_names, desktop_count) == COFI_SUCCESS) {
            log_info("Set workspace %d name to: %s", workspace_index, new_name);
        } else {
            log_error("Failed to set workspace names");
        }
    }

    for (int i = 0; i < desktop_count; i++) {
        if (desktop_names[i]) {
            free(desktop_names[i]);
        }
    }
    free(desktop_names);

    return TRUE;
}
