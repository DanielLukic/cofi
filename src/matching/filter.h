#ifndef FILTER_H
#define FILTER_H
#include <stddef.h>
#include "x11/window_info.h"
#include "matching/match_entry.h"

// Forward declaration (avoid duplicate typedef)
#ifndef APPDATA_TYPEDEF_DEFINED
#define APPDATA_TYPEDEF_DEFINED
typedef struct AppData AppData;
#endif

// Filter windows based on search text
void filter_windows(AppData *app, const char *filter);

// Apply alt-tab selection logic
void apply_alt_tab_selection(AppData *app, const char *filter);

// Compose the user-visible window title (custom name prefix when present).
void compose_window_display_title(const MatchEntryManager *manager,
                                  const WindowInfo *window,
                                  char *out, size_t out_size);

#endif // FILTER_H
