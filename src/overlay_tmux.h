#ifndef OVERLAY_TMUX_H
#define OVERLAY_TMUX_H

#include <gtk/gtk.h>

#include "app_data.h"

void create_tmux_kill_overlay_content(GtkWidget *parent_container, AppData *app);
void create_tmux_rename_overlay_content(GtkWidget *parent_container, AppData *app);
void create_tmux_new_overlay_content(GtkWidget *parent_container, AppData *app);

gboolean handle_tmux_kill_key_press(AppData *app, GdkEventKey *event);
gboolean handle_tmux_rename_key_press(AppData *app, GdkEventKey *event);
gboolean handle_tmux_new_key_press(AppData *app, GdkEventKey *event);

#endif
