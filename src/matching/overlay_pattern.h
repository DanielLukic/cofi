#ifndef OVERLAY_PATTERN_H
#define OVERLAY_PATTERN_H

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "core/app/app_data.h"

int selected_match_id_for_pattern_edit(AppData *app);
gboolean show_pattern_edit_overlay(AppData *app, int match_id, const char *context_line);
void create_pattern_edit_overlay_content(GtkWidget *parent_container, AppData *app);
gboolean handle_pattern_edit_key_press(AppData *app, GdkEventKey *event);

#endif
