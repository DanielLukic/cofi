#include "overlay_manager.h"
#include "tiling_overlay.h"
#include "workspace_overlay.h"
#include "workspace_rename_overlay.h"
#include "harpoon_overlay.h"
#include "app_data.h"
#include "log.h"
#include <gtk/gtk.h>

// Forward declarations
static gboolean on_overlay_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data);

// External function declarations
void hide_window(AppData *app); // From main.c

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
        // For edit overlays, we'll focus the entry after a short delay
        // to ensure it's properly realized
        focus_harpoon_edit_entry_delayed(app);
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

// Center dialog content in the overlay (utility function)
void center_dialog_in_overlay(GtkWidget *dialog_content, AppData *app) {
    // This is handled by the dialog_container's alignment properties
    // Additional centering logic can be added here if needed
    (void)app; // Suppress unused parameter warning
    gtk_widget_set_halign(dialog_content, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(dialog_content, GTK_ALIGN_CENTER);
}