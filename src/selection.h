#ifndef SELECTION_H
#define SELECTION_H

#include "app_data.h"

// Selection management functions
void init_selection(AppData *app);
void reset_selection(AppData *app);

// Get currently selected items
WindowInfo* get_selected_window(AppData *app);
WorkspaceInfo* get_selected_workspace(AppData *app);
int get_selected_index(AppData *app);

// Selection movement
void move_selection_up(AppData *app);
void move_selection_down(AppData *app);

// Selection preservation across filtering
void preserve_selection(AppData *app);
void restore_selection(AppData *app);

// Validation and bounds checking
void validate_selection(AppData *app);

// Scroll management
void update_scroll_position(AppData *app);
int get_scroll_offset(AppData *app);
void set_scroll_offset(AppData *app, int offset);
int get_selected_index(AppData *app);

#endif // SELECTION_H
