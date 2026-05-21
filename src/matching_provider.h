#ifndef MATCHING_PROVIDER_H
#define MATCHING_PROVIDER_H

#include "app_data.h"

void matching_provider_register(void);
TabMode matching_tab_mode(void);
gboolean handle_matching_tab_keys(GdkEventKey *event, AppData *app);

MatchEntry *matching_selected_entry(AppData *app);
int matching_selected_manager_index(AppData *app);
void matching_select_custom_name(AppData *app, const char *custom_name);

#endif
