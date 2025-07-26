#ifndef NAMED_WINDOW_H
#define NAMED_WINDOW_H

#include <X11/Xlib.h>
#include "window_info.h"
#include "constants.h"

// Structure to store a custom window name
typedef struct NamedWindow {
    Window id;                          // X11 window ID
    char custom_name[MAX_TITLE_LEN];   // User-defined custom name
    char original_title[MAX_TITLE_LEN]; // Original title (can use wildcards for matching)
    char class_name[MAX_CLASS_LEN];    // Window class name
    char instance[MAX_CLASS_LEN];      // Window instance name
    char type[16];                     // Window type ("Normal" or "Special")
    int assigned;                      // 1 if matched to existing window, 0 if orphaned
} NamedWindow;

// Manager for all named windows
typedef struct {
    NamedWindow entries[MAX_WINDOWS];
    int count;
} NamedWindowManager;

// Initialize the named window manager
void init_named_window_manager(NamedWindowManager *manager);

// Assign a custom name to a window
void assign_custom_name(NamedWindowManager *manager, const WindowInfo *window, const char *custom_name);

// Get the custom name for a window (returns NULL if no custom name)
const char* get_window_custom_name(const NamedWindowManager *manager, Window id);

// Check if a window already has a custom name
int is_window_already_named(const NamedWindowManager *manager, Window id);

// Check and reassign orphaned names to matching windows
void check_and_reassign_names(NamedWindowManager *manager, WindowInfo *windows, int window_count);

// Delete a custom name by index
void delete_custom_name(NamedWindowManager *manager, int index);

// Update an existing custom name
void update_custom_name(NamedWindowManager *manager, int index, const char *new_name);

// Get named window by index
NamedWindow* get_named_window_by_index(NamedWindowManager *manager, int index);

// Find named window index by window ID
int find_named_window_index(const NamedWindowManager *manager, Window id);

#endif // NAMED_WINDOW_H