#include "workspace_dialog.h"
#include "x11_utils.h"
#include "window_info.h"
#include "log.h"
#include "utils.h"
#include <gtk/gtk.h>
#include <string.h>
#include <stdio.h>

// Must include after other headers for type definitions
#include "app_data.h"
#include "display.h"
#include "selection.h"

static WorkspaceDialog *g_dialog = NULL;

// Forward declarations
static void populate_workspaces(WorkspaceDialog *dialog);
static void populate_workspaces_for_jump(WorkspaceDialog *dialog);
static void create_workspace_grid(WorkspaceDialog *dialog);
static void create_workspace_list(WorkspaceDialog *dialog);
static GtkWidget* create_workspace_widget(DialogWorkspaceInfo *ws);
static gboolean on_dialog_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data);
static gboolean on_jump_dialog_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data);
static gboolean on_dialog_focus_out(GtkWidget *widget, GdkEventFocus *event, gpointer data);
static void move_and_close(WorkspaceDialog *dialog, int target_workspace_idx);
static void jump_and_close(WorkspaceDialog *dialog, int target_workspace_idx);
static gboolean destroy_widget_idle(gpointer widget);

// Create and show the workspace move dialog
void show_workspace_move_dialog(struct AppData *app) {
    AppData *appdata = (AppData *)app;  // Cast to remove struct
    if (!appdata || appdata->current_tab != TAB_WINDOWS || appdata->filtered_count == 0) {
        return;
    }
    
    // Set dialog active flag to prevent main window from closing on focus loss
    appdata->dialog_active = 1;
    
    // Get selected window using centralized selection management
    WindowInfo *selected_window = get_selected_window(appdata);
    if (!selected_window) {
        log_error("No window selected for workspace dialog");
        return;
    }

    int selected_idx = get_selected_index(appdata);
    log_debug("Opening workspace dialog with selected_index: %d", selected_idx);
    
    log_debug("=== WORKSPACE MOVE DIALOG DEBUG ===");
    log_debug("selected_index: %d, filtered_count: %d",
              selected_idx, appdata->filtered_count);
    log_debug("Selected window: '%s' (ID: 0x%lx, desktop: %d)",
              selected_window->title, selected_window->id, selected_window->desktop);

    // Debug: Print all windows in filtered array with selection indicator
    log_debug("Filtered array contents (array order, top to bottom):");
    for (int i = 0; i < appdata->filtered_count; i++) {
        int display_pos = appdata->filtered_count - 1 - i;
        const char *marker = (i == selected_idx) ? " >>> SELECTED" : "     ";
        log_debug("%s [%d] (display_pos=%d): '%s' (ID: 0x%lx, desktop: %d)",
                  marker, i, display_pos,
                  appdata->filtered[i].title, appdata->filtered[i].id, appdata->filtered[i].desktop);
    }
    
    log_debug("How display works: array[0] appears at display_pos=%d (bottom)", 
              appdata->filtered_count - 1);
    log_debug("                   array[%d] appears at display_pos=0 (top)",
              appdata->filtered_count - 1);
    
    // Create dialog structure
    WorkspaceDialog *dialog = g_new0(WorkspaceDialog, 1);
    dialog->target_window = selected_window;
    dialog->display = appdata->display;
    dialog->workspaces_per_row = appdata->workspaces_per_row;
    dialog->app_data = (struct AppData *)appdata;
    dialog->workspace_selected = FALSE;
    g_dialog = dialog;
    
    log_debug("Dialog created with target_window=%p pointing to '%s' (ID: 0x%lx)",
              (void*)dialog->target_window, dialog->target_window->title, dialog->target_window->id);
    
    // Create window
    dialog->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(dialog->window), "Move Window to Workspace");

    // Calculate width based on workspaces_per_row (25% wider per additional column)
    int base_width = 400;
    int dialog_width = base_width;
    if (dialog->workspaces_per_row > 1) {
        dialog_width = base_width + (base_width * 25 * (dialog->workspaces_per_row - 1)) / 100;
    }

    gtk_window_set_default_size(GTK_WINDOW(dialog->window), dialog_width, 300);
    gtk_window_set_position(GTK_WINDOW(dialog->window), GTK_WIN_POS_CENTER);
    
    // Window properties similar to main COFI window
    gtk_window_set_decorated(GTK_WINDOW(dialog->window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog->window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(dialog->window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(dialog->window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(dialog->window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_modal(GTK_WINDOW(dialog->window), TRUE);
    
    // Additional properties to ensure dialog gets focus
    gtk_window_set_urgency_hint(GTK_WINDOW(dialog->window), TRUE);
    gtk_window_set_focus_on_map(GTK_WINDOW(dialog->window), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(dialog->window), TRUE);
    
    // Set transient for the main window to ensure proper stacking
    if (appdata->window) {
        gtk_window_set_transient_for(GTK_WINDOW(dialog->window), GTK_WINDOW(appdata->window));
    }
    
    // Main container
    dialog->content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->content_box), 20);
    gtk_container_add(GTK_CONTAINER(dialog->window), dialog->content_box);
    
    // Header with window info
    GtkWidget *header_label = gtk_label_new(NULL);

    // Escape window title for markup
    gchar *escaped_title = g_markup_escape_text(selected_window->title, -1);

    // Get user's current workspace
    int user_current_desktop = get_current_desktop(appdata->display);

    char header_text[1024];
    snprintf(header_text, sizeof(header_text),
             "<b>Move Window to Workspace</b>\n\nWindow: %s\nWindow is on: Workspace %d\nYou are on: Workspace %d",
             escaped_title,
             selected_window->desktop + 1,  // Convert to 1-based
             user_current_desktop + 1);     // Convert to 1-based

    gtk_label_set_markup(GTK_LABEL(header_label), header_text);
    gtk_label_set_line_wrap(GTK_LABEL(header_label), TRUE);
    gtk_box_pack_start(GTK_BOX(dialog->content_box), header_label, FALSE, FALSE, 0);

    g_free(escaped_title);
    
    // Separator
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(dialog->content_box), separator, FALSE, FALSE, 0);
    
    // Populate workspace info
    populate_workspaces(dialog);
    
    // Create workspace display (grid or list)
    if (dialog->workspaces_per_row > 0) {
        create_workspace_grid(dialog);
    } else {
        create_workspace_list(dialog);
    }
    
    // Legend and instructions
    GtkWidget *legend = gtk_label_new("★ = Window & you here  ● = Window here  ◆ = You here");
    gtk_widget_set_halign(legend, GTK_ALIGN_CENTER);
    gtk_box_pack_end(GTK_BOX(dialog->content_box), legend, FALSE, FALSE, 0);

    GtkWidget *instructions = gtk_label_new("[Press 1-9,0 to move, Esc to cancel]");
    gtk_widget_set_halign(instructions, GTK_ALIGN_CENTER);
    gtk_box_pack_end(GTK_BOX(dialog->content_box), instructions, FALSE, FALSE, 0);
    
    // Connect signals
    g_signal_connect(dialog->window, "key-press-event", G_CALLBACK(on_dialog_key_press), dialog);
    g_signal_connect(dialog->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(dialog->window, "focus-out-event", G_CALLBACK(on_dialog_focus_out), dialog);
    
    // Show all widgets
    gtk_widget_show_all(dialog->window);
    
    // Ensure the dialog window is realized before trying to grab focus
    gtk_widget_realize(dialog->window);
    
    // Present the window to ensure it's on top
    gtk_window_present(GTK_WINDOW(dialog->window));
    
    // Force focus to the dialog window
    gtk_widget_grab_focus(dialog->window);
    gdk_window_focus(gtk_widget_get_window(dialog->window), GDK_CURRENT_TIME);
    
    // Log current selection after dialog is shown
    log_debug("After showing dialog, selected_index is now: %d", get_selected_index(appdata));
    
    // Run dialog event loop
    gtk_main();
    
    // Clear dialog active flag
    appdata->dialog_active = 0;
    
    // Note: The main window cleanup is handled in move_and_close or on_dialog_focus_out
    // We don't need to do it here to avoid double-destruction
    
    // Cleanup
    g_dialog = NULL;
    g_free(dialog);
}

// Create and show the workspace jump dialog (simplified, no window operations)
void show_workspace_jump_dialog(struct AppData *app) {
    AppData *appdata = (AppData *)app;  // Cast to remove struct
    if (!appdata) {
        return;
    }

    // Set dialog active flag to prevent main window from closing on focus loss
    appdata->dialog_active = 1;

    log_debug("Creating workspace jump dialog");

    // Create dialog structure
    WorkspaceDialog *dialog = g_new0(WorkspaceDialog, 1);
    dialog->target_window = NULL;  // No target window for jump dialog
    dialog->display = appdata->display;
    dialog->workspaces_per_row = appdata->workspaces_per_row;
    dialog->app_data = (struct AppData *)appdata;
    dialog->workspace_selected = FALSE;
    g_dialog = dialog;

    // Create window
    dialog->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(dialog->window), "Jump to Workspace");

    // Calculate width based on workspaces_per_row (25% wider per additional column)
    int base_width = 400;
    int dialog_width = base_width;
    if (dialog->workspaces_per_row > 1) {
        dialog_width = base_width + (base_width * 25 * (dialog->workspaces_per_row - 1)) / 100;
    }

    gtk_window_set_default_size(GTK_WINDOW(dialog->window), dialog_width, 250);  // Shorter height
    gtk_window_set_position(GTK_WINDOW(dialog->window), GTK_WIN_POS_CENTER);

    // Window properties similar to main COFI window
    gtk_window_set_decorated(GTK_WINDOW(dialog->window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog->window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(dialog->window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(dialog->window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(dialog->window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_modal(GTK_WINDOW(dialog->window), TRUE);

    // Main container
    dialog->content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->content_box), 20);
    gtk_container_add(GTK_CONTAINER(dialog->window), dialog->content_box);

    // Header (simplified - no window info)
    GtkWidget *header_label = gtk_label_new(NULL);

    // Get user's current workspace
    int user_current_desktop = get_current_desktop(appdata->display);

    char header_text[512];
    snprintf(header_text, sizeof(header_text),
             "<b>Jump to Workspace</b>\n\nYou are currently on: Workspace %d",
             user_current_desktop + 1);     // Convert to 1-based

    gtk_label_set_markup(GTK_LABEL(header_label), header_text);
    gtk_label_set_line_wrap(GTK_LABEL(header_label), TRUE);
    gtk_box_pack_start(GTK_BOX(dialog->content_box), header_label, FALSE, FALSE, 0);

    // Separator
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(dialog->content_box), separator, FALSE, FALSE, 0);

    // Populate workspace info (simplified for jump dialog)
    populate_workspaces_for_jump(dialog);

    // Create workspace display (grid or list)
    if (dialog->workspaces_per_row > 0) {
        create_workspace_grid(dialog);
    } else {
        create_workspace_list(dialog);
    }

    // Legend and instructions (simplified)
    GtkWidget *legend = gtk_label_new("◆ = You are here");
    gtk_widget_set_halign(legend, GTK_ALIGN_CENTER);
    gtk_box_pack_end(GTK_BOX(dialog->content_box), legend, FALSE, FALSE, 0);

    GtkWidget *instructions = gtk_label_new("[Press 1-9,0 to jump, Esc to cancel]");
    gtk_widget_set_halign(instructions, GTK_ALIGN_CENTER);
    gtk_box_pack_end(GTK_BOX(dialog->content_box), instructions, FALSE, FALSE, 0);

    // Connect signals (use different key handler for jump dialog)
    g_signal_connect(dialog->window, "key-press-event", G_CALLBACK(on_jump_dialog_key_press), dialog);
    g_signal_connect(dialog->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(dialog->window, "focus-out-event", G_CALLBACK(on_dialog_focus_out), dialog);

    // Show all widgets
    gtk_widget_show_all(dialog->window);

    // Ensure the dialog window is realized before trying to grab focus
    gtk_widget_realize(dialog->window);

    // Present the window to ensure it's on top
    gtk_window_present(GTK_WINDOW(dialog->window));

    // Force focus to the dialog window
    gtk_widget_grab_focus(dialog->window);
    gdk_window_focus(gtk_widget_get_window(dialog->window), GDK_CURRENT_TIME);

    // Run dialog event loop
    gtk_main();

    // Clear dialog active flag
    appdata->dialog_active = 0;

    // Cleanup
    g_dialog = NULL;
    g_free(dialog);
}

// Populate workspace information
static void populate_workspaces(WorkspaceDialog *dialog) {
    dialog->workspace_count = get_number_of_desktops(dialog->display);
    if (dialog->workspace_count > 36) {
        dialog->workspace_count = 36;  // Limit to supported maximum
    }

    // Get workspace names
    int name_count;
    char **names = get_desktop_names(dialog->display, &name_count);

    // Get current desktop (where the user currently is)
    int user_current_desktop = get_current_desktop(dialog->display);

    // Get target window's desktop (where the window currently is)
    dialog->current_workspace_idx = dialog->target_window->desktop;

    // Fill workspace info
    for (int i = 0; i < dialog->workspace_count; i++) {
        dialog->workspaces[i].index = i;
        dialog->workspaces[i].number = i + 1;  // 1-based for display

        if (names && i < name_count && names[i]) {
            safe_string_copy(dialog->workspaces[i].name, names[i], 64);
        } else {
            snprintf(dialog->workspaces[i].name, 64, "Workspace %d", i + 1);
        }

        // Mark if this is where the window currently is
        dialog->workspaces[i].is_current = (i == dialog->current_workspace_idx);
        // Mark if this is where the user currently is
        dialog->workspaces[i].is_user_current = (i == user_current_desktop);
    }
    
    // Free names
    if (names) {
        for (int i = 0; i < name_count; i++) {
            free(names[i]);
        }
        free(names);
    }
}

// Populate workspace information for jump dialog (simplified)
static void populate_workspaces_for_jump(WorkspaceDialog *dialog) {
    dialog->workspace_count = get_number_of_desktops(dialog->display);
    if (dialog->workspace_count > 36) {
        dialog->workspace_count = 36;  // Limit to supported maximum
    }

    // Get workspace names
    int name_count;
    char **names = get_desktop_names(dialog->display, &name_count);

    // Get current desktop (where the user currently is)
    int user_current_desktop = get_current_desktop(dialog->display);

    // Fill workspace info (simplified for jump dialog)
    for (int i = 0; i < dialog->workspace_count; i++) {
        dialog->workspaces[i].index = i;
        dialog->workspaces[i].number = i + 1;  // 1-based for display

        if (names && i < name_count && names[i]) {
            safe_string_copy(dialog->workspaces[i].name, names[i], 64);
        } else {
            snprintf(dialog->workspaces[i].name, 64, "Workspace %d", i + 1);
        }

        // For jump dialog, we only care about where the user currently is
        dialog->workspaces[i].is_current = FALSE;  // No window to track
        dialog->workspaces[i].is_user_current = (i == user_current_desktop);
    }

    // Free names
    if (names) {
        for (int i = 0; i < name_count; i++) {
            free(names[i]);
        }
        free(names);
    }
}

// Create grid layout for workspaces
static void create_workspace_grid(WorkspaceDialog *dialog) {
    dialog->grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(dialog->grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(dialog->grid), 20);
    gtk_widget_set_halign(dialog->grid, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(dialog->grid, GTK_ALIGN_CENTER);
    
    // Add grid directly without scrolling
    gtk_box_pack_start(GTK_BOX(dialog->content_box), dialog->grid, TRUE, TRUE, 0);
    
    int per_row = dialog->workspaces_per_row;
    
    for (int i = 0; i < dialog->workspace_count; i++) {
        DialogWorkspaceInfo *ws = &dialog->workspaces[i];
        int row = i / per_row;
        int col = i % per_row;
        
        GtkWidget *ws_widget = create_workspace_widget(ws);
        gtk_grid_attach(GTK_GRID(dialog->grid), ws_widget, col, row, 1, 1);
    }
}

// Create linear list layout for workspaces
static void create_workspace_list(WorkspaceDialog *dialog) {
    // Create text view for list display
    GtkWidget *textview = gtk_text_view_new();
    dialog->textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(textview), FALSE);
    
    // Set monospace font
    PangoFontDescription *font_desc = pango_font_description_from_string("monospace 12");
    gtk_widget_override_font(textview, font_desc);
    pango_font_description_free(font_desc);
    
    // Add text view directly without scrolling
    gtk_box_pack_start(GTK_BOX(dialog->content_box), textview, TRUE, TRUE, 0);
    
    // Build display text
    GString *display_text = g_string_new("");
    
    for (int i = 0; i < dialog->workspace_count; i++) {
        DialogWorkspaceInfo *ws = &dialog->workspaces[i];

        // Format with different indicators for different states
        if (ws->is_current && ws->is_user_current) {
            // Both window and user are here
            g_string_append_printf(display_text, " ★ %d  %-30s (window & you here)\n",
                                   ws->number, ws->name);
        } else if (ws->is_current) {
            // Window is here
            g_string_append_printf(display_text, " ● %d  %-30s (window here)\n",
                                   ws->number, ws->name);
        } else if (ws->is_user_current) {
            // User is here
            g_string_append_printf(display_text, " ◆ %d  %-30s (you here)\n",
                                   ws->number, ws->name);
        } else {
            // Neither
            g_string_append_printf(display_text, "   %d  %-30s\n",
                                   ws->number, ws->name);
        }
    }
    
    gtk_text_buffer_set_text(dialog->textbuffer, display_text->str, -1);
    g_string_free(display_text, TRUE);
}

// Create a single workspace widget for grid layout
static GtkWidget* create_workspace_widget(DialogWorkspaceInfo *ws) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(box, 120, 80);

    // Number label with different indicators for different states
    GtkWidget *number_label = gtk_label_new(NULL);
    char number_text[64];
    if (ws->is_current && ws->is_user_current) {
        // Both window and user are on this workspace
        snprintf(number_text, sizeof(number_text), "<b>★%d★</b>", ws->number);
    } else if (ws->is_current) {
        // Window is on this workspace
        snprintf(number_text, sizeof(number_text), "<b>●%d●</b>", ws->number);
    } else if (ws->is_user_current) {
        // User is on this workspace
        snprintf(number_text, sizeof(number_text), "<b>◆%d◆</b>", ws->number);
    } else {
        // Neither window nor user is on this workspace
        snprintf(number_text, sizeof(number_text), "<b>[%d]</b>", ws->number);
    }
    gtk_label_set_markup(GTK_LABEL(number_label), number_text);
    gtk_box_pack_start(GTK_BOX(box), number_label, FALSE, FALSE, 0);
    
    // Name label
    GtkWidget *name_label = gtk_label_new(ws->name);
    gtk_label_set_line_wrap(GTK_LABEL(name_label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(name_label), 15);
    gtk_box_pack_start(GTK_BOX(box), name_label, FALSE, FALSE, 0);
    
    // Current indicator
    if (ws->is_current) {
        GtkWidget *current_label = gtk_label_new("(current)");
        gtk_box_pack_start(GTK_BOX(box), current_label, FALSE, FALSE, 0);
    }
    
    // Style the box based on different states
    GtkCssProvider *provider = gtk_css_provider_new();
    GtkStyleContext *context = gtk_widget_get_style_context(box);

    if (ws->is_current && ws->is_user_current) {
        // Both window and user are here - bright highlight
        gtk_widget_set_name(box, "workspace-both");
        gtk_css_provider_load_from_data(provider,
            "#workspace-both { background-color: #666666; border: 2px solid #888888; padding: 8px; }", -1, NULL);
    } else if (ws->is_current) {
        // Window is here - medium highlight
        gtk_widget_set_name(box, "workspace-window");
        gtk_css_provider_load_from_data(provider,
            "#workspace-window { background-color: #444444; border: 1px solid #666666; padding: 9px; }", -1, NULL);
    } else if (ws->is_user_current) {
        // User is here - subtle highlight
        gtk_widget_set_name(box, "workspace-user");
        gtk_css_provider_load_from_data(provider,
            "#workspace-user { background-color: #333333; border: 1px dashed #555555; padding: 9px; }", -1, NULL);
    } else {
        // Neither - no special styling
        gtk_widget_set_name(box, "workspace-normal");
        gtk_css_provider_load_from_data(provider,
            "#workspace-normal { padding: 10px; }", -1, NULL);
    }

    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    
    return box;
}

// Handle key press events in dialog
static gboolean on_dialog_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget;  // Unused parameter
    WorkspaceDialog *dialog = (WorkspaceDialog *)data;
    
    // Handle Escape
    if (event->keyval == GDK_KEY_Escape) {
        gtk_widget_destroy(dialog->window);
        return TRUE;
    }
    
    // Handle number keys 1-9
    if (event->keyval >= GDK_KEY_1 && event->keyval <= GDK_KEY_9) {
        int workspace_num = event->keyval - GDK_KEY_1 + 1;  // 1-based
        if (workspace_num <= dialog->workspace_count) {
            move_and_close(dialog, workspace_num - 1);  // Convert to 0-based index
        }
        return TRUE;
    }
    
    // Handle 0 for workspace 10
    if (event->keyval == GDK_KEY_0) {
        if (dialog->workspace_count >= 10) {
            move_and_close(dialog, 9);  // Workspace 10 is index 9
        }
        return TRUE;
    }
    
    return FALSE;
}

// Handle key press events in jump dialog
static gboolean on_jump_dialog_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget;  // Unused parameter
    WorkspaceDialog *dialog = (WorkspaceDialog *)data;

    // Handle Escape
    if (event->keyval == GDK_KEY_Escape) {
        gtk_widget_destroy(dialog->window);
        return TRUE;
    }

    // Handle number keys 1-9
    if (event->keyval >= GDK_KEY_1 && event->keyval <= GDK_KEY_9) {
        int workspace_num = event->keyval - GDK_KEY_1 + 1;  // 1-based
        if (workspace_num <= dialog->workspace_count) {
            jump_and_close(dialog, workspace_num - 1);  // Convert to 0-based index
        }
        return TRUE;
    }

    // Handle 0 for workspace 10
    if (event->keyval == GDK_KEY_0) {
        if (dialog->workspace_count >= 10) {
            jump_and_close(dialog, 9);  // Workspace 10 is index 9
        }
        return TRUE;
    }

    return FALSE;
}

// Move window and close dialog
static void move_and_close(WorkspaceDialog *dialog, int target_workspace_idx) {
    if (target_workspace_idx < 0 || target_workspace_idx >= dialog->workspace_count) {
        return;
    }
    
    // Mark that a workspace was selected
    dialog->workspace_selected = TRUE;
    
    // Move the window if needed
    if (target_workspace_idx != dialog->current_workspace_idx) {
        log_debug("=== EXECUTING WINDOW MOVE ===");
        log_debug("Target window: '%s' (ID: 0x%lx)", dialog->target_window->title, dialog->target_window->id);
        log_debug("Moving from workspace %d to workspace %d", 
                  dialog->current_workspace_idx + 1, target_workspace_idx + 1);
        
        move_window_to_desktop(dialog->display, dialog->target_window->id, target_workspace_idx);
        
        log_info("Moved window '%s' (ID: 0x%lx) from workspace %d to workspace %d",
                 dialog->target_window->title,
                 dialog->target_window->id,
                 dialog->current_workspace_idx + 1,
                 target_workspace_idx + 1);
    } else {
        log_debug("Window already on target workspace %d", target_workspace_idx + 1);
    }
    
    // Store the window ID we need to activate
    Window target_window_id = dialog->target_window->id;
    AppData *appdata = (AppData *)dialog->app_data;
    
    // Close dialog first
    gtk_widget_destroy(dialog->window);
    
    // Now use the standard window activation logic
    // This will switch to the workspace and activate the window
    activate_window(target_window_id);
    
    // And close the main window - this will trigger proper cleanup
    if (appdata && appdata->window) {
        gtk_widget_destroy(appdata->window);
    }
}

// Jump to workspace and close dialog (no window operations)
static void jump_and_close(WorkspaceDialog *dialog, int target_workspace_idx) {
    if (target_workspace_idx < 0 || target_workspace_idx >= dialog->workspace_count) {
        return;
    }

    // Mark that a workspace was selected
    dialog->workspace_selected = TRUE;

    // Get current workspace
    int current_workspace = get_current_desktop(dialog->display);

    // Jump to the workspace if needed
    if (target_workspace_idx != current_workspace) {
        log_debug("=== EXECUTING WORKSPACE JUMP ===");
        log_debug("Jumping from workspace %d to workspace %d",
                  current_workspace + 1, target_workspace_idx + 1);

        switch_to_desktop(dialog->display, target_workspace_idx);

        log_info("Jumped from workspace %d to workspace %d",
                 current_workspace + 1,
                 target_workspace_idx + 1);
    } else {
        log_debug("Already on target workspace %d", target_workspace_idx + 1);
    }

    AppData *appdata = (AppData *)dialog->app_data;

    // Close dialog first
    gtk_widget_destroy(dialog->window);

    // Close the main window - this will trigger proper cleanup
    if (appdata && appdata->window) {
        gtk_widget_destroy(appdata->window);
    }
}

// Handle focus out events (close dialog when focus is lost)
static gboolean on_dialog_focus_out(GtkWidget *widget, GdkEventFocus *event, gpointer data) {
    (void)event;  // Unused parameter
    WorkspaceDialog *dialog = (WorkspaceDialog *)data;
    AppData *appdata = (AppData *)dialog->app_data;
    
    // Only close if auto-close is enabled
    if (!appdata || !appdata->close_on_focus_loss) {
        log_debug("Dialog focus lost but auto-close is disabled, not closing");
        return FALSE;
    }
    
    // Don't close if already being destroyed or if workspace was selected
    if (!widget || !gtk_widget_get_mapped(widget) || dialog->workspace_selected) {
        return FALSE;
    }
    
    log_debug("Dialog lost focus, closing dialog and main window");
    
    // Clear the dialog active flag first
    appdata->dialog_active = 0;
    
    // Quit the dialog's event loop - this will return control to show_workspace_move_dialog
    gtk_main_quit();
    
    // The actual destruction will happen after gtk_main returns
    // Schedule destruction of both windows after we return to the main loop
    g_idle_add(destroy_widget_idle, widget);
    
    if (appdata && appdata->window && GTK_IS_WIDGET(appdata->window) && 
        gtk_widget_get_mapped(appdata->window)) {
        g_idle_add(destroy_widget_idle, appdata->window);
    }
    
    return TRUE;
}

// Helper function to destroy widget in idle callback
static gboolean destroy_widget_idle(gpointer widget) {
    if (widget && GTK_IS_WIDGET(widget)) {
        gtk_widget_destroy(GTK_WIDGET(widget));
    }
    return FALSE; // Don't repeat
}