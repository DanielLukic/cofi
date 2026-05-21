#ifndef OVERLAY_PROJECTS_H
#define OVERLAY_PROJECTS_H

#include <gtk/gtk.h>

#include "app_data.h"

void create_project_kill_overlay_content(GtkWidget *parent_container, AppData *app);
void create_project_rename_overlay_content(GtkWidget *parent_container, AppData *app);
void create_project_new_overlay_content(GtkWidget *parent_container, AppData *app);
void create_project_remote_host_overlay_content(GtkWidget *parent_container, AppData *app);

gboolean handle_project_kill_key_press(AppData *app, GdkEventKey *event);
gboolean handle_project_rename_key_press(AppData *app, GdkEventKey *event);
gboolean handle_project_new_key_press(AppData *app, GdkEventKey *event);
gboolean handle_project_remote_host_key_press(AppData *app, GdkEventKey *event);

#endif
