#ifndef OVERLAY_HARPOON_H
#define OVERLAY_HARPOON_H

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "core/app/app_data.h"

void show_harpoon_delete_confirm(AppData *app, int slot_index);
void focus_edit_entry_delayed(AppData *app);

#endif
