#ifndef WORKSPACE_UTILS_H
#define WORKSPACE_UTILS_H

#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include "app_data.h"

// Maximum number of workspaces we support
#define MAX_WORKSPACE_COUNT 36

// Workspace names management structure
typedef struct {
    char **names;
    int count;
} WorkspaceNames;

// Get workspace count with maximum limit applied
int get_limited_workspace_count(Display *display);

// Workspace names management with automatic cleanup
WorkspaceNames* get_workspace_names(Display *display);
void free_workspace_names(WorkspaceNames *ws_names);
const char* get_workspace_name_or_default(WorkspaceNames *names, int index);

// Common GTK widget creation helpers
GtkWidget* create_workspace_instructions(GtkWidget *parent_container);

// Workspace number key handling
typedef enum {
    WORKSPACE_ACTION_JUMP,
    WORKSPACE_ACTION_MOVE_WINDOW,
    WORKSPACE_ACTION_MOVE_ALL
} WorkspaceAction;

// Parse workspace number from keyboard event (1-9, 0 for 10)
// Returns -1 if not a workspace number key
int parse_workspace_number_key(GdkEventKey *event);

// Get target workspace index from number key (handles 1-9, 0 for 10)
// Returns 0-based index or -1 if invalid
int get_workspace_index_from_key(GdkEventKey *event, int workspace_count);

#endif // WORKSPACE_UTILS_H