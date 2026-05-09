#ifndef PREFIX_TABS_H
#define PREFIX_TABS_H

#include "app_data.h"

void clear_prefix_tab_claim(AppData *app);
gboolean cofi_is_prefix_char(char c);
void cofi_dispatch_prefix(AppData *app, char c);
void apply_prefix_tab_claim(AppData *app, const char *entry_text);

#endif
