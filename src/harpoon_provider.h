#ifndef HARPOON_PROVIDER_H
#define HARPOON_PROVIDER_H

#include <gtk/gtk.h>

#include "app_data.h"

void harpoon_provider_register(void);
TabMode harpoon_tab_mode(void);
void filter_harpoon(AppData *app, const char *filter);
HarpoonSlot *harpoon_selected_slot(AppData *app, int *actual_slot_out);
gboolean handle_harpoon_tab_keys(GdkEventKey *event, AppData *app);

#endif /* HARPOON_PROVIDER_H */
