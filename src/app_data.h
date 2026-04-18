#ifndef APP_DATA_H
#define APP_DATA_H

#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <stdint.h>
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
#include "hotkeys.h"
#include "rules_config.h"
#include "rules.h"
#include "apps.h"
#include "daemon_socket.h"


typedef enum {
    TAB_WINDOWS,
    TAB_WORKSPACES,
    TAB_HARPOON,
    TAB_NAMES,
    TAB_CONFIG,
    TAB_HOTKEYS,
    TAB_APPS
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

// Entry mode definitions
typedef enum {
    CMD_MODE_NORMAL,    // Regular window switching mode
    CMD_MODE_COMMAND,   // Command entry mode (after pressing ':')
    CMD_MODE_RUN        // Run entry mode (after pressing '!')
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

typedef struct {
    char history[10][256];          // Session-only run history (last 10 commands)
    int history_count;              // Number of run commands in history
    int history_index;              // Current position in history (-1 = not browsing)
    gboolean close_on_exit;         // True when window should close after exiting run mode
    gboolean suppress_entry_change; // Guard while programmatically updating the entry text
} RunMode;

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
    int apps_index;                         // Selected index in apps tab

    // Scroll state for each tab
    int window_scroll_offset;               // First visible item index for windows tab
    int workspace_scroll_offset;            // First visible item index for workspaces tab
    int harpoon_scroll_offset;              // First visible item index for harpoon tab
    int names_scroll_offset;               // First visible item index for names tab
    int config_scroll_offset;              // First visible item index for config tab
    int hotkeys_scroll_offset;             // First visible item index for hotkeys tab
    int apps_scroll_offset;                // First visible item index for apps tab
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
    Window command_target_id;              // Pre-focus active window for command mode targeting

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

    // Apps tab data
    AppEntry filtered_apps[MAX_APPS];
    int filtered_apps_count;

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
    HotkeyGrabState hotkey_grab_state;      // Runtime XGrabKey registration state
    RulesConfig rules_config;               // Window title rules
    RuleState rule_state;                   // Per-rule per-window match state
    CommandMode command_mode;               // Command mode state
    RunMode run_mode;                       // Run mode state
    int start_in_command_mode;              // Whether to start in command mode (--command delegate)
    int start_in_run_mode;                  // Whether to start in run mode (--run delegate)
    int assign_slots_and_exit;              // Whether to assign workspace slots and exit (--assign-slots flag)
    uint8_t startup_delegate_opcode;        // Startup delegate opcode requested by CLI (0 = none)

    int daemon_socket_fd;                   // Listening unix socket fd (-1 when inactive)
    char daemon_socket_path[COFI_SOCKET_PATH_MAX]; // Bound unix socket path
    GIOChannel *daemon_socket_channel;      // GLib channel for daemon socket watcher
    guint daemon_socket_watch_id;           // GLib watch id for daemon socket

    // Overlay state management
    gboolean overlay_active;                // Whether any overlay is currently shown
    OverlayType current_overlay;            // Which overlay is currently active
    gboolean hotkey_capture_active;         // True while hotkey add overlay is in capture mode
    
    // Window visibility state
    gboolean window_visible;                // Whether the window is currently visible

    // Fixed window sizing authority (TFD-100)
    gint fixed_cols;                        // Fixed text columns once initialized (0 = not initialized)
    gint fixed_rows;                        // Fixed visible text rows once initialized (0 = not initialized)
    gboolean fixed_window_size_initializing; // Guard flag while initial resize is being applied
    gulong fixed_size_allocate_handler_id;  // One-shot size-allocate handler id for textview
    gboolean pending_initial_render;        // True when first render is waiting for fixed sizing
    
    // Timer management for deferred operations
    guint focus_loss_timer;                 // Timer ID for focus loss delay
    guint focus_grab_timer;                 // Timer ID for focus grab delay
    guint32 focus_timestamp;               // X11 event time for focus requests (0 = CurrentTime)
    int pending_hotkey_mode;               // ShowMode to dispatch on next idle (-1 = none)
    
    // Move-all-to-workspace state
    Window windows_to_move[MAX_WINDOWS];    // Windows to move for move-all command
    int windows_to_move_count;              // Number of windows to move

    // Repeat last action (windows tab, session-only)
    char last_windows_query[256];           // Query from last successful windows-tab activation
    gboolean last_windows_query_valid;      // Whether a repeatable action has been stored
} AppData;

#define APPDATA_TYPEDEF_DEFINED

#endif // APP_DATA_H
