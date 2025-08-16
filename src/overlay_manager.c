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
#include <gtk/gtk.h>
#include <string.h>

// Forward declarations
static gboolean on_overlay_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
static void create_name_assign_overlay_content(GtkWidget *parent_container, AppData *app);
static void create_name_edit_overlay_content(GtkWidget *parent_container, AppData *app);
static gboolean handle_name_assign_key_press(AppData *app, GdkEventKey *event);
static gboolean handle_name_edit_key_press(AppData *app, GdkEventKey *event);

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

// Initialize the overlay system
void init_overlay_system(AppData *app) {
    log_debug("Initializing overlay system");

    // Initialize overlay state
    app->overlay_active = FALSE;
    app->current_overlay = OVERLAY_NONE;

    // Create modal background overlay
    app->modal_background = gtk_event_box_new();
    gtk_widget_set_name(app->modal_background, "modal-background");
    gtk_widget_set_visible(app->modal_background, FALSE);
    gtk_widget_set_no_show_all(app->modal_background, TRUE); // Prevent show_all from showing this

    // Make modal background capture events and focus
    gtk_widget_set_can_focus(app->modal_background, TRUE);
    gtk_widget_add_events(app->modal_background, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);
    g_signal_connect(app->modal_background, "button-press-event",
                     G_CALLBACK(on_modal_background_button_press), app);
    g_signal_connect(app->modal_background, "key-press-event",
                     G_CALLBACK(on_overlay_key_press), app);

    // Create dialog container (simple box, no frame)
    app->dialog_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(app->dialog_container, "dialog-overlay");
    gtk_widget_set_visible(app->dialog_container, FALSE);
    gtk_widget_set_no_show_all(app->dialog_container, TRUE); // Prevent show_all from showing this
    gtk_widget_set_halign(app->dialog_container, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(app->dialog_container, GTK_ALIGN_CENTER);

    // Add overlays to the main overlay container
    gtk_overlay_add_overlay(GTK_OVERLAY(app->main_overlay), app->modal_background);
    gtk_overlay_add_overlay(GTK_OVERLAY(app->main_overlay), app->dialog_container);

    // Ensure modal background doesn't pass through events when visible
    gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(app->main_overlay),
                                         app->modal_background, TRUE); // Pass through when hidden

    log_debug("Overlay system initialized successfully");
}

// Show an overlay of the specified type
void show_overlay(AppData *app, OverlayType type, gpointer data) {
    (void)data; // Currently unused
    if (app->overlay_active) {
        log_debug("Overlay already active, hiding current overlay first");
        hide_overlay(app);
    }
    
    log_debug("Showing overlay type: %d", type);

    // Clear any existing content in dialog container
    gtk_container_foreach(GTK_CONTAINER(app->dialog_container),
                          (GtkCallback)gtk_widget_destroy, NULL);
    
    // Create content based on overlay type - add directly to dialog container
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
        case OVERLAY_NONE:
        default:
            log_error("Invalid overlay type: %d", type);
            return;
    }

    // Update state
    app->overlay_active = TRUE;
    app->current_overlay = type;
    // NOTE: Do NOT set dialog_active for overlays - they should allow focus loss closing
    // Only true modal dialogs (like workspace_dialog.c) should set dialog_active

    // Show overlays explicitly (since we set no_show_all)
    gtk_widget_show(app->modal_background);
    gtk_widget_show(app->dialog_container);

    // Show all children of dialog container
    gtk_container_foreach(GTK_CONTAINER(app->dialog_container),
                          (GtkCallback)gtk_widget_show_all, NULL);

    // Disable pass-through for modal background when showing
    gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(app->main_overlay),
                                         app->modal_background, FALSE);
    
    // Make main window components non-focusable during overlay
    if (app->entry) {
        gtk_widget_set_can_focus(app->entry, FALSE);
    }
    if (app->textview) {
        gtk_widget_set_can_focus(app->textview, FALSE);
    }

    // Handle focus based on overlay type
    if (type == OVERLAY_HARPOON_EDIT || type == OVERLAY_WORKSPACE_RENAME) {
        // For harpoon/workspace edit overlays, use existing focus function
        focus_harpoon_edit_entry_delayed(app);
    } else if (type == OVERLAY_NAME_ASSIGN || type == OVERLAY_NAME_EDIT) {
        // For name overlays, focus the name entry
        focus_name_entry_delayed(app);
    } else {
        // Remove focus from entry widget to prevent typing
        if (app->entry && gtk_widget_has_focus(app->entry)) {
            gtk_widget_grab_focus(app->modal_background);
            log_debug("Removed focus from entry widget during overlay");
        } else {
            // Grab focus to prevent input to main window
            gtk_widget_grab_focus(app->modal_background);
        }

        // Also grab keyboard focus more aggressively
        if (gtk_widget_get_realized(app->modal_background)) {
            gdk_window_focus(gtk_widget_get_window(app->modal_background), GDK_CURRENT_TIME);
        }
    }

    log_debug("Overlay shown successfully");
}

