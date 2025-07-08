#include "tiling_overlay.h"
#include "app_data.h"
#include "log.h"
#include "selection.h"
#include "tiling.h"
#include <gtk/gtk.h>

// Forward declarations
static void create_tiling_grid_overlay(GtkWidget *parent_box, AppData *app);
extern void destroy_window(AppData *app); // From main.c

// Create tiling overlay content
void create_tiling_overlay_content(GtkWidget *parent_container, AppData *app) {
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

// Helper function to create tiling grid display
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

// Handle key press events for tiling overlay
gboolean handle_tiling_overlay_key_press(AppData *app, GdkEventKey *event) {
    // Get selected window
    WindowInfo *selected_window = get_selected_window(app);
    if (!selected_window) {
        log_error("No window selected for tiling");
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
            if (event->state & GDK_CONTROL_MASK) {
                option = TILE_GRID_1_WIDE;
            } else {
                option = TILE_GRID_1;
            }
            valid_option = (1 <= max_positions);
            break;
        case GDK_KEY_2:
            if (event->state & GDK_CONTROL_MASK) {
                option = TILE_GRID_2_WIDE;
            } else {
                option = TILE_GRID_2;
            }
            valid_option = (2 <= max_positions);
            break;
        case GDK_KEY_3:
            if (event->state & GDK_CONTROL_MASK) {
                option = TILE_GRID_3_WIDE;
            } else {
                option = TILE_GRID_3;
            }
            valid_option = (3 <= max_positions);
            break;
        case GDK_KEY_4:
            if (event->state & GDK_CONTROL_MASK) {
                option = TILE_GRID_4_WIDE;
            } else {
                option = TILE_GRID_4;
            }
            valid_option = (4 <= max_positions);
            break;
        case GDK_KEY_5:
            if (event->state & GDK_CONTROL_MASK) {
                option = TILE_GRID_5_WIDE;
            } else {
                option = TILE_GRID_5;
            }
            valid_option = (5 <= max_positions);
            break;
        case GDK_KEY_6:
            if (event->state & GDK_CONTROL_MASK) {
                option = TILE_GRID_6_WIDE;
            } else {
                option = TILE_GRID_6;
            }
            valid_option = (6 <= max_positions);
            break;
        case GDK_KEY_7:
            if (event->state & GDK_CONTROL_MASK) {
                option = TILE_GRID_7_WIDE;
            } else {
                option = TILE_GRID_7;
            }
            valid_option = (7 <= max_positions);
            break;
        case GDK_KEY_8:
            if (event->state & GDK_CONTROL_MASK) {
                option = TILE_GRID_8_WIDE;
            } else {
                option = TILE_GRID_8;
            }
            valid_option = (8 <= max_positions);
            break;
        case GDK_KEY_9:
            if (event->state & GDK_CONTROL_MASK) {
                option = TILE_GRID_9_WIDE;
            } else {
                option = TILE_GRID_9;
            }
            valid_option = (9 <= max_positions);
            break;
        case GDK_KEY_f:
        case GDK_KEY_F:
            option = TILE_FULLSCREEN;
            break;
        case GDK_KEY_c:
        case GDK_KEY_C:
            if (event->state & GDK_CONTROL_MASK && event->state & GDK_SHIFT_MASK) {
                option = TILE_CENTER_THREE_QUARTERS;
            } else if (event->state & GDK_CONTROL_MASK) {
                option = TILE_CENTER_TWO_THIRDS;
            } else if (event->state & GDK_SHIFT_MASK) {
                option = TILE_CENTER_THIRD;
            } else {
                option = TILE_CENTER;
            }
            break;
        default:
            valid_option = FALSE;
            break;
    }

    if (valid_option) {
        log_info("USER: Tiling window '%s' with option %d", selected_window->title, option);

        // Apply tiling using the existing function from tiling_dialog.c
        apply_tiling(app->display, selected_window->id, option, tile_columns);

        // Close the main application (similar to original tiling dialog behavior)
        destroy_window(app);

        return TRUE;
    }

    return FALSE; // Invalid key, don't handle
}