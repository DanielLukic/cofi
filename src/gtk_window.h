#ifndef GTK_WINDOW_H
#define GTK_WINDOW_H

#include <gtk/gtk.h>
#include "app_data.h"

// Window positioning and alignment functions
void apply_window_position(GtkWidget *window, WindowAlignment alignment);
void on_window_size_allocate(GtkWidget *window, GtkAllocation *allocation, gpointer user_data);

#endif // GTK_WINDOW_H