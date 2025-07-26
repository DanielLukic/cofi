#ifndef HISTORY_H
#define HISTORY_H

// Forward declaration (avoid duplicate typedef)
#ifndef APPDATA_TYPEDEF_DEFINED
#define APPDATA_TYPEDEF_DEFINED
typedef struct AppData AppData;
#endif

// Update history with current window list
void update_history(AppData *app);

// Partition windows by type and workspace, maintaining MRU order within each group
// Order: Active window, Current Normal, Other Normal, Current Special, Other Special, Sticky
void partition_and_reorder(AppData *app);

#endif // HISTORY_H