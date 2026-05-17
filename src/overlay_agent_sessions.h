#ifndef OVERLAY_AGENT_SESSIONS_H
#define OVERLAY_AGENT_SESSIONS_H

#include <gtk/gtk.h>

#include "app_data.h"

void create_agent_session_delete_overlay_content(GtkWidget *parent_container,
                                                 AppData *app);
gboolean handle_agent_session_delete_key_press(AppData *app, GdkEventKey *event);

#endif /* OVERLAY_AGENT_SESSIONS_H */
