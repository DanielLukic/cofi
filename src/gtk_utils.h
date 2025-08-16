#ifndef GTK_UTILS_H
#define GTK_UTILS_H

#include <gtk/gtk.h>

// Common GTK widget creation helpers

// Separator utilities
GtkWidget* add_horizontal_separator(GtkWidget *parent_box);
GtkWidget* add_vertical_separator(GtkWidget *parent_box);

// Label creation utilities
GtkWidget* create_centered_label(const char *text);
GtkWidget* create_markup_label(const char *markup, gboolean center);
GtkWidget* create_header_label(const char *text, GtkWidget *parent_container);

// Error display utilities
void show_no_window_error(GtkWidget *parent_container, const char *operation);
void show_error_message(GtkWidget *parent_container, const char *format, ...);

// Common dialog patterns
GtkWidget* create_scrolled_text_view(int width, int height);
GtkWidget* create_dialog_container(void);

// Grid utilities
GtkWidget* create_standard_grid(int row_spacing, int col_spacing);

#endif // GTK_UTILS_H