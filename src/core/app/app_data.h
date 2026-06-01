#ifndef APP_DATA_H
#define APP_DATA_H

#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <stdint.h>
#include "x11/window_info.h"
#include "x11/workspace_info.h"
#include "harpoon/harpoon.h"
#include "config/config.h"
#include "x11/atom_cache.h"
#include "matching/match_entry.h"
#include "names/names_store.h"
#include "harpoon/workspace_slots.h"
#include "ui/slot_overlay.h"
#include "ui/window_highlight.h"
#include "geom/layout_store.h"
#include "daemon/hotkey_config.h"
#include "daemon/hotkeys.h"
#include "rules/rules_config.h"
#include "rules/rules.h"
#include "apps/apps.h"
#include "sinks/sinks.h"
#include "proc/proc.h"
#include "projects/projects.h"
#include "daemon/daemon_socket.h"
#include "calc/calc.h"
#include "sessions/sessions.h"
#include "emoji/emoji_data.h"

#define MAX_PROVIDER_ENABLEMENT_ROWS 32

typedef enum {
    TAB_WINDOWS,
    TAB_COUNT
} TabMode;

#define COFI_MAX_TAB_HANDLES 128

typedef enum {
    TAB_VIS_PINNED,
    TAB_VIS_SURFACED,
    TAB_VIS_HIDDEN
} TabVisibility;

// Overlay types for dialog management
typedef enum {
    OVERLAY_NONE,
    OVERLAY_TILING,
    OVERLAY_WORKSPACE_MOVE,
    OVERLAY_WORKSPACE_JUMP,
    OVERLAY_WORKSPACE_RENAME,
    OVERLAY_WORKSPACE_MOVE_ALL,
    OVERLAY_CONFIRM,
    OVERLAY_NAME_ASSIGN,
    OVERLAY_NAME_EDIT,
    OVERLAY_MATCH_PATTERN_EDIT,
    OVERLAY_CONFIG_EDIT,
    OVERLAY_HOTKEY_ADD,
    OVERLAY_HOTKEY_EDIT,
    OVERLAY_HOTKEY_REBIND,
    OVERLAY_RULE_ADD,
    OVERLAY_RULE_EDIT,
    OVERLAY_PROJECT_KILL,
    OVERLAY_PROJECT_RENAME,
    OVERLAY_PROJECT_NEW,
    OVERLAY_PROJECT_REMOTE_HOST,
    OVERLAY_SESSION_RENAME,
    OVERLAY_PROVIDER_ENABLEMENT
} OverlayType;

typedef enum {
    PROJECT_DELETE_KILL_SESSION,
    PROJECT_DELETE_FORGET_REMOTE,
    PROJECT_DELETE_REMOVE_FOLDER
} ProjectDeleteAction;

// Entry mode definitions
typedef enum {
    CMD_MODE_NORMAL,    // Regular window switching mode
    CMD_MODE_COMMAND,   // Command entry mode (after pressing ':')
    CMD_MODE_MODAL      // Modal provider tab (after pressing a provider prefix char, e.g. '=' or '!')
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
    const char *candidates[16];     // Candidate verbs/aliases for prefix strip (static strings)
    int candidate_count;             // Number of visible candidates (0 hides strip)
    int candidate_highlight;         // Highlighted candidate index
    gboolean close_on_exit;         // True when window should close after exiting command mode
} CommandMode;

#define RUN_HISTORY_CAP 32

typedef struct {
    char history[RUN_HISTORY_CAP][256]; // Session-only run history, newest-first (index 0 = newest)
    int history_count;              // Number of run commands in history
    int history_index;              // Current position in history (-1 = not browsing)
    gboolean close_on_exit;         // True when window should close after exiting run mode
    gboolean suppress_entry_change; // Guard while programmatically updating the entry text
} RunMode;

typedef enum { APPS_MODE_DEFAULT, APPS_MODE_PATH } AppsMode;

#define PROVIDER_ID_MAX SESSION_PATH_LEN

