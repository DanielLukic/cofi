#ifndef RULES_PROVIDER_H
#define RULES_PROVIDER_H

#include "app_data.h"
#include "rules_config.h"

void rules_provider_register(void);

Rule *rules_selected_rule(AppData *app);
int   rules_selected_config_index(AppData *app);
void  rules_select_config_index(AppData *app, int config_index);

#endif /* RULES_PROVIDER_H */
