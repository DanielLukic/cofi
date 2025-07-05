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

// Command mode definitions
typedef enum {
    CMD_MODE_NORMAL,    // Regular window switching mode
    CMD_MODE_COMMAND    // Command entry mode (after pressing ':')
} CommandModeState;

typedef struct {
    CommandModeState state;
    char command_buffer[256];
    int cursor_pos;
    gboolean showing_help;          // True when help is being displayed
    char history[10][256];          // Command history (last 10 commands)
    int history_count;              // Number of commands in history
    int history_index;              // Current position in history (-1 = not browsing)
} CommandMode;

// Selection management structure
typedef struct {
    int window_index;                       // Selected index in filtered windows array
    int workspace_index;                    // Selected index in filtered workspaces array
    Window selected_window_id;              // ID of currently selected window (for persistence)
    int selected_workspace_id;              // ID of currently selected workspace (for persistence)
} SelectionState;

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
    SelectionState selection;               // Centralized selection management
    int active_window_id;                   // Currently active window
    Window own_window_id;                   // Our own window ID for filtering
    WindowAlignment alignment;              // Window alignment setting
    int has_saved_position;                 // Whether we have a saved position
    int saved_x;                            // Saved window X position
    int saved_y;                            // Saved window Y position

    WorkspaceInfo workspaces[MAX_WORKSPACES]; // Workspace list
    WorkspaceInfo filtered_workspaces[MAX_WORKSPACES]; // Filtered workspaces
    int workspace_count;
    int filtered_workspace_count;
    TabMode current_tab;                    // Current active tab

    Display *display;
    HarpoonManager harpoon;                 // Harpoon number assignments
    int close_on_focus_loss;                // Whether to close window when focus is lost
    int workspaces_per_row;                 // Number of workspaces per row in grid layout (0 = linear)
    int dialog_active;                      // Whether a dialog is currently active
    CommandMode command_mode;               // Command mode state
    Window last_commanded_window_id;        // Last window affected by command mode action
} AppData;

#define APPDATA_TYPEDEF_DEFINED

#endif // APP_DATA_H