#ifndef NAMES_PROVIDER_H
#define NAMES_PROVIDER_H

#include <gdk/gdk.h>

#include "core/app/app_data.h"
#include "names/names_store.h"

void names_provider_register(void);
TabMode names_tab_mode(void);
void names_on_query_changed(AppData *app, const char *query);
gboolean handle_names_tab_keys(GdkEventKey *event, AppData *app);
NameRecord *names_selected_record(AppData *app);
int names_selected_store_index(AppData *app);
void names_select_custom_name(AppData *app, const char *custom_name);

#endif
