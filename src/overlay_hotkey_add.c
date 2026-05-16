#include "overlay_hotkey_add.h"

#include <string.h>

#include "display.h"
#include "hotkeys_provider.h"
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

    hotkeys_select_key(app, hotkey);

    validate_selection(app);
    update_scroll_position(app);
    update_display(app);
}

gboolean apply_rebind(AppData *app, const char *canonical) {
    g_strlcpy(app->hotkey_config.bindings[app->hotkey_rebind.target_index].key,
              canonical,
              sizeof(app->hotkey_config.bindings[0].key));
    save_hotkey_config(&app->hotkey_config);
    log_info("USER: Rebound hotkey '%s' -> '%s' (cmd: %s)",
             app->hotkey_rebind.target_key, canonical,
             app->hotkey_rebind.target_command);
    hide_overlay(app);
    finish_hotkey_capture_add(app, canonical);
    return TRUE;
}

gboolean show_rebind_conflict(AppData *app, GtkWidget *error_label,
                              const char *canonical, int conflict_idx) {
    app->hotkey_rebind.awaiting_confirm = TRUE;
    app->hotkey_rebind.conflict_index = conflict_idx;
    g_strlcpy(app->hotkey_rebind.pending_combo, canonical,
              sizeof(app->hotkey_rebind.pending_combo));
    if (error_label) {
        char msg[320];
        snprintf(msg, sizeof(msg),
                 "Conflict: '%s' already uses this key. Y=replace, N=cancel",
                 app->hotkey_config.bindings[conflict_idx].command);
        gtk_label_set_text(GTK_LABEL(error_label), msg);
    }
    return TRUE;
}

static gboolean dispatch_rebind_canonical(AppData *app, GtkWidget *error_label,
                                          const char *canonical, int existing) {
    if (existing == app->hotkey_rebind.target_index ||
        strcmp(canonical, app->hotkey_rebind.target_key) == 0) {
        hide_overlay(app);
        return TRUE;
    }
    if (existing >= 0) {
        return show_rebind_conflict(app, error_label, canonical, existing);
    }
    return apply_rebind(app, canonical);
}

static gboolean dispatch_add_canonical(AppData *app, GtkWidget *error_label,
                                       GtkWidget *name_entry_or_null,
                                       const char *canonical, int existing,
                                       const char *log_verb) {
    if (existing >= 0) {
        gtk_label_set_text(GTK_LABEL(error_label), "That hotkey already exists");
        return TRUE;
    }
    if (!add_hotkey_binding(&app->hotkey_config, canonical, "")) {
        gtk_label_set_text(GTK_LABEL(error_label), "Could not add hotkey binding");
        return TRUE;
    }
    if (name_entry_or_null) {
        gtk_entry_set_text(GTK_ENTRY(name_entry_or_null), canonical);
    }
    save_hotkey_config(&app->hotkey_config);
    if (!app->hotkey_capture_active) {
        regrab_hotkeys(app);
    }
    log_info("USER: %s hotkey binding '%s'", log_verb, canonical);
    hide_overlay(app);
    finish_hotkey_capture_add(app, canonical);
    return TRUE;
}

gboolean process_canonical_combo(AppData *app, GtkWidget *error_label,
                                 GtkWidget *name_entry_or_null,
                                 const char *canonical, const char *log_verb) {
    int existing = find_hotkey_binding(&app->hotkey_config, canonical);
    if (app->hotkey_rebind.active) {
        return dispatch_rebind_canonical(app, error_label, canonical, existing);
    }
    return dispatch_add_canonical(app, error_label, name_entry_or_null,
                                  canonical, existing, log_verb);
}

