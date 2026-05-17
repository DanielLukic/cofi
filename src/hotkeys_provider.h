#ifndef HOTKEYS_PROVIDER_H
#define HOTKEYS_PROVIDER_H

#include "app_data.h"

void hotkeys_provider_register(void);
TabMode hotkeys_tab_mode(void);
void filter_hotkeys(AppData *app, const char *filter);
gboolean handle_hotkeys_tab_keys(GdkEventKey *event, AppData *app);

HotkeyBinding *hotkeys_selected_binding(AppData *app, int *master_idx_out);
void hotkeys_select_key(AppData *app, const char *key);

#endif
