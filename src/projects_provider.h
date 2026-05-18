#ifndef PROJECTS_PROVIDER_H
#define PROJECTS_PROVIDER_H

#include <gtk/gtk.h>

#include "app_data.h"

void projects_provider_register(void);
gboolean handle_projects_tab_keys(GdkEventKey *event, AppData *app);

#endif /* PROJECTS_PROVIDER_H */
