#ifndef OVERLAY_RULES_H
#define OVERLAY_RULES_H

#include <gtk/gtk.h>

#include "core/app/app_data.h"

void create_rule_add_overlay_content(GtkWidget *parent_container, AppData *app);
void create_rule_edit_overlay_content(GtkWidget *parent_container, AppData *app);

gboolean handle_rule_add_key_press(AppData *app, GdkEventKey *event);
gboolean handle_rule_edit_key_press(AppData *app, GdkEventKey *event);
void show_rule_delete_confirm(AppData *app, int rule_index);

#endif
