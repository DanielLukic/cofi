#ifndef OVERLAY_HOTKEY_ADD_H
#define OVERLAY_HOTKEY_ADD_H

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "app_data.h"

void create_hotkey_add_overlay_content(GtkWidget *parent_container, AppData *app);
gboolean handle_hotkey_add_key_press(AppData *app, GdkEventKey *event);
gboolean overlay_hotkey_add_should_capture_event(const GdkEventKey *event);

/* Rebind helpers — exposed for unit tests. error_label may be NULL. */
gboolean apply_rebind(AppData *app, const char *canonical);
gboolean show_rebind_conflict(AppData *app, GtkWidget *error_label,
                              const char *canonical, int conflict_idx);
gboolean handle_rebind_confirm_key(AppData *app, GdkEventKey *event);

#endif
