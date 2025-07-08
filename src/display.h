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

// Get maximum number of lines that can be displayed
int get_max_display_lines(void);

// Generate text-based scrollbar
void generate_scrollbar(int total_items, int visible_items, int scroll_offset, char *scrollbar, int scrollbar_height);

// Activate window using wmctrl
void activate_window(Window window_id);

#endif // DISPLAY_H