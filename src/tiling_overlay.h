#ifndef TILING_OVERLAY_H
#define TILING_OVERLAY_H

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "app_data.h"

// Tiling overlay content creation
void create_tiling_overlay_content(GtkWidget *parent_container, AppData *app);

// Tiling overlay key press handling
gboolean handle_tiling_overlay_key_press(AppData *app, GdkEventKey *event);

#endif // TILING_OVERLAY_H