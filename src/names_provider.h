#ifndef NAMES_PROVIDER_H
#define NAMES_PROVIDER_H

#include "app_data.h"

void names_provider_register(void);
TabMode names_tab_mode(void);
gboolean handle_names_tab_keys(GdkEventKey *event, AppData *app);

NamedWindow *names_selected_entry(AppData *app);
int names_selected_manager_index(AppData *app);
void names_select_custom_name(AppData *app, const char *custom_name);

#endif
