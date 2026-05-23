#ifndef OVERLAY_CONFIRM_H
#define OVERLAY_CONFIRM_H

#include <gtk/gtk.h>

#include "app_data.h"

void show_confirm_overlay(AppData *app,
                          const char *title,
                          const char *info,
                          void (*on_confirm)(AppData *));
void create_confirm_overlay_content(GtkWidget *parent_container, AppData *app);
gboolean handle_confirm_overlay_key_press(AppData *app, GdkEventKey *event);
void clear_confirm_overlay_state(AppData *app);

#endif
