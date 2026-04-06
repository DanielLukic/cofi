#include "overlay_manager.h"
#include "tiling_overlay.h"
#include "workspace_overlay.h"
#include "workspace_rename_overlay.h"
#include "harpoon_overlay.h"
#include "app_data.h"
#include "log.h"
#include "named_window.h"
#include "named_window_config.h"
#include "filter.h"
#include "filter_names.h"
#include "display.h"
#include "hotkeys.h"
#include "selection.h"
#include "utils.h"
#include <gtk/gtk.h>
#include <string.h>

// Forward declarations
static gboolean on_overlay_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
static void create_name_assign_overlay_content(GtkWidget *parent_container, AppData *app);
static void create_name_edit_overlay_content(GtkWidget *parent_container, AppData *app);
static void create_config_edit_overlay_content(GtkWidget *parent_container, AppData *app);
static void create_hotkey_add_overlay_content(GtkWidget *parent_container, AppData *app);
static void create_hotkey_edit_overlay_content(GtkWidget *parent_container, AppData *app);
static gboolean handle_name_assign_key_press(AppData *app, GdkEventKey *event);
static gboolean handle_name_edit_key_press(AppData *app, GdkEventKey *event);
static gboolean handle_config_edit_key_press(AppData *app, GdkEventKey *event);
static gboolean handle_hotkey_add_key_press(AppData *app, GdkEventKey *event);
static gboolean handle_hotkey_edit_key_press(AppData *app, GdkEventKey *event);
static void finish_hotkey_capture_add(AppData *app, const char *hotkey);
static gboolean should_capture_hotkey_event(const GdkEventKey *event);

// External function declarations
void hide_window(AppData *app); // From main.c

// Helper function to focus name entry with delay
static gboolean focus_name_entry_timeout(gpointer user_data) {
    AppData *app = (AppData *)user_data;
    
    // Get the name entry widget
    GtkWidget *name_entry = g_object_get_data(G_OBJECT(app->dialog_container), "name_entry");
    if (name_entry && gtk_widget_get_visible(name_entry)) {
        gtk_widget_grab_focus(name_entry);
        log_debug("Focused name entry widget");
    } else {
        log_debug("Name entry widget not found or not visible");
    }
    
    return FALSE; // Don't repeat
}

static void focus_name_entry_delayed(AppData *app) {
    // Set focus after a short delay to ensure widgets are realized
    g_timeout_add(50, focus_name_entry_timeout, app);
}

static gboolean should_capture_hotkey_event(const GdkEventKey *event) {
    if (!event)
        return FALSE;

    if (event->keyval == GDK_KEY_Escape) {
        return FALSE;
    }

    // Return/KP_Enter without modifiers = confirm/submit; with modifiers = valid hotkey combo.
    GdkModifierType relevant = event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK |
                                               GDK_SUPER_MASK | GDK_META_MASK |
                                               GDK_HYPER_MASK);
    if ((event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) && !relevant) {
        return FALSE;
    }

    GdkModifierType mods = event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK |
                                          GDK_MOD1_MASK | GDK_SUPER_MASK |
                                          GDK_META_MASK | GDK_HYPER_MASK);
    mods &= ~(GDK_LOCK_MASK | GDK_MOD2_MASK);

    // If any non-shift modifier is pressed, capture directly.
    if (mods & ~(GDK_SHIFT_MASK)) {
        return TRUE;
    }

    // Shift+text input is interpreted as typing into the entry; Shift+special keys capture.
    gunichar uni = gdk_keyval_to_unicode(event->keyval);
    if (mods == GDK_SHIFT_MASK) {
        return !(uni != 0 && g_unichar_isprint(uni));
    }

    // With no modifiers, capture only non-printable/special keys (e.g. Tab, F1, KP_1).
    return !(uni != 0 && g_unichar_isprint(uni));
}