// Selection management structure
typedef struct {
    int window_index;                       // Selected index in filtered windows array
    Window selected_window_id;              // ID of currently selected window (for persistence)
    char selected_provider_id[PROVIDER_ID_MAX]; // Selected provider row identity (for persistence)

    int provider_index;                     // Selected index for any registered provider tab
    int sinks_index;                        // Selected index in sinks tab
    int proc_index;                         // Selected index in proc tab

    // Scroll state for each tab
    int window_scroll_offset;               // First visible item index for windows tab
    int provider_scroll_offset;            // First visible item index for any registered provider tab
    int sinks_scroll_offset;               // First visible item index for sinks tab
    int proc_scroll_offset;                // First visible item index for proc tab
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
    TabVisibility tab_visibility[COFI_MAX_TAB_HANDLES]; // Tab visibility state by tab handle

    // Harpoon tab data
    HarpoonSlot filtered_harpoon[MAX_HARPOON_SLOTS];
    int filtered_harpoon_indices[MAX_HARPOON_SLOTS];  // Actual slot indices for filtered items
    int filtered_harpoon_count;

    // Matching tab data
    MatchEntry filtered_matching[MAX_WINDOWS];
    int filtered_matching_count;

    // Names tab data
    NameRecord filtered_names[MAX_WINDOWS];
    int filtered_names_indices[MAX_WINDOWS];
    int filtered_names_count;

    // Layouts tab data
    int filtered_geom[MAX_WINDOWS];    // Indices into layouts.records
    int filtered_geom_count;

    // Config tab data
    ConfigEntry filtered_config[MAX_CONFIG_ENTRIES];
    int filtered_config_count;

    // Hotkeys tab data
    HotkeyBinding filtered_hotkeys[MAX_HOTKEY_BINDINGS];
    int filtered_hotkeys_indices[MAX_HOTKEY_BINDINGS];  // Actual indices in hotkey_config.bindings
    int filtered_hotkeys_count;

    // Rules tab data
    Rule filtered_rules[MAX_RULES];
    int filtered_rule_indices[MAX_RULES];
    int filtered_rules_count;

    // Apps tab data
    AppEntry filtered_apps[MAX_APPS];
    int filtered_apps_count;
    int filtered_emoji[EMOJI_COUNT];
    int filtered_emoji_count;

    // Sinks tab data
    SinksMode sinks_mode;
    ProcMode proc_mode;
    ProjectsMode projects_mode;
    AppsMode apps_mode;

    struct {
        int target_match_id;
        char context_line[MAX_COMMANDS_LEN];
    } pattern_edit;

    // Shared confirm overlay state
    struct {
        gboolean active;
        char *title;
        char *info;
        void (*on_confirm)(struct AppData *);
    } confirm_overlay;

    // Projects tab overlay state
    struct {
        gboolean pending_kill;
        ProjectDeleteAction action;
        ProjectBackend backend;
        char session_name[MAX_PROJECT_SESSION_NAME_LEN];
        char remote_host[128];
        char remote_cwd[512];
        char folder_path[1024];
        gboolean folder_is_remote;
    } project_kill;

    struct {
        gboolean pending_rename;
        char session_name[MAX_PROJECT_SESSION_NAME_LEN];
    } project_rename;

    struct {
        ProjectBackend backend;
        char session_name[MAX_PROJECT_SESSION_NAME_LEN];
        char start_dir[1024];
    } project_new;

    struct {
        char host[128];
    } project_remote;

    struct {
        char source[16];
        char session_id[SESSION_ID_LEN];
        char path[SESSION_PATH_LEN];
        char current_name[SESSION_NAME_LEN];
    } session_rename;

    // Rebind state (Hotkeys tab Ctrl+B)
    struct {
        gboolean active;           // TRUE while rebind overlay is open
        int target_index;          // Index into hotkey_config.bindings[] of the row being rebound
        char target_key[64];       // Current key of that row (for same-combo no-op check)
        char target_command[256];  // Command to preserve (byte-for-byte)
        gboolean awaiting_confirm; // TRUE when conflict confirm prompt is showing
        int conflict_index;        // Index of the conflicting binding (-1 when none)
        char pending_combo[64];    // New combo captured while awaiting confirm
    } hotkey_rebind;

    Display *display;
    AtomCache atoms;                        // Cached X11 atoms
    HarpoonManager harpoon;                 // Harpoon number assignments
    MatchEntryManager matching;            // Pure window matching identity registry
    NamesStore names;                      // Custom names by match_id
    LayoutStore layouts;                    // Saved window layouts by stable match_id
    CofiConfig config;                      // Unified configuration settings
    WorkspaceSlotManager workspace_slots;   // Per-workspace window slot assignments
    SlotOverlayState slot_overlays;         // Active slot number overlays
    WindowHighlight highlight;              // Active window highlight border
    HotkeyConfig hotkey_config;             // User-defined hotkey bindings
    HotkeyGrabState hotkey_grab_state;      // Runtime XGrabKey registration state
    RulesConfig rules_config;               // Window title rules
    RuleState rule_state;                   // Per-rule per-window match state
    RuleBreakerState rule_breaker;          // Per-(rule,window) fire-rate circuit breaker
    gboolean in_rule_dispatch;              // Guard: set while a rule command is executing
    CommandMode command_mode;               // Command mode state
    RunMode run_mode;                       // Run mode state
    CalcMode calc_mode;                     // Calculator tab state
    gboolean suppress_entry_change;         // Modal-shared guard while programmatically updating entry text
    int start_in_command_mode;              // Whether to start in command mode (--command delegate)
    int start_in_run_mode;                  // Whether to start in run mode (--run delegate)
    int assign_slots_and_exit;              // Whether to assign workspace slots and exit (--assign-slots flag)
    uint8_t startup_delegate_opcode;        // Startup delegate opcode requested by CLI (0 = none)
    char startup_delegate_tab_name[64];     // Optional delegate payload for --show NAME

    int daemon_socket_fd;                   // Listening unix socket fd (-1 when inactive)
    char daemon_socket_path[COFI_SOCKET_PATH_MAX]; // Bound unix socket path
    GIOChannel *daemon_socket_channel;      // GLib channel for daemon socket watcher
    guint daemon_socket_watch_id;           // GLib watch id for daemon socket
    guint provider_tick_timer_id;           // Generic provider tick source
    TabMode provider_tick_tab;              // Tab owning provider_tick_timer_id

    // Overlay state management
    gboolean overlay_active;                // Whether any overlay is currently shown
    OverlayType current_overlay;            // Which overlay is currently active
    gboolean hotkey_capture_active;         // True while hotkey add overlay is in capture mode
    struct {
        int selected;
        int count;
        int provider_ids[MAX_PROVIDER_ENABLEMENT_ROWS];
        int enabled[MAX_PROVIDER_ENABLEMENT_ROWS];
    } provider_enablement;
    
    // Window visibility state
    gboolean window_visible;                // Whether the window is currently visible
#ifdef COFI_DEBUG_PRINTSCR_CAPTURE
    gint64 debug_printscr_keep_visible_until_us; // Suppress focus reset/close after PrintScr
#endif

    // Fixed window sizing authority (TFD-100)
    gint fixed_cols;                        // Fixed text columns once initialized (0 = not initialized)
    gint fixed_rows;                        // Fixed visible text rows once initialized (0 = not initialized)
    gboolean fixed_window_size_initializing; // Guard flag while initial resize is being applied
    gulong fixed_size_allocate_handler_id;  // One-shot size-allocate handler id for textview
    gboolean pending_initial_render;        // True when first render is waiting for fixed sizing
    
    // Timer management for deferred operations
    guint focus_loss_timer;                 // Timer ID for focus loss delay
    guint focus_grab_timer;                 // Timer ID for focus grab delay
    guint initial_overlay_idle_id;          // Idle source for initial per-workspace slot overlays
    guint32 focus_timestamp;               // X11 event time for focus requests (0 = CurrentTime)
    int pending_hotkey_mode;               // ShowMode to dispatch on next idle (-1 = none)
    
    // Move-all-to-workspace state
    Window windows_to_move[MAX_WINDOWS];    // Windows to move for move-all command
    int windows_to_move_count;              // Number of windows to move

    // Repeat last action (windows tab, session-only)
    char last_windows_query[256];           // Query from last successful windows-tab activation
    gboolean last_windows_query_valid;      // Whether a repeatable action has been stored

    // Prefix-driven tab claim state
    TabMode prefix_origin_tab;              // Tab active before the current prefix claim
    char active_prefix_claim;               // Active leading prefix claim ('\0' when none)
} AppData;

#define APPDATA_TYPEDEF_DEFINED

#endif // APP_DATA_H
