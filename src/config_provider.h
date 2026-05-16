#ifndef CONFIG_PROVIDER_H
#define CONFIG_PROVIDER_H

#include "app_data.h"

void config_provider_register(void);
void filter_config(AppData *app, const char *filter);

ConfigEntry *config_selected_entry(AppData *app);
void config_select_key(AppData *app, const char *key);

#endif