// Hide the current overlay
void hide_overlay(AppData *app) {
    if (!app->overlay_active) {
        return;
    }
    
    log_debug("Hiding overlay type: %d", app->current_overlay);
    
    // Hide overlays
    gtk_widget_hide(app->modal_background);
    gtk_widget_hide(app->dialog_container);

    // Re-enable pass-through for modal background when hiding
    gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(app->main_overlay),
                                         app->modal_background, TRUE);

    // Clear dialog container content
    gtk_container_foreach(GTK_CONTAINER(app->dialog_container),
                          (GtkCallback)gtk_widget_destroy, NULL);
    
    // Update state
    app->overlay_active = FALSE;
    app->current_overlay = OVERLAY_NONE;
    // NOTE: Don't clear dialog_active here since overlays don't set it
    
    // Restore focusability of main window components
    if (app->entry) {
        gtk_widget_set_can_focus(app->entry, TRUE);
    }
    if (app->textview) {
        gtk_widget_set_can_focus(app->textview, TRUE);
    }
    
    // Return focus to main entry
    if (app->entry) {
        gtk_widget_grab_focus(app->entry);
    }
    
    log_debug("Overlay hidden successfully");
}

// Check if any overlay is currently active
gboolean is_overlay_active(AppData *app) {
    return app->overlay_active;
}

// GTK callback for key press events on modal background
gboolean on_overlay_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    (void)widget; // Suppress unused parameter warning
    AppData *app = (AppData *)user_data;
    return handle_overlay_key_press(app, event);
}

// Handle key press events for overlays
gboolean handle_overlay_key_press(AppData *app, GdkEventKey *event) {
    if (!app->overlay_active) {
        return FALSE; // Not handled, let main window handle it
    }

    // ESC always closes overlay
    if (event->keyval == GDK_KEY_Escape) {
        hide_overlay(app);
        return TRUE; // Handled
    }

    // Handle overlay-specific key presses based on current overlay type
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
        case OVERLAY_NONE:
        default:
            return FALSE;
    }
}

// Handle clicks on modal background (should close overlay)
gboolean on_modal_background_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    AppData *app = (AppData *)user_data;
    
    if (event->button == 1) { // Left click
        log_debug("Modal background clicked, hiding overlay");
        hide_overlay(app);
        return TRUE; // Handled
    }
    
    return FALSE;
}

// Convenience functions for showing specific overlays
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

// Public function to show harpoon delete overlay
void show_harpoon_delete_overlay(AppData *app, int slot_index) {
    app->harpoon_delete.pending_delete = TRUE;
    app->harpoon_delete.delete_slot = slot_index;
    show_overlay(app, OVERLAY_HARPOON_DELETE, NULL);
}

// Public function to show harpoon edit overlay
void show_harpoon_edit_overlay(AppData *app, int slot_index) {
    app->harpoon_edit.editing = TRUE;
    app->harpoon_edit.editing_slot = slot_index;
    show_overlay(app, OVERLAY_HARPOON_EDIT, NULL);
}