gboolean handle_rebind_confirm_key(AppData *app, GdkEventKey *event) {
    if (event->keyval == GDK_KEY_y) {
        const char *conflict_key =
            app->hotkey_config.bindings[app->hotkey_rebind.conflict_index].key;
        remove_hotkey_binding(&app->hotkey_config, conflict_key);
        int new_target = find_hotkey_binding(&app->hotkey_config,
                                             app->hotkey_rebind.target_key);
        if (new_target >= 0) {
            g_strlcpy(app->hotkey_config.bindings[new_target].key,
                      app->hotkey_rebind.pending_combo,
                      sizeof(app->hotkey_config.bindings[0].key));
        }
        save_hotkey_config(&app->hotkey_config);
        log_info("USER: Rebound hotkey '%s' -> '%s' replacing conflict",
                 app->hotkey_rebind.target_key, app->hotkey_rebind.pending_combo);
        hide_overlay(app);
        finish_hotkey_capture_add(app, app->hotkey_rebind.pending_combo);
        return TRUE;
    }
    if (event->keyval == GDK_KEY_n || event->keyval == GDK_KEY_Escape) {
        hide_overlay(app);
        return TRUE;
    }
    return TRUE; // swallow all other keys while awaiting confirm
}

void create_hotkey_add_overlay_content(GtkWidget *parent_container, AppData *app) {
    gboolean rebind = app->hotkey_rebind.active;

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title_label = gtk_label_new(rebind ? "Rebind Hotkey" : "Add Hotkey Binding");
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    GtkWidget *info_label = gtk_label_new(
        rebind ? "Press a new key combo to rebind, or type and press Enter"
               : "Press a key combo to capture, or type a shortcut text");
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

static gboolean is_plain_enter(const GdkEventKey *event) {
    GdkModifierType mods = event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK |
                                           GDK_SUPER_MASK | GDK_META_MASK |
                                           GDK_HYPER_MASK);
    return (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) && !mods;
}

static gboolean handle_typed_shortcut_enter(AppData *app, GtkWidget *name_entry,
                                            GtkWidget *error_label) {
    const char *input = gtk_entry_get_text(GTK_ENTRY(name_entry));
    if (!input || input[0] == '\0') {
        gtk_label_set_text(GTK_LABEL(error_label), "Enter a shortcut to add");
        return TRUE;
    }
    char canonical[128];
    char err_buf[256];
    if (!canonicalize_hotkey_shortcut(input, canonical, sizeof(canonical),
                                      err_buf, sizeof(err_buf))) {
        gtk_label_set_text(GTK_LABEL(error_label), err_buf);
        return TRUE;
    }
    return process_canonical_combo(app, error_label, NULL, canonical, "Added");
}

static gboolean handle_captured_event(AppData *app, GtkWidget *name_entry,
                                      GtkWidget *error_label, GdkEventKey *event) {
    char canonical[128];
    char err_buf[256];
    if (!canonicalize_hotkey_event(event, canonical, sizeof(canonical),
                                   err_buf, sizeof(err_buf))) {
        gtk_label_set_text(GTK_LABEL(error_label), err_buf);
        return TRUE;
    }
    return process_canonical_combo(app, error_label, name_entry, canonical, "Captured");
}

gboolean handle_hotkey_add_key_press(AppData *app, GdkEventKey *event) {
    GtkWidget *name_entry = g_object_get_data(G_OBJECT(app->dialog_container), "name_entry");
    GtkWidget *error_label = g_object_get_data(G_OBJECT(app->dialog_container), "error_label");
    if (!name_entry || !error_label) {
        log_error("Hotkey add widgets not found");
        hide_overlay(app);
        return TRUE;
    }
    if (app->hotkey_rebind.active && app->hotkey_rebind.awaiting_confirm) {
        return handle_rebind_confirm_key(app, event);
    }
    if (is_plain_enter(event)) {
        return handle_typed_shortcut_enter(app, name_entry, error_label);
    }
    if (!overlay_hotkey_add_should_capture_event(event)) {
        return FALSE;
    }
    return handle_captured_event(app, name_entry, error_label, event);
}
