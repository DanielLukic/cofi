#include "gtk_utils.h"
#include "log.h"
#include <stdarg.h>
#include <stdio.h>

// Add horizontal separator to a box
GtkWidget* add_horizontal_separator(GtkWidget *parent_box) {
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(parent_box), separator, FALSE, FALSE, 0);
    return separator;
}

// Add vertical separator to a box
GtkWidget* add_vertical_separator(GtkWidget *parent_box) {
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(parent_box), separator, FALSE, FALSE, 0);
    return separator;
}

// Create a centered label
GtkWidget* create_centered_label(const char *text) {
    GtkWidget *label = gtk_label_new(text);
    gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    return label;
}

// Create a label with markup
GtkWidget* create_markup_label(const char *markup, gboolean center) {
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    if (center) {
        gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
    }
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    return label;
}

// Create a header label and add it to container
GtkWidget* create_header_label(const char *text, GtkWidget *parent_container) {
    char markup[512];
    snprintf(markup, sizeof(markup), "<b>%s</b>", text);
    
    GtkWidget *header = create_markup_label(markup, TRUE);
    gtk_box_pack_start(GTK_BOX(parent_container), header, FALSE, FALSE, 0);
    
    // Add separator after header
    add_horizontal_separator(parent_container);
    
    return header;
}

// Show "No window selected" error
void show_no_window_error(GtkWidget *parent_container, const char *operation) {
    log_error("No window selected for %s", operation);
    
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "No window selected for %s", operation);
    
    GtkWidget *error_label = gtk_label_new(error_msg);
    gtk_widget_set_halign(error_label, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(parent_container), error_label);
}

// Show formatted error message
void show_error_message(GtkWidget *parent_container, const char *format, ...) {
    char buffer[512];
    va_list args;
    
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    log_error("%s", buffer);
    
    GtkWidget *error_label = gtk_label_new(buffer);
    gtk_widget_set_halign(error_label, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(parent_container), error_label);
}

// Create a scrolled text view
GtkWidget* create_scrolled_text_view(int width, int height) {
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled, width, height);
    
    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolled), text_view);
    
    return scrolled;
}

// Create a standard dialog container
GtkWidget* create_dialog_container(void) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
    gtk_widget_set_size_request(vbox, 400, -1);
    return vbox;
}

// Create a standard grid
GtkWidget* create_standard_grid(int row_spacing, int col_spacing) {
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), row_spacing);
    gtk_grid_set_column_spacing(GTK_GRID(grid), col_spacing);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);
    return grid;
}