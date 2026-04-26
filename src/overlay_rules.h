#ifndef OVERLAY_RULES_H
#define OVERLAY_RULES_H

#include <gtk/gtk.h>

#include "app_data.h"

void create_rule_add_overlay_content(GtkWidget *parent_container, AppData *app);
void create_rule_edit_overlay_content(GtkWidget *parent_container, AppData *app);
void create_rule_delete_overlay_content(GtkWidget *parent_container, AppData *app);

gboolean handle_rule_add_key_press(AppData *app, GdkEventKey *event);
gboolean handle_rule_edit_key_press(AppData *app, GdkEventKey *event);
gboolean handle_rule_delete_key_press(AppData *app, GdkEventKey *event);

#endif
