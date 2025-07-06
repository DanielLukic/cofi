#include "overlay_manager.h"
#include "app_data.h"
#include "log.h"
#include "selection.h"
#include "tiling_dialog.h"
#include "x11_utils.h"
#include <gtk/gtk.h>

// Forward declarations
static void create_tiling_grid_overlay(GtkWidget *parent_box, AppData *app);
static gboolean handle_tiling_overlay_key_press(AppData *app, GdkEventKey *event);
static gboolean on_overlay_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
static void create_tiling_overlay_content(GtkWidget *parent_container, AppData *app);
static void create_workspace_jump_overlay_content(GtkWidget *parent_container, AppData *app);
static void create_workspace_move_overlay_content(GtkWidget *parent_container, AppData *app);
static GtkWidget* create_workspace_widget_overlay(int workspace_num, const char *workspace_name,
                                                  gboolean is_current, gboolean is_user_current);
static gboolean handle_workspace_jump_key_press(AppData *app, GdkEventKey *event);
static gboolean handle_workspace_move_key_press(AppData *app, GdkEventKey *event);

// External function declarations
void destroy_window(AppData *app); // From main.c

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



// Create tiling overlay content (extracted from tiling_dialog.c)
static void create_tiling_overlay_content(GtkWidget *parent_container, AppData *app) {
    // Get selected window using centralized selection management
    WindowInfo *selected_window = get_selected_window(app);
    if (!selected_window) {
        log_error("No window selected for tiling overlay");
        GtkWidget *error_label = gtk_label_new("No window selected for tiling");
        gtk_container_add(GTK_CONTAINER(parent_container), error_label);
        return;
    }

    // Header with window title
    char *escaped_title = g_markup_escape_text(selected_window->title, -1);
    char header_text[512];
    snprintf(header_text, sizeof(header_text),
             "<b>Tile Window:</b> %s", escaped_title);

    GtkWidget *header_label = gtk_label_new(NULL);
    gtk_widget_set_halign(header_label, GTK_ALIGN_CENTER);
    gtk_label_set_markup(GTK_LABEL(header_label), header_text);
    gtk_label_set_line_wrap(GTK_LABEL(header_label), TRUE);
    gtk_box_pack_start(GTK_BOX(parent_container), header_label, FALSE, FALSE, 0);

    g_free(escaped_title);

    // Separator
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(parent_container), separator, FALSE, FALSE, 0);

    // Create tiling options display
    create_tiling_grid_overlay(parent_container, app);
}

