#ifndef REPEAT_ACTION_H
#define REPEAT_ACTION_H

#ifndef APPDATA_TYPEDEF_DEFINED
#define APPDATA_TYPEDEF_DEFINED
typedef struct AppData AppData;
#endif

// Store the query used in a successful windows-tab activation.
// Called immediately before activate_window on Enter.
void store_last_windows_query(AppData *app, const char *query);

// Execute the repeat action: re-filter the live window list with the
// stored query and activate the top match.  Returns without activating
// if no state has been stored or the query yields no current matches.
// Designed to be called only when the entry is empty and the tab is
// TAB_WINDOWS — the caller (key_handler) is responsible for that gate.
void handle_repeat_key(AppData *app);

#endif // REPEAT_ACTION_H
