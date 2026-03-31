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

// Overlay scrollbar on the rightmost column of each line in text (in place).
// Pads/truncates each line to target_columns, places scrollbar char at the last position.
// For bottom-up display, pass flipped offset: (total - visible) - offset.
void overlay_scrollbar(GString *text, int total_items, int visible_items, int scroll_offset, int target_columns);

// Activate window using direct X11 calls
void activate_window(Display *display, Window window_id);

// Switch to a specific tab (handles filtering, placeholder text, display refresh)
#include "app_data.h"
void switch_to_tab(AppData *app, TabMode target_tab);

#endif // DISPLAY_H