#ifndef WINDOW_LIST_H
#define WINDOW_LIST_H

// Forward declaration (avoid duplicate typedef)
#ifndef APPDATA_TYPEDEF_DEFINED
#define APPDATA_TYPEDEF_DEFINED
typedef struct AppData AppData;
#endif

// Get list of all windows using _NET_CLIENT_LIST
void get_window_list(AppData *app);

#endif // WINDOW_LIST_H