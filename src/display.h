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

// Get maximum number of lines that can be displayed (legacy function)
int get_max_display_lines(void);

// Get maximum number of lines using dynamic calculation
int get_max_display_lines_dynamic(AppData *app);

// Generate text-based scrollbar
void generate_scrollbar(int total_items, int visible_items, int scroll_offset, char *scrollbar, int scrollbar_height);

// Overlay scrollbar on last char of each line in text (in place).
// For bottom-up display, pass flipped offset: (total - visible) - offset.
void overlay_scrollbar(GString *text, int total_items, int visible_items, int scroll_offset);

// Activate window using direct X11 calls
void activate_window(Display *display, Window window_id);

#endif // DISPLAY_H