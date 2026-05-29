#ifndef MATCHING_PROVIDER_H
#define MATCHING_PROVIDER_H

#include "core/app/app_data.h"

void matching_provider_register(void);
TabMode matching_tab_mode(void);

MatchEntry *matching_selected_entry(AppData *app);
int matching_selected_manager_index(AppData *app);

#endif
