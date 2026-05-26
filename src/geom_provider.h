#ifndef GEOM_PROVIDER_H
#define GEOM_PROVIDER_H

#include "app_data.h"

TabMode geom_tab_mode(void);
void geom_provider_register(void);
gboolean handle_geom_tab_keys(GdkEventKey *event, AppData *app);
void geom_on_query_changed(AppData *app, const char *query);

#endif