// Helper function to create tiling grid display (extracted from tiling_dialog.c)
static void create_tiling_grid_overlay(GtkWidget *parent_box, AppData *app) {
    // Create main horizontal container
    GtkWidget *main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 40);
    gtk_box_pack_start(GTK_BOX(parent_box), main_hbox, TRUE, TRUE, 20);

    // === LEFT HALF: Diamond shape for directions ===
    GtkWidget *left_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_halign(left_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(main_hbox), left_box, TRUE, TRUE, 10);

    // Half-screen title
    GtkWidget *halves_label = gtk_label_new("<b>Half Screen</b>");
    gtk_label_set_use_markup(GTK_LABEL(halves_label), TRUE);
    gtk_widget_set_halign(halves_label, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(left_box), halves_label, FALSE, FALSE, 5);

    // Diamond layout container
    GtkWidget *diamond_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_halign(diamond_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(left_box), diamond_box, TRUE, TRUE, 10);

    // Top (T)
    GtkWidget *top_label = gtk_label_new("T");
    gtk_widget_set_halign(top_label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(top_label, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(top_label, 40, 30);
    GtkStyleContext *top_context = gtk_widget_get_style_context(top_label);
    gtk_style_context_add_class(top_context, "grid-cell");
    gtk_box_pack_start(GTK_BOX(diamond_box), top_label, FALSE, FALSE, 0);

    // Middle row (L and R)
    GtkWidget *middle_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 50);
    gtk_widget_set_halign(middle_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(diamond_box), middle_box, FALSE, FALSE, 0);
    
    GtkWidget *left_label = gtk_label_new("L");
    gtk_widget_set_halign(left_label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(left_label, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(left_label, 40, 30);
    GtkStyleContext *left_context = gtk_widget_get_style_context(left_label);
    gtk_style_context_add_class(left_context, "grid-cell");
    
    GtkWidget *right_label = gtk_label_new("R");
    gtk_widget_set_halign(right_label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(right_label, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(right_label, 40, 30);
    GtkStyleContext *right_context = gtk_widget_get_style_context(right_label);
    gtk_style_context_add_class(right_context, "grid-cell");
    
    gtk_box_pack_start(GTK_BOX(middle_box), left_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(middle_box), right_label, FALSE, FALSE, 0);

    // Bottom (B)
    GtkWidget *bottom_label = gtk_label_new("B");
    gtk_widget_set_halign(bottom_label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(bottom_label, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(bottom_label, 40, 30);
    GtkStyleContext *bottom_context = gtk_widget_get_style_context(bottom_label);
    gtk_style_context_add_class(bottom_context, "grid-cell");
    gtk_box_pack_start(GTK_BOX(diamond_box), bottom_label, FALSE, FALSE, 0);

    // === VERTICAL DIVIDER ===
    GtkWidget *vseparator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(main_hbox), vseparator, FALSE, FALSE, 0);

    // === RIGHT HALF: Grid and other options ===
    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_halign(right_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(main_hbox), right_box, TRUE, TRUE, 10);

    // Dynamic Grid
    int tile_columns = app->config.tile_columns;
    char grid_label_text[64];
    snprintf(grid_label_text, sizeof(grid_label_text), "<b>%dx2 Grid</b>", tile_columns);
    GtkWidget *grid_label = gtk_label_new(grid_label_text);
    gtk_label_set_use_markup(GTK_LABEL(grid_label), TRUE);
    gtk_widget_set_halign(grid_label, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(right_box), grid_label, FALSE, FALSE, 5);

    // Grid visualization
    GtkWidget *grid_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_halign(grid_container, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(right_box), grid_container, FALSE, FALSE, 10);

    // Top row
    GtkWidget *top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(grid_container), top_row, FALSE, FALSE, 0);

    for (int i = 1; i <= tile_columns; i++) {
        char label_text[16];
        snprintf(label_text, sizeof(label_text), "%d", i);
        GtkWidget *cell = gtk_label_new(label_text);
        gtk_widget_set_size_request(cell, 40, 30);
        gtk_widget_set_halign(cell, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(cell, GTK_ALIGN_CENTER);

        // Add border styling
        GtkStyleContext *context = gtk_widget_get_style_context(cell);
        gtk_style_context_add_class(context, "grid-cell");

        gtk_box_pack_start(GTK_BOX(top_row), cell, FALSE, FALSE, 0);
    }

    // Bottom row
    GtkWidget *bottom_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(grid_container), bottom_row, FALSE, FALSE, 0);

    for (int i = tile_columns + 1; i <= tile_columns * 2; i++) {
        char label_text[16];
        snprintf(label_text, sizeof(label_text), "%d", i);
        GtkWidget *cell = gtk_label_new(label_text);
        gtk_widget_set_size_request(cell, 40, 30);
        gtk_widget_set_halign(cell, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(cell, GTK_ALIGN_CENTER);

        // Add border styling
        GtkStyleContext *context = gtk_widget_get_style_context(cell);
        gtk_style_context_add_class(context, "grid-cell");

        gtk_box_pack_start(GTK_BOX(bottom_row), cell, FALSE, FALSE, 0);
    }

    // === BOTTOM: Other options ===
    GtkWidget *bottom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(bottom_box, GTK_ALIGN_CENTER);
    gtk_box_pack_end(GTK_BOX(parent_box), bottom_box, FALSE, FALSE, 20);

    GtkWidget *other_label = gtk_label_new("<b>Other:</b>");
    gtk_label_set_use_markup(GTK_LABEL(other_label), TRUE);
    gtk_box_pack_start(GTK_BOX(bottom_box), other_label, FALSE, FALSE, 0);

    GtkWidget *other_options = gtk_label_new("  F - Fullscreen   C - Center");
    gtk_box_pack_start(GTK_BOX(bottom_box), other_options, FALSE, FALSE, 0);
}

// Handle key press events for tiling overlay (extracted from tiling_dialog.c)
static gboolean handle_tiling_overlay_key_press(AppData *app, GdkEventKey *event) {
    // Get selected window
    WindowInfo *selected_window = get_selected_window(app);
    if (!selected_window) {
        log_error("No window selected for tiling");
        hide_overlay(app);
        return TRUE;
    }

    TileOption option;
    gboolean valid_option = TRUE;

    // Get tile_columns from config for validation
    int tile_columns = app->config.tile_columns;
    int max_positions = tile_columns * 2;  // columns * 2 rows

    // Handle tiling options
    switch (event->keyval) {
        case GDK_KEY_l:
        case GDK_KEY_L:
            option = TILE_LEFT_HALF;
            break;
        case GDK_KEY_r:
        case GDK_KEY_R:
            option = TILE_RIGHT_HALF;
            break;
        case GDK_KEY_t:
        case GDK_KEY_T:
            option = TILE_TOP_HALF;
            break;
        case GDK_KEY_b:
        case GDK_KEY_B:
            option = TILE_BOTTOM_HALF;
            break;
        case GDK_KEY_1:
            option = TILE_GRID_1;
            valid_option = (1 <= max_positions);
            break;
        case GDK_KEY_2:
            option = TILE_GRID_2;
            valid_option = (2 <= max_positions);
            break;
        case GDK_KEY_3:
            option = TILE_GRID_3;
            valid_option = (3 <= max_positions);
            break;
        case GDK_KEY_4:
            option = TILE_GRID_4;
            valid_option = (4 <= max_positions);
            break;
        case GDK_KEY_5:
            option = TILE_GRID_5;
            valid_option = (5 <= max_positions);
            break;
        case GDK_KEY_6:
            option = TILE_GRID_6;
            valid_option = (6 <= max_positions);
            break;
        case GDK_KEY_7:
            option = TILE_GRID_7;
            valid_option = (7 <= max_positions);
            break;
        case GDK_KEY_8:
            option = TILE_GRID_8;
            valid_option = (8 <= max_positions);
            break;
        case GDK_KEY_9:
            option = TILE_GRID_9;
            valid_option = (9 <= max_positions);
            break;
        case GDK_KEY_f:
        case GDK_KEY_F:
            option = TILE_FULLSCREEN;
            break;
        case GDK_KEY_c:
        case GDK_KEY_C:
            option = TILE_CENTER;
            break;
        default:
            valid_option = FALSE;
            break;
    }

    if (valid_option) {
        log_info("USER: Tiling window '%s' with option %d", selected_window->title, option);

        // Apply tiling using the existing function from tiling_dialog.c
        apply_tiling(app->display, selected_window->id, option, tile_columns);

        // Hide overlay and close application
        hide_overlay(app);

        // Close the main application (similar to original tiling dialog behavior)
        destroy_window(app);

        return TRUE;
    }

    return FALSE; // Invalid key, don't handle
}



// Create workspace jump overlay content
static void create_workspace_jump_overlay_content(GtkWidget *parent_container, AppData *app) {
    // Header
    GtkWidget *header_label = gtk_label_new(NULL);
    gtk_widget_set_halign(header_label, GTK_ALIGN_CENTER);
    gtk_label_set_markup(GTK_LABEL(header_label), "<b>Jump to Workspace</b>");
    gtk_box_pack_start(GTK_BOX(parent_container), header_label, FALSE, FALSE, 0);

    // Separator
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(parent_container), separator, FALSE, FALSE, 0);

    // Get workspace information
    int workspace_count = get_number_of_desktops(app->display);
    if (workspace_count > 36) {
        workspace_count = 36;  // Limit to supported maximum
    }

    // Get workspace names
    int name_count;
    char **names = get_desktop_names(app->display, &name_count);

    // Get current desktop (where the user currently is)
    int user_current_desktop = get_current_desktop(app->display);

    // Create workspace display based on configuration
    if (app->config.workspaces_per_row > 0) {
        // Grid layout
        GtkWidget *grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
        gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(parent_container), grid, TRUE, TRUE, 0);

        int per_row = app->config.workspaces_per_row;

        for (int i = 0; i < workspace_count; i++) {
            int row = i / per_row;
            int col = i % per_row;

            // Get workspace name
            char workspace_name[64];
            if (names && i < name_count && names[i]) {
                snprintf(workspace_name, sizeof(workspace_name), "%s", names[i]);
            } else {
                snprintf(workspace_name, sizeof(workspace_name), "Workspace %d", i + 1);
            }

            GtkWidget *ws_widget = create_workspace_widget_overlay(
                i + 1,  // 1-based number for display
                workspace_name,
                FALSE,  // No window to track for jump dialog
                (i == user_current_desktop)  // Is user current
            );
            gtk_grid_attach(GTK_GRID(grid), ws_widget, col, row, 1, 1);
        }
    } else {
        // Linear list layout
        GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request(scrolled, 400, 200);
        gtk_box_pack_start(GTK_BOX(parent_container), scrolled, TRUE, TRUE, 0);

        GtkWidget *text_view = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
        gtk_container_add(GTK_CONTAINER(scrolled), text_view);

        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
        GString *display_text = g_string_new("");

        for (int i = 0; i < workspace_count; i++) {
            // Get workspace name
            char workspace_name[64];
            if (names && i < name_count && names[i]) {
                snprintf(workspace_name, sizeof(workspace_name), "%s", names[i]);
            } else {
                snprintf(workspace_name, sizeof(workspace_name), "Workspace %d", i + 1);
            }

            // Format workspace entry
            if (i == user_current_desktop) {
                g_string_append_printf(display_text, "◆%d◆ %s (current)\n", i + 1, workspace_name);
            } else {
                g_string_append_printf(display_text, "[%d] %s\n", i + 1, workspace_name);
            }
        }

        gtk_text_buffer_set_text(buffer, display_text->str, -1);
        g_string_free(display_text, TRUE);
    }

    // Free workspace names
    if (names) {
        for (int i = 0; i < name_count; i++) {
            free(names[i]);
        }
        free(names);
    }

    // Instructions
    GtkWidget *instructions = gtk_label_new("[Press 1-9, 0 for workspace 10, Esc to cancel]");
    gtk_widget_set_halign(instructions, GTK_ALIGN_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(instructions), TRUE);
    gtk_box_pack_end(GTK_BOX(parent_container), instructions, FALSE, FALSE, 0);
}

// Create workspace move overlay content
static void create_workspace_move_overlay_content(GtkWidget *parent_container, AppData *app) {
    // Get selected window using centralized selection management
    WindowInfo *selected_window = get_selected_window(app);
    if (!selected_window) {
        log_error("No window selected for workspace move overlay");
        GtkWidget *error_label = gtk_label_new("No window selected for workspace move");
        gtk_container_add(GTK_CONTAINER(parent_container), error_label);
        return;
    }

    // Header with window title
    char *escaped_title = g_markup_escape_text(selected_window->title, -1);
    char header_text[512];
    snprintf(header_text, sizeof(header_text),
             "<b>Move Window to Workspace:</b> %s", escaped_title);

    GtkWidget *header_label = gtk_label_new(NULL);
    gtk_widget_set_halign(header_label, GTK_ALIGN_CENTER);
    gtk_label_set_markup(GTK_LABEL(header_label), header_text);
    gtk_label_set_line_wrap(GTK_LABEL(header_label), TRUE);
    gtk_box_pack_start(GTK_BOX(parent_container), header_label, FALSE, FALSE, 0);

    g_free(escaped_title);

    // Separator
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(parent_container), separator, FALSE, FALSE, 0);

    // Get workspace information
    int workspace_count = get_number_of_desktops(app->display);
    if (workspace_count > 36) {
        workspace_count = 36;  // Limit to supported maximum
    }

    // Get workspace names
    int name_count;
    char **names = get_desktop_names(app->display, &name_count);

    // Get current desktop (where the user currently is)
    int user_current_desktop = get_current_desktop(app->display);

    // Get window's current desktop
    int window_current_desktop = selected_window->desktop;

    // Create workspace display based on configuration
    if (app->config.workspaces_per_row > 0) {
        // Grid layout
        GtkWidget *grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
        gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(parent_container), grid, TRUE, TRUE, 0);

        int per_row = app->config.workspaces_per_row;

        for (int i = 0; i < workspace_count; i++) {
            int row = i / per_row;
            int col = i % per_row;

            // Get workspace name
            char workspace_name[64];
            if (names && i < name_count && names[i]) {
                snprintf(workspace_name, sizeof(workspace_name), "%s", names[i]);
            } else {
                snprintf(workspace_name, sizeof(workspace_name), "Workspace %d", i + 1);
            }

            GtkWidget *ws_widget = create_workspace_widget_overlay(
                i + 1,  // 1-based number for display
                workspace_name,
                (i == window_current_desktop),  // Is window current
                (i == user_current_desktop)     // Is user current
            );
            gtk_grid_attach(GTK_GRID(grid), ws_widget, col, row, 1, 1);
        }
    } else {
        // Linear list layout
        GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request(scrolled, 400, 200);
        gtk_box_pack_start(GTK_BOX(parent_container), scrolled, TRUE, TRUE, 0);

        GtkWidget *text_view = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
        gtk_container_add(GTK_CONTAINER(scrolled), text_view);

        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
        GString *display_text = g_string_new("");

        for (int i = 0; i < workspace_count; i++) {
            // Get workspace name
            char workspace_name[64];
            if (names && i < name_count && names[i]) {
                snprintf(workspace_name, sizeof(workspace_name), "%s", names[i]);
            } else {
                snprintf(workspace_name, sizeof(workspace_name), "Workspace %d", i + 1);
            }

            // Format workspace entry with indicators
            if (i == window_current_desktop && i == user_current_desktop) {
                g_string_append_printf(display_text, "★%d★ %s (window & user here)\n", i + 1, workspace_name);
            } else if (i == window_current_desktop) {
                g_string_append_printf(display_text, "●%d● %s (window here)\n", i + 1, workspace_name);
            } else if (i == user_current_desktop) {
                g_string_append_printf(display_text, "◆%d◆ %s (user here)\n", i + 1, workspace_name);
            } else {
                g_string_append_printf(display_text, "[%d] %s\n", i + 1, workspace_name);
            }
        }

        gtk_text_buffer_set_text(buffer, display_text->str, -1);
        g_string_free(display_text, TRUE);
    }

    // Free workspace names
    if (names) {
        for (int i = 0; i < name_count; i++) {
            free(names[i]);
        }
        free(names);
    }

    // Instructions
    GtkWidget *instructions = gtk_label_new("[Press 1-9, 0 for workspace 10, Esc to cancel]");
    gtk_widget_set_halign(instructions, GTK_ALIGN_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(instructions), TRUE);
    gtk_box_pack_end(GTK_BOX(parent_container), instructions, FALSE, FALSE, 0);
}

// Create a single workspace widget for overlay grid layout
static GtkWidget* create_workspace_widget_overlay(int workspace_num, const char *workspace_name,
                                                  gboolean is_current, gboolean is_user_current) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(box, 120, 80);

    // Number label with different indicators for different states
    GtkWidget *number_label = gtk_label_new(NULL);
    char number_text[64];
    if (is_current && is_user_current) {
        // Both window and user are on this workspace
        snprintf(number_text, sizeof(number_text), "<b>★%d★</b>", workspace_num);
    } else if (is_current) {
        // Window is on this workspace
        snprintf(number_text, sizeof(number_text), "<b>●%d●</b>", workspace_num);
    } else if (is_user_current) {
        // User is on this workspace
        snprintf(number_text, sizeof(number_text), "<b>◆%d◆</b>", workspace_num);
    } else {
        // Neither window nor user is on this workspace
        snprintf(number_text, sizeof(number_text), "<b>[%d]</b>", workspace_num);
    }
    gtk_label_set_markup(GTK_LABEL(number_label), number_text);
    gtk_box_pack_start(GTK_BOX(box), number_label, FALSE, FALSE, 0);

    // Name label
    GtkWidget *name_label = gtk_label_new(workspace_name);
    gtk_label_set_line_wrap(GTK_LABEL(name_label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(name_label), 15);
    gtk_box_pack_start(GTK_BOX(box), name_label, FALSE, FALSE, 0);

    // Current indicator
    if (is_current) {
        GtkWidget *current_label = gtk_label_new("(current)");
        gtk_box_pack_start(GTK_BOX(box), current_label, FALSE, FALSE, 0);
    }

    // Style the box based on different states
    GtkCssProvider *provider = gtk_css_provider_new();
    GtkStyleContext *context = gtk_widget_get_style_context(box);

    if (is_current && is_user_current) {
        // Both window and user are here - bright highlight
        gtk_widget_set_name(box, "workspace-both");
        gtk_css_provider_load_from_data(provider,
            "#workspace-both { background-color: #666666; border: 2px solid #888888; padding: 8px; }", -1, NULL);
    } else if (is_current) {
        // Window is here - medium highlight
        gtk_widget_set_name(box, "workspace-window");
        gtk_css_provider_load_from_data(provider,
            "#workspace-window { background-color: #444444; border: 1px solid #666666; padding: 9px; }", -1, NULL);
    } else if (is_user_current) {
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

// Handle key press events for workspace jump overlay
static gboolean handle_workspace_jump_key_press(AppData *app, GdkEventKey *event) {
    // Get workspace count
    int workspace_count = get_number_of_desktops(app->display);
    if (workspace_count > 36) {
        workspace_count = 36;  // Limit to supported maximum
    }

    // Handle number keys 1-9
    if (event->keyval >= GDK_KEY_1 && event->keyval <= GDK_KEY_9) {
        int workspace_num = event->keyval - GDK_KEY_1 + 1;  // 1-based
        if (workspace_num <= workspace_count) {
            int target_workspace_idx = workspace_num - 1;  // Convert to 0-based index

            // Get current workspace
            int current_workspace = get_current_desktop(app->display);

            // Jump to the workspace if needed
            if (target_workspace_idx != current_workspace) {
                log_debug("=== EXECUTING WORKSPACE JUMP ===");
                log_debug("Jumping from workspace %d to workspace %d",
                          current_workspace + 1, target_workspace_idx + 1);

                switch_to_desktop(app->display, target_workspace_idx);

                log_info("USER: Jumped from workspace %d to workspace %d",
                         current_workspace + 1, target_workspace_idx + 1);
            } else {
                log_debug("Already on target workspace %d", target_workspace_idx + 1);
            }

            // Hide overlay and close application
            hide_overlay(app);
            destroy_window(app);
        }
        return TRUE;
    }

    // Handle 0 for workspace 10
    if (event->keyval == GDK_KEY_0) {
        if (workspace_count >= 10) {
            int target_workspace_idx = 9;  // Workspace 10 is index 9

            // Get current workspace
            int current_workspace = get_current_desktop(app->display);

            // Jump to the workspace if needed
            if (target_workspace_idx != current_workspace) {
                log_debug("=== EXECUTING WORKSPACE JUMP ===");
                log_debug("Jumping from workspace %d to workspace %d",
                          current_workspace + 1, target_workspace_idx + 1);

                switch_to_desktop(app->display, target_workspace_idx);

                log_info("USER: Jumped from workspace %d to workspace %d",
                         current_workspace + 1, target_workspace_idx + 1);
            } else {
                log_debug("Already on target workspace %d", target_workspace_idx + 1);
            }

            // Hide overlay and close application
            hide_overlay(app);
            destroy_window(app);
        }
        return TRUE;
    }

    return FALSE; // Invalid key, don't handle
}

// Handle key press events for workspace move overlay
static gboolean handle_workspace_move_key_press(AppData *app, GdkEventKey *event) {
    // Get selected window
    WindowInfo *selected_window = get_selected_window(app);
    if (!selected_window) {
        log_error("No window selected for workspace move");
        hide_overlay(app);
        return TRUE;
    }

    // Get workspace count
    int workspace_count = get_number_of_desktops(app->display);
    if (workspace_count > 36) {
        workspace_count = 36;  // Limit to supported maximum
    }

    // Handle number keys 1-9
    if (event->keyval >= GDK_KEY_1 && event->keyval <= GDK_KEY_9) {
        int workspace_num = event->keyval - GDK_KEY_1 + 1;  // 1-based
        if (workspace_num <= workspace_count) {
            int target_workspace_idx = workspace_num - 1;  // Convert to 0-based index

            log_debug("=== EXECUTING WORKSPACE MOVE ===");
            log_debug("Moving window '%s' (ID: 0x%lx) to workspace %d",
                      selected_window->title, selected_window->id, target_workspace_idx + 1);

            // Move the window to the target workspace
            move_window_to_desktop(app->display, selected_window->id, target_workspace_idx);

            log_info("USER: Moved window '%s' to workspace %d",
                     selected_window->title, target_workspace_idx + 1);

            // Hide overlay and close application
            hide_overlay(app);
            destroy_window(app);
        }
        return TRUE;
    }

    // Handle 0 for workspace 10
    if (event->keyval == GDK_KEY_0) {
        if (workspace_count >= 10) {
            int target_workspace_idx = 9;  // Workspace 10 is index 9

            log_debug("=== EXECUTING WORKSPACE MOVE ===");
            log_debug("Moving window '%s' (ID: 0x%lx) to workspace %d",
                      selected_window->title, selected_window->id, target_workspace_idx + 1);

            // Move the window to the target workspace
            move_window_to_desktop(app->display, selected_window->id, target_workspace_idx);

            log_info("USER: Moved window '%s' to workspace %d",
                     selected_window->title, target_workspace_idx + 1);

            // Hide overlay and close application
            hide_overlay(app);
            destroy_window(app);
        }
        return TRUE;
    }

    return FALSE; // Invalid key, don't handle
}

// Center dialog content in the overlay (utility function)
void center_dialog_in_overlay(GtkWidget *dialog_content, AppData *app) {
    // This is handled by the dialog_container's alignment properties
    // Additional centering logic can be added here if needed
    (void)app; // Suppress unused parameter warning
    gtk_widget_set_halign(dialog_content, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(dialog_content, GTK_ALIGN_CENTER);
}
