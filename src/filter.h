#ifndef FILTER_H
#define FILTER_H

// Forward declaration (avoid duplicate typedef)
#ifndef APPDATA_TYPEDEF_DEFINED
#define APPDATA_TYPEDEF_DEFINED
typedef struct AppData AppData;
#endif

// Filter windows based on search text
void filter_windows(AppData *app, const char *filter);

// Apply alt-tab selection logic
void apply_alt_tab_selection(AppData *app);

#endif // FILTER_H