static void finish_hotkey_capture_add(AppData *app, const char *hotkey) {
    // Append newly added binding to live and filtered list.
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

void init_overlay_system(AppData *app) {
    log_debug("Initializing overlay system");
    app->overlay_active = FALSE;
    app->current_overlay = OVERLAY_NONE;

    app->modal_background = gtk_event_box_new();
    gtk_widget_set_name(app->modal_background, "modal-background");
    gtk_widget_set_visible(app->modal_background, FALSE);
    gtk_widget_set_no_show_all(app->modal_background, TRUE);
    gtk_widget_set_can_focus(app->modal_background, TRUE);
    gtk_widget_add_events(app->modal_background, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);
    g_signal_connect(app->modal_background, "button-press-event",
                     G_CALLBACK(on_modal_background_button_press), app);
    g_signal_connect(app->modal_background, "key-press-event",
                     G_CALLBACK(on_overlay_key_press), app);

    app->dialog_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(app->dialog_container, "dialog-overlay");
    gtk_widget_set_visible(app->dialog_container, FALSE);
    gtk_widget_set_no_show_all(app->dialog_container, TRUE);
    gtk_widget_set_halign(app->dialog_container, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(app->dialog_container, GTK_ALIGN_CENTER);

    gtk_overlay_add_overlay(GTK_OVERLAY(app->main_overlay), app->modal_background);
    gtk_overlay_add_overlay(GTK_OVERLAY(app->main_overlay), app->dialog_container);
    gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(app->main_overlay),
                                         app->modal_background, TRUE);

    log_debug("Overlay system initialized successfully");
}

extern void show_window(AppData *app);

void show_overlay(AppData *app, OverlayType type, gpointer data) {
    (void)data;

    if (!app->window_visible)
        show_window(app);

    if (app->overlay_active)
        hide_overlay(app);

    log_debug("Showing overlay type: %d", type);

    gtk_container_foreach(GTK_CONTAINER(app->dialog_container),
                          (GtkCallback)gtk_widget_destroy, NULL);

    switch (type) {
        case OVERLAY_TILING:
            create_tiling_overlay_content(app->dialog_container, app);
            break;
        case OVERLAY_WORKSPACE_MOVE:
            create_workspace_move_overlay_content(app->dialog_container, app);
            break;
        case OVERLAY_WORKSPACE_JUMP:
            create_workspace_jump_overlay_content(app->dialog_container, app);
            break;
        case OVERLAY_WORKSPACE_MOVE_ALL:
            create_workspace_move_all_overlay_content(app->dialog_container, app);
            break;
        case OVERLAY_WORKSPACE_RENAME:
            {
                int workspace_index = GPOINTER_TO_INT(data);
                create_workspace_rename_overlay_content(app->dialog_container, app, workspace_index);
            }
            break;
        case OVERLAY_HARPOON_DELETE:
            create_harpoon_delete_overlay_content(app->dialog_container, app, app->harpoon_delete.delete_slot);
            break;
        case OVERLAY_HARPOON_EDIT:
            create_harpoon_edit_overlay_content(app->dialog_container, app, app->harpoon_edit.editing_slot);
            break;
        case OVERLAY_NAME_ASSIGN:
            create_name_assign_overlay_content(app->dialog_container, app);
            break;
        case OVERLAY_NAME_EDIT:
            create_name_edit_overlay_content(app->dialog_container, app);
            break;
        case OVERLAY_CONFIG_EDIT:
            create_config_edit_overlay_content(app->dialog_container, app);
            break;
        case OVERLAY_HOTKEY_ADD:
            create_hotkey_add_overlay_content(app->dialog_container, app);
            break;
        case OVERLAY_HOTKEY_EDIT:
            create_hotkey_edit_overlay_content(app->dialog_container, app);
            break;
        case OVERLAY_NONE:
        default:
            log_error("Invalid overlay type: %d", type);
            return;
    }

    app->overlay_active = TRUE;
    app->current_overlay = type;

    gtk_widget_show(app->modal_background);
    gtk_widget_show(app->dialog_container);
    gtk_container_foreach(GTK_CONTAINER(app->dialog_container),
                          (GtkCallback)gtk_widget_show_all, NULL);
    gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(app->main_overlay),
                                         app->modal_background, FALSE);

    if (app->entry)
        gtk_widget_set_can_focus(app->entry, FALSE);
    if (app->textview)
        gtk_widget_set_can_focus(app->textview, FALSE);

    if (type == OVERLAY_HARPOON_EDIT || type == OVERLAY_WORKSPACE_RENAME) {
        focus_harpoon_edit_entry_delayed(app);
    } else if (type == OVERLAY_NAME_ASSIGN || type == OVERLAY_NAME_EDIT ||
               type == OVERLAY_CONFIG_EDIT || type == OVERLAY_HOTKEY_ADD ||
               type == OVERLAY_HOTKEY_EDIT) {
        focus_name_entry_delayed(app);
    } else {
        gtk_widget_grab_focus(app->modal_background);
        if (gtk_widget_get_realized(app->modal_background))
            gdk_window_focus(gtk_widget_get_window(app->modal_background), GDK_CURRENT_TIME);
    }

    log_debug("Overlay shown successfully");
}

