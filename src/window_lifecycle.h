#ifndef WINDOW_LIFECYCLE_H
#define WINDOW_LIFECYCLE_H

#include <gtk/gtk.h>

#include "app_data.h"

void destroy_window(AppData *app);
void hide_window(AppData *app);
void show_window(AppData *app);
gboolean check_focus_loss_delayed(AppData *app);
gboolean on_focus_out_event(GtkWidget *widget, GdkEventFocus *event, AppData *app);
gboolean on_delete_event(GtkWidget *widget, GdkEvent *event, AppData *app);
void ensure_cofi_on_current_workspace(AppData *app);

#endif
