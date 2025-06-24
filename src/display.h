#ifndef DISPLAY_H
#define DISPLAY_H

#include "app_data.h"

// Update the text display with proper 5-column format
void update_display(AppData *app);

// Activate window using wmctrl
void activate_window(Window window_id);

#endif // DISPLAY_H