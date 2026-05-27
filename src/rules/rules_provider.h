#ifndef RULES_PROVIDER_H
#define RULES_PROVIDER_H

#include "core/app/app_data.h"
#include "rules/rules_config.h"

void rules_provider_register(void);
TabMode rules_tab_mode(void);
gboolean handle_rules_tab_keys(GdkEventKey *event, AppData *app);

void  filter_rules(AppData *app, const char *filter);
Rule *rules_selected_rule(AppData *app);
int   rules_selected_config_index(AppData *app);
void  rules_select_config_index(AppData *app, int config_index);

#endif /* RULES_PROVIDER_H */
