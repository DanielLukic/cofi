#ifndef APPS_PROVIDER_H
#define APPS_PROVIDER_H

#include "app_data.h"

void apps_provider_register(void);
TabMode apps_tab_mode(void);
void filter_apps(AppData *app, const char *filter);

#endif /* APPS_PROVIDER_H */
