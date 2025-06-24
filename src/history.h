#ifndef HISTORY_H
#define HISTORY_H

#include "app_data.h"

// Update history with current window list
void update_history(AppData *app);

// Partition windows by type and reorder (Normal first, then Special)
void partition_and_reorder(AppData *app);

// Apply Alt-Tab swap (swap first two windows for quick toggling)
void apply_alttab_swap(AppData *app);

#endif // HISTORY_H