void hide_overlay(AppData *app) {
    if (!app->overlay_active) return;

    log_debug("Hiding overlay type: %d", app->current_overlay);

    gtk_widget_hide(app->modal_background);
    gtk_widget_hide(app->dialog_container);
    gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(app->main_overlay),
                                         app->modal_background, TRUE);
    gtk_container_foreach(GTK_CONTAINER(app->dialog_container),
                          (GtkCallback)gtk_widget_destroy, NULL);

    app->overlay_active = FALSE;
    app->current_overlay = OVERLAY_NONE;

    if (app->hotkey_capture_active) {
        app->hotkey_capture_active = FALSE;
        if (!app->no_daemon) {
            regrab_hotkeys(app);
        }
        log_debug("Restored hotkey grabs after hotkey add capture");
    }

    if (app->entry)
        gtk_widget_set_can_focus(app->entry, TRUE);
    if (app->textview)
        gtk_widget_set_can_focus(app->textview, TRUE);
    if (app->entry)
        gtk_widget_grab_focus(app->entry);

    log_debug("Overlay hidden successfully");
}

gboolean is_overlay_active(AppData *app) {
    return app->overlay_active;
}

gboolean on_overlay_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    (void)widget;
    AppData *app = (AppData *)user_data;
    return handle_overlay_key_press(app, event);
}

gboolean handle_overlay_key_press(AppData *app, GdkEventKey *event) {
    if (!app->overlay_active) return FALSE;

    if (event->keyval == GDK_KEY_Escape) {
        hide_overlay(app);
        return TRUE;
    }

    switch (app->current_overlay) {
        case OVERLAY_TILING:
            return handle_tiling_overlay_key_press(app, event);
        case OVERLAY_WORKSPACE_MOVE:
            return handle_workspace_move_key_press(app, event);
        case OVERLAY_WORKSPACE_JUMP:
            return handle_workspace_jump_key_press(app, event);
        case OVERLAY_WORKSPACE_MOVE_ALL:
            return handle_workspace_move_all_key_press(app, event);
        case OVERLAY_WORKSPACE_RENAME:
            if (handle_workspace_rename_key_press(app, event->keyval)) {
                hide_overlay(app);
                return TRUE;
            }
            return FALSE;
        case OVERLAY_HARPOON_DELETE:
            return handle_harpoon_delete_key_press(app, event);
        case OVERLAY_HARPOON_EDIT:
            return handle_harpoon_edit_key_press(app, event);
        case OVERLAY_NAME_ASSIGN:
            return handle_name_assign_key_press(app, event);
        case OVERLAY_NAME_EDIT:
            return handle_name_edit_key_press(app, event);
        case OVERLAY_CONFIG_EDIT:
            return handle_config_edit_key_press(app, event);
        case OVERLAY_HOTKEY_ADD:
            return handle_hotkey_add_key_press(app, event);
        case OVERLAY_HOTKEY_EDIT:
            return handle_hotkey_edit_key_press(app, event);
        case OVERLAY_NONE:
        default:
            return FALSE;
    }
}

gboolean on_modal_background_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    (void)widget;
    AppData *app = (AppData *)user_data;

    if (event->button == 1) {
        log_debug("Modal background clicked, hiding overlay");
        hide_overlay(app);
        return TRUE;
    }
    return FALSE;
}

void show_tiling_overlay(AppData *app) {
    show_overlay(app, OVERLAY_TILING, NULL);
}

void show_workspace_move_overlay(AppData *app) {
    show_overlay(app, OVERLAY_WORKSPACE_MOVE, NULL);
}

void show_workspace_jump_overlay(AppData *app) {
    show_overlay(app, OVERLAY_WORKSPACE_JUMP, NULL);
}

void show_workspace_move_all_overlay(AppData *app) {
    show_overlay(app, OVERLAY_WORKSPACE_MOVE_ALL, NULL);
}

void show_workspace_rename_overlay(AppData *app, int workspace_index) {
    show_overlay(app, OVERLAY_WORKSPACE_RENAME, GINT_TO_POINTER(workspace_index));
}

void show_harpoon_delete_overlay(AppData *app, int slot_index) {
    app->harpoon_delete.pending_delete = TRUE;
    app->harpoon_delete.delete_slot = slot_index;
    show_overlay(app, OVERLAY_HARPOON_DELETE, NULL);
}

void show_harpoon_edit_overlay(AppData *app, int slot_index) {
    app->harpoon_edit.editing = TRUE;
    app->harpoon_edit.editing_slot = slot_index;
    show_overlay(app, OVERLAY_HARPOON_EDIT, NULL);
}

