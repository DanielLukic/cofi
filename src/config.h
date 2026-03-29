#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

// Forward declaration for WindowAlignment (defined in app_data.h)
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

// Digit slot modes for Alt+digit behavior
typedef enum {
    DIGIT_MODE_DEFAULT,       // Global harpoon slots (existing behavior)
    DIGIT_MODE_PER_WORKSPACE, // Workspace-scoped window slots by position
    DIGIT_MODE_WORKSPACES     // Switch to workspace N
} DigitSlotMode;

// Config entry for display and editing (used by Config tab and :show config)
#define MAX_CONFIG_ENTRIES 32
#define CONFIG_KEY_LEN 64
#define CONFIG_VALUE_LEN 128

typedef enum {
    CONFIG_TYPE_BOOL,
    CONFIG_TYPE_INT,
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_ENUM
} ConfigFieldType;

typedef struct {
    char key[CONFIG_KEY_LEN];
    char value[CONFIG_VALUE_LEN];
    ConfigFieldType type;
} ConfigEntry;

// Sort order for per-workspace slot assignment
typedef enum {
    SLOT_SORT_ROW_FIRST,    // Top-to-bottom rows, left-to-right within row (default)
    SLOT_SORT_COLUMN_FIRST  // Left-to-right columns, top-to-bottom within column
} SlotSortOrder;


// Unified configuration structure
typedef struct {
    int close_on_focus_loss;        // Whether to close window when focus is lost
    WindowAlignment alignment;      // Window alignment setting
    int workspaces_per_row;        // Number of workspaces per row in grid layout (0 = linear)
    int tile_columns;              // Number of columns for tiling grid (2 or 3, default 3)
    DigitSlotMode digit_slot_mode; // What Alt+digit does
    int slot_overlay_duration_ms;  // Duration of slot number overlays (0 = disabled)
    int ripple_enabled;            // Whether to show ripple effect on window activation (1=on, 0=off)
    SlotSortOrder slot_sort_order; // How to number per-workspace slots: row-first or column-first
    char hotkey_windows[64];       // Hotkey for windows mode, e.g. "Mod1+Tab" ("" = disabled)
    char hotkey_command[64];       // Hotkey for command mode, e.g. "Mod1+grave"
    char hotkey_workspaces[64];    // Hotkey for workspaces mode, e.g. "Mod1+BackSpace"
} CofiConfig;

// Alignment string conversion
const char* alignment_to_string(WindowAlignment align);

// Digit slot mode string conversion
const char* digit_slot_mode_to_string(DigitSlotMode mode);
DigitSlotMode string_to_digit_slot_mode(const char *str);

// Slot sort order string conversion
const char* slot_sort_order_to_string(SlotSortOrder order);
SlotSortOrder string_to_slot_sort_order(const char *str);

// Configuration management functions (options only - harpoon slots handled separately)
void save_config(const CofiConfig *config);
void load_config(CofiConfig *config);

// Initialize config with default values
void init_config_defaults(CofiConfig *config);

// Apply a single config setting by key/value. Returns 1 on success, 0 on error.
int apply_config_setting(CofiConfig *config, const char *key, const char *value,
                         char *err_buf, size_t err_size);

// Cycle to the next valid value for an enum config key. Returns NULL if not an enum key.
const char* get_next_enum_value(const char *key, const char *current_value);

// Build the canonical list of all config entries. Single source of truth.
void build_config_entries(const CofiConfig *config, ConfigEntry *entries, int *count);

// Format all config settings as display text. Returns bytes written.
int format_config_display(const CofiConfig *config, char *buf, size_t buf_size);

/* Scoring constants from fzy (moved to bottom to maintain compatibility) */
#define SCORE_MATCH_CONSECUTIVE 16
#define SCORE_MATCH_SLASH 9
#define SCORE_MATCH_WORD 8
#define SCORE_MATCH_CAPITAL 7
#define SCORE_MATCH_DOT 6

#define SCORE_GAP_LEADING -9
#define SCORE_GAP_TRAILING -10
#define SCORE_GAP_INNER -11

#endif /* CONFIG_H */