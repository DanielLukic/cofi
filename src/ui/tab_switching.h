#ifndef TAB_SWITCHING_H
#define TAB_SWITCHING_H

#include <gtk/gtk.h>

#include "core/app/app_data.h"

void switch_to_tab(AppData *app, TabMode target_tab);
void surface_tab(AppData *app, TabMode tab);
void clear_surfaced_tabs(AppData *app);
gboolean tab_is_visible(AppData *app, TabMode tab);
gboolean handle_tab_switching(GdkEventKey *event, AppData *app);

#endif
