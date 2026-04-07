#ifndef APP_SETUP_H
#define APP_SETUP_H

#include <gtk/gtk.h>

#include "app_data.h"
#include "types.h"

int run_cofi(int argc, char *argv[]);
void setup_application(AppData *app, WindowAlignment alignment);
void on_textview_size_allocate_for_fixed_init(GtkWidget *widget,
                                              GtkAllocation *allocation,
                                              gpointer user_data);

#endif