static void create_name_assign_overlay_content(GtkWidget *parent_container, AppData *app) {
    // Get the currently selected window
    if (app->current_tab != TAB_WINDOWS || app->filtered_count == 0) {
        GtkWidget *error_label = gtk_label_new("No window selected for name assignment");
        gtk_box_pack_start(GTK_BOX(parent_container), error_label, FALSE, FALSE, 10);
        return;
    }
    
    WindowInfo *selected = &app->filtered[app->selection.window_index];
    
    // Create main container
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);
    
    // Title label
    GtkWidget *title_label = gtk_label_new("Assign Custom Name");
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    pango_attr_list_insert(attrs, pango_attr_scale_new(1.2));
    gtk_label_set_attributes(GTK_LABEL(title_label), attrs);
    pango_attr_list_unref(attrs);
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);
    
    // Window info label
    char window_info[512];
    snprintf(window_info, sizeof(window_info), "Window: %s [%s]", 
             selected->title, selected->class_name);
    GtkWidget *info_label = gtk_label_new(window_info);
    gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);
    
    // Entry for custom name
    GtkWidget *name_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(name_entry), "Enter custom name...");
    gtk_widget_set_size_request(name_entry, 300, -1);
    gtk_box_pack_start(GTK_BOX(vbox), name_entry, FALSE, FALSE, 0);
    
    // Store reference to entry for focus handling
    g_object_set_data(G_OBJECT(parent_container), "name_entry", name_entry);
    
    // Instructions label
    GtkWidget *inst_label = gtk_label_new("Press Enter to assign name, Escape to cancel");
    gtk_widget_set_opacity(inst_label, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst_label, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
    log_info("Name assignment overlay created for window: %s", selected->title);
}

static void create_name_edit_overlay_content(GtkWidget *parent_container, AppData *app) {
    if (app->current_tab != TAB_NAMES || app->filtered_names_count == 0) {
        GtkWidget *error_label = gtk_label_new("No named window selected for editing");
        gtk_box_pack_start(GTK_BOX(parent_container), error_label, FALSE, FALSE, 10);
        return;
    }
    
    NamedWindow *selected = &app->filtered_names[app->selection.names_index];
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title_label = gtk_label_new("Edit Custom Name");
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);
    
    char window_info[512];
    snprintf(window_info, sizeof(window_info), "Window: %s [%s]",
             selected->original_title, selected->class_name);
    GtkWidget *info_label = gtk_label_new(window_info);
    gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

    GtkWidget *name_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(name_entry), selected->custom_name);
    gtk_editable_select_region(GTK_EDITABLE(name_entry), 0, -1);
    gtk_widget_set_size_request(name_entry, 300, -1);
    gtk_box_pack_start(GTK_BOX(vbox), name_entry, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(parent_container), "name_entry", name_entry);
    g_object_set_data(G_OBJECT(parent_container), "named_window_index", GINT_TO_POINTER(app->selection.names_index));

    GtkWidget *inst_label = gtk_label_new("Press Enter to save changes, Escape to cancel");
    gtk_widget_set_opacity(inst_label, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst_label, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
    log_info("Name edit overlay created for window: %s", selected->original_title);
}

static gboolean handle_name_assign_key_press(AppData *app, GdkEventKey *event) {
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        GtkWidget *name_entry = g_object_get_data(G_OBJECT(app->dialog_container), "name_entry");
        if (!name_entry) {
            log_error("Name entry widget not found");
            hide_overlay(app);
            return TRUE;
        }

        const char *custom_name = gtk_entry_get_text(GTK_ENTRY(name_entry));
        if (!custom_name || strlen(custom_name) == 0) {
            log_info("Empty name entered, canceling assignment");
            hide_overlay(app);
            return TRUE;
        }

        if (app->current_tab != TAB_WINDOWS || app->filtered_count == 0) {
            log_error("No window selected for name assignment");
            hide_overlay(app);
            return TRUE;
        }
        
        WindowInfo *selected = &app->filtered[app->selection.window_index];

        assign_custom_name(&app->names, selected, custom_name);
        save_named_windows(&app->names);
        log_info("Assigned custom name '%s' to window: %s", custom_name, selected->title);

        hide_overlay(app);
        if (app->current_tab == TAB_WINDOWS) {
            const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
            filter_windows(app, current_filter);
            update_display(app);
        }
        
        return TRUE;
    }
    
    return FALSE;
}

