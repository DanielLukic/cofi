#include "overlay_hotkey_edit.h"

#include <string.h>

#include "display.h"
#include "filter.h"
#include "hotkeys.h"
#include "log.h"
#include "overlay_manager.h"
#include "selection.h"

void create_hotkey_edit_overlay_content(GtkWidget *parent_container, AppData *app) {
    if (app->current_tab != TAB_HOTKEYS || app->filtered_hotkeys_count == 0) {
        GtkWidget *error_label = gtk_label_new("No hotkey binding selected");
        gtk_box_pack_start(GTK_BOX(parent_container), error_label, FALSE, FALSE, 10);
        return;
    }

    HotkeyBinding *binding = &app->filtered_hotkeys[app->selection.hotkeys_index];

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title_label = gtk_label_new("Edit Hotkey Command");
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    char info_text[128];
    snprintf(info_text, sizeof(info_text), "Key: %s", binding->key);
    GtkWidget *info_label = gtk_label_new(info_text);
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

    GtkWidget *name_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(name_entry), binding->command);
    gtk_editable_select_region(GTK_EDITABLE(name_entry), 0, -1);
    gtk_widget_set_size_request(name_entry, 400, -1);
    gtk_box_pack_start(GTK_BOX(vbox), name_entry, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(parent_container), "name_entry", name_entry);
    g_object_set_data_full(G_OBJECT(parent_container), "hotkey_key",
                           g_strdup(binding->key), g_free);

    GtkWidget *inst_label = gtk_label_new("Press Enter to save, Escape to cancel");
    gtk_widget_set_opacity(inst_label, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
    log_info("Hotkey edit overlay created for key: %s", binding->key);
}

gboolean handle_hotkey_edit_key_press(AppData *app, GdkEventKey *event) {
    if (event->keyval != GDK_KEY_Return && event->keyval != GDK_KEY_KP_Enter) {
        return FALSE;
    }

    GtkWidget *name_entry = g_object_get_data(G_OBJECT(app->dialog_container), "name_entry");
    const char *hotkey_key = g_object_get_data(G_OBJECT(app->dialog_container), "hotkey_key");

    if (!name_entry || !hotkey_key) {
        log_error("Hotkey edit widgets not found");
        hide_overlay(app);
        return TRUE;
    }

    const char *new_command = gtk_entry_get_text(GTK_ENTRY(name_entry));
    if (!new_command || strlen(new_command) == 0) {
        log_info("Empty command entered, canceling edit");
        hide_overlay(app);
        return TRUE;
    }

    int idx = find_hotkey_binding(&app->hotkey_config, hotkey_key);
    if (idx >= 0) {
        strncpy(app->hotkey_config.bindings[idx].command, new_command,
                sizeof(app->hotkey_config.bindings[idx].command) - 1);
        app->hotkey_config.bindings[idx].command[
            sizeof(app->hotkey_config.bindings[idx].command) - 1] = '\0';
        save_hotkey_config(&app->hotkey_config);
        regrab_hotkeys(app);
        log_info("USER: Updated hotkey '%s' command to '%s'", hotkey_key, new_command);
    } else {
        log_error("Hotkey '%s' not found for editing", hotkey_key);
    }

    hide_overlay(app);

    const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
    filter_hotkeys(app, current_filter);
    validate_selection(app);
    update_scroll_position(app);
    update_display(app);

    return TRUE;
}
