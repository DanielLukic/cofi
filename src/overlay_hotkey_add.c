#include "overlay_hotkey_add.h"

#include <string.h>

#include "display.h"
#include "filter.h"
#include "hotkeys.h"
#include "log.h"
#include "overlay_manager.h"
#include "selection.h"
#include "utils.h"

static void finish_hotkey_capture_add(AppData *app, const char *hotkey) {
    if (!app || !hotkey || hotkey[0] == '\0') {
        return;
    }

    filter_hotkeys(app, gtk_entry_get_text(GTK_ENTRY(app->entry)));

    gboolean found = FALSE;
    for (int i = 0; i < app->filtered_hotkeys_count; i++) {
        if (strcmp(app->filtered_hotkeys[i].key, hotkey) == 0) {
            app->selection.hotkeys_index = i;
            found = TRUE;
            break;
        }
    }

    if (!found) {
        app->selection.hotkeys_index = 0;
    }

    validate_selection(app);
    update_scroll_position(app);
    update_display(app);
}

void create_hotkey_add_overlay_content(GtkWidget *parent_container, AppData *app) {
    (void)app;

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title_label = gtk_label_new("Add Hotkey Binding");
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    GtkWidget *info_label = gtk_label_new("Press a key combo to capture, or type a shortcut text");
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

    GtkWidget *name_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(name_entry), "e.g. Mod1+Tab");
    gtk_widget_set_size_request(name_entry, 400, -1);
    gtk_box_pack_start(GTK_BOX(vbox), name_entry, FALSE, FALSE, 0);

    GtkWidget *error_label = gtk_label_new("");
    gtk_widget_set_halign(error_label, GTK_ALIGN_START);
    gtk_widget_set_opacity(error_label, 0.8);
    gtk_box_pack_start(GTK_BOX(vbox), error_label, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(parent_container), "name_entry", name_entry);
    g_object_set_data(G_OBJECT(parent_container), "error_label", error_label);

    GtkWidget *inst_label = gtk_label_new("Press Enter to add typed shortcut, Escape to cancel");
    gtk_widget_set_opacity(inst_label, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
    log_info("Hotkey add overlay created");
}

gboolean handle_hotkey_add_key_press(AppData *app, GdkEventKey *event) {
    GtkWidget *name_entry = g_object_get_data(G_OBJECT(app->dialog_container), "name_entry");
    GtkWidget *error_label = g_object_get_data(G_OBJECT(app->dialog_container), "error_label");

    if (!name_entry || !error_label) {
        log_error("Hotkey add widgets not found");
        hide_overlay(app);
        return TRUE;
    }

    GdkModifierType add_mods = event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK |
                                               GDK_SUPER_MASK | GDK_META_MASK |
                                               GDK_HYPER_MASK);
    if ((event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) && !add_mods) {
        const char *shortcut_input = gtk_entry_get_text(GTK_ENTRY(name_entry));
        char canonical[128];
        char err_buf[256];

        if (!shortcut_input || shortcut_input[0] == '\0') {
            gtk_label_set_text(GTK_LABEL(error_label), "Enter a shortcut to add");
            return TRUE;
        }

        if (!canonicalize_hotkey_shortcut(shortcut_input, canonical, sizeof(canonical),
                                          err_buf, sizeof(err_buf))) {
            gtk_label_set_text(GTK_LABEL(error_label), err_buf);
            return TRUE;
        }

        if (find_hotkey_binding(&app->hotkey_config, canonical) >= 0) {
            gtk_label_set_text(GTK_LABEL(error_label), "That hotkey already exists");
            return TRUE;
        }

        if (!add_hotkey_binding(&app->hotkey_config, canonical, "")) {
            gtk_label_set_text(GTK_LABEL(error_label), "Could not add hotkey binding");
            return TRUE;
        }

        save_hotkey_config(&app->hotkey_config);
        if (!app->hotkey_capture_active) {
            regrab_hotkeys(app);
        }

        log_info("USER: Added hotkey binding '%s'", canonical);
        hide_overlay(app);
        finish_hotkey_capture_add(app, canonical);
        return TRUE;
    }

    if (!overlay_hotkey_add_should_capture_event(event)) {
        return FALSE;
    }

    char canonical[128];
    char err_buf[256];

    if (!canonicalize_hotkey_event(event, canonical, sizeof(canonical),
                                   err_buf, sizeof(err_buf))) {
        gtk_label_set_text(GTK_LABEL(error_label), err_buf);
        return TRUE;
    }

    if (find_hotkey_binding(&app->hotkey_config, canonical) >= 0) {
        gtk_label_set_text(GTK_LABEL(error_label), "That hotkey already exists");
        return TRUE;
    }

    if (!add_hotkey_binding(&app->hotkey_config, canonical, "")) {
        gtk_label_set_text(GTK_LABEL(error_label), "Could not add hotkey binding");
        return TRUE;
    }

    gtk_entry_set_text(GTK_ENTRY(name_entry), canonical);
    save_hotkey_config(&app->hotkey_config);
    if (!app->hotkey_capture_active) {
        regrab_hotkeys(app);
    }

    log_info("USER: Captured hotkey binding '%s'", canonical);
    hide_overlay(app);
    finish_hotkey_capture_add(app, canonical);
    return TRUE;
}
