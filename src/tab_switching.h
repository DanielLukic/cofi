#ifndef TAB_SWITCHING_H
#define TAB_SWITCHING_H

#include <gtk/gtk.h>

#include "app_data.h"

void switch_to_tab(AppData *app, TabMode target_tab);
void surface_tab(AppData *app, TabMode tab);
void clear_surfaced_tabs(AppData *app);
gboolean tab_is_visible(AppData *app, TabMode tab);
gboolean handle_tab_switching(GdkEventKey *event, AppData *app);
void filter_workspaces(AppData *app, const char *filter);
void filter_harpoon(AppData *app, const char *filter);
void filter_config(AppData *app, const char *filter);
void filter_hotkeys(AppData *app, const char *filter);
void filter_apps(AppData *app, const char *filter);

#endif
