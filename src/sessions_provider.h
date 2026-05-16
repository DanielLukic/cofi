#ifndef SESSIONS_PROVIDER_H
#define SESSIONS_PROVIDER_H

#include <gtk/gtk.h>

#include "app_data.h"

void sessions_provider_register(void);
gboolean handle_sessions_tab_keys(GdkEventKey *event, AppData *app);

#endif /* SESSIONS_PROVIDER_H */
