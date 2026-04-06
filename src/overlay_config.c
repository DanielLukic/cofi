#include "overlay_config.h"

#include "display.h"
#include "filter.h"
#include "log.h"
#include "overlay_manager.h"
#include "selection.h"

void create_config_edit_overlay_content(GtkWidget *parent_container, AppData *app) {
    if (app->current_tab != TAB_CONFIG || app->filtered_config_count == 0) {
        GtkWidget *error_label = gtk_label_new("No config option selected");
        gtk_box_pack_start(GTK_BOX(parent_container), error_label, FALSE, FALSE, 10);
        return;
    }

    ConfigEntry *entry = &app->filtered_config[app->selection.config_index];
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title_label = gtk_label_new("Edit Config Value");
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    char info_text[256];
    snprintf(info_text, sizeof(info_text), "Key: %s", entry->key);
    GtkWidget *info_label = gtk_label_new(info_text);
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

    GtkWidget *name_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(name_entry), entry->value);
    gtk_editable_select_region(GTK_EDITABLE(name_entry), 0, -1);
    gtk_widget_set_size_request(name_entry, 300, -1);
    gtk_box_pack_start(GTK_BOX(vbox), name_entry, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(parent_container), "name_entry", name_entry);
    g_object_set_data_full(G_OBJECT(parent_container), "config_key",
                           g_strdup(entry->key), g_free);

    GtkWidget *inst_label = gtk_label_new("Press Enter to apply, Escape to cancel");
    gtk_widget_set_opacity(inst_label, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
    log_info("Config edit overlay created for key: %s", entry->key);
}

gboolean handle_config_edit_key_press(AppData *app, GdkEventKey *event) {
    if (event->keyval != GDK_KEY_Return && event->keyval != GDK_KEY_KP_Enter) {
        return FALSE;
    }

    GtkWidget *name_entry = g_object_get_data(G_OBJECT(app->dialog_container), "name_entry");
    const char *config_key = g_object_get_data(G_OBJECT(app->dialog_container), "config_key");

    if (!name_entry || !config_key) {
        log_error("Config edit widgets not found");
        hide_overlay(app);
        return TRUE;
    }

    const char *new_value = gtk_entry_get_text(GTK_ENTRY(name_entry));
    if (!new_value) {
        hide_overlay(app);
        return TRUE;
    }

    char err_buf[128];
    if (apply_config_setting(&app->config, config_key, new_value, err_buf, sizeof(err_buf))) {
        save_config(&app->config);
        log_info("USER: Set config '%s' = '%s'", config_key, new_value);
    } else {
        log_error("Failed to set config '%s': %s", config_key, err_buf);
    }

    hide_overlay(app);

    const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
    filter_config(app, current_filter);
    validate_selection(app);
    update_scroll_position(app);
    update_display(app);

    return TRUE;
}
