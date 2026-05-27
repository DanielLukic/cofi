#ifndef OVERLAY_SESSIONS_H
#define OVERLAY_SESSIONS_H

#include <gtk/gtk.h>

#include "core/app/app_data.h"

void create_session_rename_overlay_content(GtkWidget *parent_container,
                                                 AppData *app);
gboolean handle_session_rename_key_press(AppData *app, GdkEventKey *event);
void show_session_delete_confirm(AppData *app,
                                 const char *source,
                                 const char *session_id,
                                 const char *path);

#endif /* OVERLAY_SESSIONS_H */
