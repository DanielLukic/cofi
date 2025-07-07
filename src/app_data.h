#ifndef APP_DATA_H
#define APP_DATA_H

#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include "window_info.h"
#include "workspace_info.h"
#include "harpoon.h"
#include "config.h"

typedef enum {
    TAB_WINDOWS,
    TAB_WORKSPACES,
    TAB_HARPOON
} TabMode;

// Overlay types for dialog management
typedef enum {
    OVERLAY_NONE,
    OVERLAY_TILING,
    OVERLAY_WORKSPACE_MOVE,
    OVERLAY_WORKSPACE_JUMP
} OverlayType;

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
    int harpoon_index;                      // Selected index in harpoon tab
    Window selected_window_id;              // ID of currently selected window (for persistence)
    int selected_workspace_id;              // ID of currently selected workspace (for persistence)
} SelectionState;

typedef struct AppData {
    GtkWidget *window;
    GtkWidget *entry;
    GtkWidget *textview;
    GtkWidget *scrolled;
    GtkTextBuffer *textbuffer;

    // Overlay system components
    GtkWidget *main_overlay;        // Root GtkOverlay container
    GtkWidget *main_content;        // Main content container (existing vbox)
    GtkWidget *modal_background;    // Semi-transparent modal overlay
    GtkWidget *dialog_container;    // Container for dialog content

    WindowInfo windows[MAX_WINDOWS];        // Raw window list from X11
    WindowInfo history[MAX_WINDOWS];        // History-ordered windows
    WindowInfo filtered[MAX_WINDOWS];       // Filtered and display-ready windows
    int window_count;
    int history_count;
    int filtered_count;
    SelectionState selection;               // Centralized selection management
    int active_window_id;                   // Currently active window
    Window own_window_id;                   // Our own window ID for filtering

    WorkspaceInfo workspaces[MAX_WORKSPACES]; // Workspace list
    WorkspaceInfo filtered_workspaces[MAX_WORKSPACES]; // Filtered workspaces
    int workspace_count;
    int filtered_workspace_count;
    TabMode current_tab;                    // Current active tab

    // Harpoon tab data
    HarpoonSlot filtered_harpoon[MAX_HARPOON_SLOTS];
    int filtered_harpoon_count;

    // Edit state for harpoon
    struct {
        gboolean editing;
        int editing_slot;
        char edit_buffer[MAX_TITLE_LEN];
    } harpoon_edit;

    // Delete confirmation state
    struct {
        gboolean pending_delete;
        int delete_slot;
    } harpoon_delete;

    Display *display;
    HarpoonManager harpoon;                 // Harpoon number assignments
    CofiConfig config;                      // Unified configuration settings
    CommandMode command_mode;               // Command mode state
    Window last_commanded_window_id;        // Last window affected by command mode action
    int start_in_command_mode;              // Whether to start in command mode (--command flag)

    // Overlay state management
    gboolean overlay_active;                // Whether any overlay is currently shown
    OverlayType current_overlay;            // Which overlay is currently active
} AppData;

#define APPDATA_TYPEDEF_DEFINED

#endif // APP_DATA_H