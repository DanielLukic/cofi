#include "config/overlay_config.h"

#include "providers/cofi_tab_provider.h"
#include "config/config_provider.h"
#include "ui/display.h"
#include "core/log/log.h"
#include "ui/overlay_manager.h"
#include "core/selection/selection.h"

static void refresh_config_tab_after_provider_change(AppData *app) {
    const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
    filter_config(app, current_filter);
    config_select_key(app, "disabled_providers");
    validate_selection(app);
    update_scroll_position(app);
    update_display(app);
}

static void update_provider_enablement_label(AppData *app) {
    GtkWidget *label = g_object_get_data(G_OBJECT(app->dialog_container), "provider_list_label");
    if (!label) return;

    GString *text = g_string_new(NULL);
    for (int i = 0; i < app->provider_enablement.count; i++) {
        int provider_id = app->provider_enablement.provider_ids[i];
        const CofiTabProvider *provider = cofi_get_provider(provider_id);
        if (!provider) continue;

        const char *name = provider->display_name ? provider->display_name : provider->id;
        const char *cursor = (i == app->provider_enablement.selected) ? "> " : "  ";
        const char *check = app->provider_enablement.enabled[i] ? "[x]" : "[ ]";
        g_string_append_printf(text, "%s%s %s", cursor, check, name);
        if (!cofi_provider_is_disableable(provider_id))
            g_string_append(text, " (required)");
        g_string_append_c(text, '\n');
    }

    gtk_label_set_text(GTK_LABEL(label), text->str);
    g_string_free(text, TRUE);
}

static void init_provider_enablement_state(AppData *app) {
    app->provider_enablement.selected = 0;
    app->provider_enablement.count = 0;

    for (int i = 0; i < cofi_provider_count(); i++) {
        if (app->provider_enablement.count >= MAX_PROVIDER_ENABLEMENT_ROWS) break;
        const CofiTabProvider *provider = cofi_get_provider(i);
        if (!provider || !provider->id) continue;

        int row = app->provider_enablement.count++;
        app->provider_enablement.provider_ids[row] = i;
        app->provider_enablement.enabled[row] = cofi_provider_is_enabled(i);
    }
}

void create_config_edit_overlay_content(GtkWidget *parent_container, AppData *app) {
    if (app->current_tab != config_tab_mode()) {
        GtkWidget *error_label = gtk_label_new("No config option selected");
        gtk_box_pack_start(GTK_BOX(parent_container), error_label, FALSE, FALSE, 10);
        return;
    }

    ConfigEntry *entry = config_selected_entry(app);
    if (!config_entry_allows_edit(entry)) {
        GtkWidget *error_label = gtk_label_new("No config option selected");
        gtk_box_pack_start(GTK_BOX(parent_container), error_label, FALSE, FALSE, 10);
        return;
    }

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

void create_provider_enablement_overlay_content(GtkWidget *parent_container, AppData *app) {
    init_provider_enablement_state(app);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title_label = gtk_label_new("Provider Plugins");
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    GtkWidget *provider_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(provider_label), 0.0f);
    gtk_widget_set_name(provider_label, "provider-list-label");
    gtk_box_pack_start(GTK_BOX(vbox), provider_label, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(parent_container), "provider_list_label", provider_label);

    GtkWidget *inst_label = gtk_label_new("Space=toggle  Enter=apply  Esc=cancel");
    gtk_widget_set_opacity(inst_label, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
    update_provider_enablement_label(app);
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

    char selected_key[CONFIG_KEY_LEN];
    g_strlcpy(selected_key, config_key, sizeof(selected_key));
    hide_overlay(app);

    const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
    filter_config(app, current_filter);
    config_select_key(app, selected_key);
    validate_selection(app);
    update_scroll_position(app);
    update_display(app);

    return TRUE;
}

gboolean handle_provider_enablement_key_press(AppData *app, GdkEventKey *event) {
    if (!app || app->provider_enablement.count <= 0) {
        hide_overlay(app);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_k) {
        if (app->provider_enablement.selected > 0)
            app->provider_enablement.selected--;
        update_provider_enablement_label(app);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_j) {
        if (app->provider_enablement.selected < app->provider_enablement.count - 1)
            app->provider_enablement.selected++;
        update_provider_enablement_label(app);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_space) {
        int row = app->provider_enablement.selected;
        int provider_id = app->provider_enablement.provider_ids[row];
        if (cofi_provider_is_disableable(provider_id)) {
            app->provider_enablement.enabled[row] = !app->provider_enablement.enabled[row];
            update_provider_enablement_label(app);
        }
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        for (int i = 0; i < app->provider_enablement.count; i++) {
            cofi_set_provider_enabled(app->provider_enablement.provider_ids[i],
                                      app->provider_enablement.enabled[i]);
        }
        cofi_build_disabled_providers_string(app->config.disabled_providers,
                                             sizeof(app->config.disabled_providers));
        save_config(&app->config);
        hide_overlay(app);
        refresh_config_tab_after_provider_change(app);
        return TRUE;
    }

    return FALSE;
}
