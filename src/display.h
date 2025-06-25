#ifndef DISPLAY_H
#define DISPLAY_H

#include <X11/Xlib.h>

// Forward declaration (avoid duplicate typedef)
#ifndef APPDATA_TYPEDEF_DEFINED
#define APPDATA_TYPEDEF_DEFINED
typedef struct AppData AppData;
#endif

// Update the text display with proper 5-column format
void update_display(AppData *app);

// Activate window using wmctrl
void activate_window(Window window_id);

#endif // DISPLAY_H