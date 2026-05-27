#ifndef CONFIG_PROVIDER_H
#define CONFIG_PROVIDER_H

#include "core/app/app_data.h"

void config_provider_register(void);
TabMode config_tab_mode(void);
void filter_config(AppData *app, const char *filter);
gboolean handle_config_tab_keys(GdkEventKey *event, AppData *app);

ConfigEntry *config_selected_entry(AppData *app);
void config_select_key(AppData *app, const char *key);
int config_entry_allows_edit(const ConfigEntry *entry);

#endif
