#ifndef HISTORY_H
#define HISTORY_H

// Forward declaration (avoid duplicate typedef)
#ifndef APPDATA_TYPEDEF_DEFINED
#define APPDATA_TYPEDEF_DEFINED
typedef struct AppData AppData;
#endif

// Update history with current window list
void update_history(AppData *app);

// Partition windows by type and reorder (Normal first, then Special)
void partition_and_reorder(AppData *app);

// Apply Alt-Tab swap (swap first two windows for quick toggling)
void apply_alttab_swap(AppData *app);

#endif // HISTORY_H