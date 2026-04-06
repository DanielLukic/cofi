#ifndef APP_DATA_H
#define APP_DATA_H

#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include "window_info.h"
#include "workspace_info.h"
#include "harpoon.h"
#include "config.h"
#include "atom_cache.h"
#include "named_window.h"
#include "workspace_slots.h"
#include "slot_overlay.h"
#include "window_highlight.h"
#include "hotkey_config.h"
#include "rules_config.h"
#include "rules.h"


typedef enum {
    TAB_WINDOWS,
    TAB_WORKSPACES,
    TAB_HARPOON,
    TAB_NAMES,
    TAB_CONFIG,
    TAB_HOTKEYS
} TabMode;

// Overlay types for dialog management
typedef enum {
    OVERLAY_NONE,
    OVERLAY_TILING,
    OVERLAY_WORKSPACE_MOVE,
    OVERLAY_WORKSPACE_JUMP,
    OVERLAY_WORKSPACE_RENAME,
    OVERLAY_WORKSPACE_MOVE_ALL,
    OVERLAY_HARPOON_DELETE,
    OVERLAY_HARPOON_EDIT,
    OVERLAY_NAME_ASSIGN,
    OVERLAY_NAME_EDIT,
    OVERLAY_CONFIG_EDIT,
    OVERLAY_HOTKEY_ADD,
    OVERLAY_HOTKEY_EDIT
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
    int help_scroll_offset;         // First visible help line when help is paged
    char history[10][256];          // Command history (last 10 commands)
    int history_count;              // Number of commands in history
    int history_index;              // Current position in history (-1 = not browsing)
    gboolean close_on_exit;         // True when window should close after exiting command mode
} CommandMode;

// Selection management structure
typedef struct {
    int window_index;                       // Selected index in filtered windows array
    int workspace_index;                    // Selected index in filtered workspaces array
    int harpoon_index;                      // Selected index in harpoon tab
    int names_index;                        // Selected index in names tab
    Window selected_window_id;              // ID of currently selected window (for persistence)
    int selected_workspace_id;              // ID of currently selected workspace (for persistence)

    int config_index;                       // Selected index in config tab
    int hotkeys_index;                      // Selected index in hotkeys tab

    // Scroll state for each tab
    int window_scroll_offset;               // First visible item index for windows tab
    int workspace_scroll_offset;            // First visible item index for workspaces tab
    int harpoon_scroll_offset;              // First visible item index for harpoon tab
    int names_scroll_offset;                // First visible item index for names tab
    int config_scroll_offset;               // First visible item index for config tab
    int hotkeys_scroll_offset;              // First visible item index for hotkeys tab
} SelectionState;

typedef struct AppData {
    GtkWidget *window;
    GtkWidget *entry;
    GtkWidget *mode_indicator;      // Label showing ">" or ":" for mode
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
    int filtered_harpoon_indices[MAX_HARPOON_SLOTS];  // Actual slot indices for filtered items
    int filtered_harpoon_count;

    // Names tab data
    NamedWindow filtered_names[MAX_WINDOWS];
    int filtered_names_count;

    // Config tab data
    ConfigEntry filtered_config[MAX_CONFIG_ENTRIES];
    int filtered_config_count;

    // Hotkeys tab data
    HotkeyBinding filtered_hotkeys[MAX_HOTKEY_BINDINGS];
    int filtered_hotkeys_indices[MAX_HOTKEY_BINDINGS];  // Actual indices in hotkey_config.bindings
    int filtered_hotkeys_count;

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
    AtomCache atoms;                        // Cached X11 atoms
    HarpoonManager harpoon;                 // Harpoon number assignments
    NamedWindowManager names;               // Custom window names
    CofiConfig config;                      // Unified configuration settings
    WorkspaceSlotManager workspace_slots;   // Per-workspace window slot assignments
    SlotOverlayState slot_overlays;         // Active slot number overlays
    WindowHighlight highlight;              // Active window highlight border
    HotkeyConfig hotkey_config;             // User-defined hotkey bindings
    RulesConfig rules_config;               // Window title rules
    RuleState rule_state;                   // Per-rule per-window match state
    CommandMode command_mode;               // Command mode state
    int start_in_command_mode;              // Whether to start in command mode (--command flag)
    int assign_slots_and_exit;              // Whether to assign workspace slots and exit (--assign-slots flag)
    int no_daemon;                          // Whether to run in no-daemon mode (--no-daemon flag)

    // Overlay state management
    gboolean overlay_active;                // Whether any overlay is currently shown
    OverlayType current_overlay;            // Which overlay is currently active
    gboolean hotkey_capture_active;         // True while hotkey add overlay is in capture mode
    
    // Window visibility state
    gboolean window_visible;                // Whether the window is currently visible
    
    // Timer management for deferred operations
    guint focus_loss_timer;                 // Timer ID for focus loss delay
    guint focus_grab_timer;                 // Timer ID for focus grab delay
    guint32 focus_timestamp;               // X11 event time for focus requests (0 = CurrentTime)
    int pending_hotkey_mode;               // ShowMode to dispatch on next idle (-1 = none)
    
    // Move-all-to-workspace state
    Window windows_to_move[MAX_WINDOWS];    // Windows to move for move-all command
    int windows_to_move_count;              // Number of windows to move
} AppData;

#define APPDATA_TYPEDEF_DEFINED

#endif // APP_DATA_H
