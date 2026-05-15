#ifndef OVERLAY_SESSIONS_H
#define OVERLAY_SESSIONS_H

#include <gtk/gtk.h>

#include "app_data.h"

void create_session_kill_overlay_content(GtkWidget *parent_container, AppData *app);
void create_session_rename_overlay_content(GtkWidget *parent_container, AppData *app);
void create_session_new_overlay_content(GtkWidget *parent_container, AppData *app);

gboolean handle_session_kill_key_press(AppData *app, GdkEventKey *event);
gboolean handle_session_rename_key_press(AppData *app, GdkEventKey *event);
gboolean handle_session_new_key_press(AppData *app, GdkEventKey *event);

#endif
