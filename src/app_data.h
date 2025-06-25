#ifndef APP_DATA_H
#define APP_DATA_H

#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include "window_info.h"
#include "workspace_info.h"
#include "harpoon.h"

// Forward declaration for WindowAlignment
typedef enum {
    ALIGN_CENTER,
    ALIGN_TOP,
    ALIGN_TOP_LEFT,
    ALIGN_TOP_RIGHT,
    ALIGN_LEFT,
    ALIGN_RIGHT,
    ALIGN_BOTTOM,
    ALIGN_BOTTOM_LEFT,
    ALIGN_BOTTOM_RIGHT
} WindowAlignment;

typedef enum {
    TAB_WINDOWS,
    TAB_WORKSPACES
} TabMode;

typedef struct {
    GtkWidget *window;
    GtkWidget *entry;
    GtkWidget *textview;
    GtkWidget *scrolled;
    GtkTextBuffer *textbuffer;
    
    WindowInfo windows[MAX_WINDOWS];        // Raw window list from X11
    WindowInfo history[MAX_WINDOWS];        // History-ordered windows
    WindowInfo filtered[MAX_WINDOWS];       // Filtered and display-ready windows
    int window_count;
    int history_count;
    int filtered_count;
    int selected_index;
    int active_window_id;                   // Currently active window
    Window own_window_id;                   // Our own window ID for filtering
    WindowAlignment alignment;              // Window alignment setting
    
    WorkspaceInfo workspaces[MAX_WORKSPACES]; // Workspace list
    WorkspaceInfo filtered_workspaces[MAX_WORKSPACES]; // Filtered workspaces
    int workspace_count;
    int filtered_workspace_count;
    int selected_workspace_index;
    TabMode current_tab;                    // Current active tab
    
    Display *display;
    HarpoonManager harpoon;                 // Harpoon number assignments
} AppData;

#endif // APP_DATA_H