// Create name assignment overlay content
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
    // For name editing, we reuse the name assignment logic but pre-fill the current name
    // Get the currently selected named window
    if (app->current_tab != TAB_NAMES || app->filtered_names_count == 0) {
        GtkWidget *error_label = gtk_label_new("No named window selected for editing");
        gtk_box_pack_start(GTK_BOX(parent_container), error_label, FALSE, FALSE, 10);
        return;
    }
    
    NamedWindow *selected = &app->filtered_names[app->selection.names_index];
    
    // Create vertical box for content
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);
    
    // Title label
    GtkWidget *title_label = gtk_label_new("Edit Custom Name");
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);
    
    // Window info label
    char window_info[512];
    snprintf(window_info, sizeof(window_info), "Window: %s [%s]", 
             selected->original_title, selected->class_name);
    GtkWidget *info_label = gtk_label_new(window_info);
    gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);
    
    // Entry for custom name (pre-filled with current name)
    GtkWidget *name_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(name_entry), selected->custom_name);
    gtk_editable_select_region(GTK_EDITABLE(name_entry), 0, -1); // Select all text
    gtk_widget_set_size_request(name_entry, 300, -1);
    gtk_box_pack_start(GTK_BOX(vbox), name_entry, FALSE, FALSE, 0);
    
    // Store reference to entry for focus handling
    g_object_set_data(G_OBJECT(parent_container), "name_entry", name_entry);
    
    // Store the named window index for the handler
    g_object_set_data(G_OBJECT(parent_container), "named_window_index", GINT_TO_POINTER(app->selection.names_index));
    
    // Instructions label
    GtkWidget *inst_label = gtk_label_new("Press Enter to save changes, Escape to cancel");
    gtk_widget_set_opacity(inst_label, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst_label, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
    log_info("Name edit overlay created for window: %s", selected->original_title);
}

// Handle key press for name assignment overlay
static gboolean handle_name_assign_key_press(AppData *app, GdkEventKey *event) {
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        // Get the entry widget
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
        
        // Get the selected window
        if (app->current_tab != TAB_WINDOWS || app->filtered_count == 0) {
            log_error("No window selected for name assignment");
            hide_overlay(app);
            return TRUE;
        }
        
        WindowInfo *selected = &app->filtered[app->selection.window_index];
        
        // Assign the custom name
        assign_custom_name(&app->names, selected, custom_name);
        
        // Save the configuration
        save_named_windows(&app->names);
        
        log_info("Assigned custom name '%s' to window: %s", custom_name, selected->title);
        
        // Update display and hide overlay
        hide_overlay(app);
        
        // Refresh the current display
        if (app->current_tab == TAB_WINDOWS) {
            const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
            filter_windows(app, current_filter);
            update_display(app);
        }
        
        return TRUE;
    }
    
    return FALSE; // Let overlay manager handle other keys (like ESC)
}

static gboolean handle_name_edit_key_press(AppData *app, GdkEventKey *event) {
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        // Get the entry widget and named window index
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
        
        // Find the manager index for this window
        int manager_index = find_named_window_index(&app->names, named_window->id);
        if (manager_index < 0) {
            log_error("Named window not found in manager");
            hide_overlay(app);
            return TRUE;
        }
        
        // Update the name in the manager
        update_custom_name(&app->names, manager_index, new_name);
        save_named_windows(&app->names);
        
        // Refresh the filtered list
        const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
        filter_names(app, current_filter);
        
        hide_overlay(app);
        update_display(app);
        
        log_info("USER: Updated custom name to '%s'", new_name);
        return TRUE;
    }
    
    return FALSE; // Let overlay manager handle ESC
}

// Public functions to show name overlays
void show_name_assign_overlay(AppData *app) {
    show_overlay(app, OVERLAY_NAME_ASSIGN, NULL);
}

void show_name_edit_overlay(AppData *app) {
    show_overlay(app, OVERLAY_NAME_EDIT, NULL);
}

// Center dialog content in the overlay (utility function)
void center_dialog_in_overlay(GtkWidget *dialog_content, AppData *app) {
    // This is handled by the dialog_container's alignment properties
    // Additional centering logic can be added here if needed
    (void)app; // Suppress unused parameter warning
    gtk_widget_set_halign(dialog_content, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(dialog_content, GTK_ALIGN_CENTER);
}