static gboolean handle_name_edit_key_press(AppData *app, GdkEventKey *event) {
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        GtkWidget *name_entry = g_object_get_data(G_OBJECT(app->dialog_container), "name_entry");
        int named_index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(app->dialog_container), "named_window_index"));
        
        if (!name_entry) {
            log_error("Name entry widget not found");
            hide_overlay(app);
            return TRUE;
        }
        
        const char *new_name = gtk_entry_get_text(GTK_ENTRY(name_entry));
        if (!new_name || strlen(new_name) == 0) {
            log_info("Empty name entered, canceling edit");
            hide_overlay(app);
            return TRUE;
        }
        
        // Get the actual named window from the filtered list
        if (named_index < 0 || named_index >= app->filtered_names_count) {
            log_error("Invalid named window index: %d", named_index);
            hide_overlay(app);
            return TRUE;
        }
        
        NamedWindow *named_window = &app->filtered_names[named_index];
        int manager_index = find_named_window_index(&app->names, named_window->id);
        if (manager_index < 0) {
            manager_index = find_named_window_by_name(&app->names, named_window->custom_name);
        }
        if (manager_index < 0) {
            log_error("Named window not found in manager");
            hide_overlay(app);
            return TRUE;
        }
        
        update_custom_name(&app->names, manager_index, new_name);
        save_named_windows(&app->names);

        const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
        filter_names(app, current_filter);
        
        hide_overlay(app);
        update_display(app);
        
        log_info("USER: Updated custom name to '%s'", new_name);
        return TRUE;
    }
    
    return FALSE;
}

void show_name_assign_overlay(AppData *app) {
    show_overlay(app, OVERLAY_NAME_ASSIGN, NULL);
}

void show_name_edit_overlay(AppData *app) {
    show_overlay(app, OVERLAY_NAME_EDIT, NULL);
}

// Forward declarations for filter functions defined in main.c
// (these are static in main.c, so we re-declare the ones we need via extern)
// Instead we'll just call update_display and let the caller handle re-filtering.

static void create_config_edit_overlay_content(GtkWidget *parent_container, AppData *app) {
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

static gboolean handle_config_edit_key_press(AppData *app, GdkEventKey *event) {
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
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

        // Re-filter and redraw the list immediately so edited value is visible.
        // Re-assigning entry text can be a no-op if unchanged and won't trigger refresh.
        const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
        filter_config(app, current_filter);
        validate_selection(app);
        update_scroll_position(app);
        update_display(app);

        return TRUE;
    }

    return FALSE;
}

static void create_hotkey_edit_overlay_content(GtkWidget *parent_container, AppData *app) {
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

static void create_hotkey_add_overlay_content(GtkWidget *parent_container, AppData *app) {
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

static gboolean handle_hotkey_add_key_press(AppData *app, GdkEventKey *event) {
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
        if (!app->hotkey_capture_active)
            regrab_hotkeys(app);
        log_info("USER: Added hotkey binding '%s'", canonical);

        hide_overlay(app);
        finish_hotkey_capture_add(app, canonical);
        return TRUE;
    }

    if (should_capture_hotkey_event(event)) {
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
        if (!app->hotkey_capture_active)
            regrab_hotkeys(app);
        log_info("USER: Captured hotkey binding '%s'", canonical);

        hide_overlay(app);
        finish_hotkey_capture_add(app, canonical);
        return TRUE;
    }

    return FALSE;
}

static gboolean handle_hotkey_edit_key_press(AppData *app, GdkEventKey *event) {
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
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

        // Find the binding and update its command
        int idx = find_hotkey_binding(&app->hotkey_config, hotkey_key);
        if (idx >= 0) {
            strncpy(app->hotkey_config.bindings[idx].command, new_command,
                    sizeof(app->hotkey_config.bindings[idx].command) - 1);
            app->hotkey_config.bindings[idx].command[sizeof(app->hotkey_config.bindings[idx].command) - 1] = '\0';
            save_hotkey_config(&app->hotkey_config);
            regrab_hotkeys(app);
            log_info("USER: Updated hotkey '%s' command to '%s'", hotkey_key, new_command);
        } else {
            log_error("Hotkey '%s' not found for editing", hotkey_key);
        }

        hide_overlay(app);

        // Re-filter and redraw the list immediately so edited command is visible.
        // Re-assigning entry text can be a no-op if unchanged and won't trigger refresh.
        const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
        filter_hotkeys(app, current_filter);
        validate_selection(app);
        update_scroll_position(app);
        update_display(app);

        return TRUE;
    }

    return FALSE;
}

void center_dialog_in_overlay(GtkWidget *dialog_content, AppData *app) {
    (void)app;
    gtk_widget_set_halign(dialog_content, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(dialog_content, GTK_ALIGN_CENTER);
}
