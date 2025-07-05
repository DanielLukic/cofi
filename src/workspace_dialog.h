#ifndef WORKSPACE_DIALOG_H
#define WORKSPACE_DIALOG_H

#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include "window_info.h"

typedef struct {
    int index;               // 0-based index (for X11 API)
    int number;              // 1-based number (for display)
    char name[64];           // Workspace name from _NET_DESKTOP_NAMES
    gboolean is_current;     // Is this where the window currently is?
    gboolean is_user_current; // Is this where the user currently is?
} DialogWorkspaceInfo;

typedef struct {
    GtkWidget *window;
    GtkWidget *content_box;
    GtkWidget *grid;              // For grid layout
    GtkWidget *vbox;              // For linear layout
    GtkTextBuffer *textbuffer;    // For linear layout text display
    
    WindowInfo *target_window;    // Window to move
    Display *display;
    struct AppData *app_data;     // Reference to main app data
    
    int workspaces_per_row;
    DialogWorkspaceInfo workspaces[36]; // Support up to 36 workspaces
    int workspace_count;
    int current_workspace_idx;    // 0-based index of window's current workspace
    gboolean workspace_selected;  // Flag to indicate workspace was selected
} WorkspaceDialog;

// Forward declaration
struct AppData;

// Show the workspace move dialog
void show_workspace_move_dialog(struct AppData *app);

// Show the workspace jump dialog (no window operations)
void show_workspace_jump_dialog(struct AppData *app);

// Move window to specific desktop (0-based index)
void move_window_to_desktop(Display *display, Window window, int desktop_index);

#endif // WORKSPACE_DIALOG_H