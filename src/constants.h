#ifndef CONSTANTS_H
#define CONSTANTS_H

// Harpoon slot ranges
#define HARPOON_FIRST_NUMBER 0
#define HARPOON_LAST_NUMBER 9
#define HARPOON_FIRST_LETTER 10  // 'a'
#define HARPOON_LAST_LETTER 35   // 'z'

// Display column widths
#define DISPLAY_HARPOON_WIDTH 1
#define DISPLAY_DESKTOP_WIDTH 4
#define DISPLAY_INSTANCE_WIDTH 20
#define DISPLAY_TITLE_WIDTH 55
#define DISPLAY_CLASS_WIDTH 18

// Filter scoring constants
#define SCORE_WORD_BOUNDARY 2000
#define SCORE_INITIALS_MATCH 1900
#define SCORE_SUBSEQUENCE_MATCH 1500
#define SCORE_CLASS_INSTANCE_MATCH 1400
#define SCORE_FUZZY_BASE 1000
#define SCORE_FUZZY_CONSECUTIVE_BONUS 10
#define SCORE_FUZZY_CASE_BONUS 10
#define SCORE_FUZZY_POSITION_PENALTY 1

// Window type constants
#define WINDOW_TYPE_NORMAL "Normal"
#define WINDOW_TYPE_SPECIAL "Special"

// Desktop indicator
#define DESKTOP_STICKY_INDICATOR "[S] "
#define DESKTOP_FORMAT "[%d] "

// Selection indicator
#define SELECTION_INDICATOR "> "
#define NO_SELECTION_INDICATOR "  "

// Font settings
#define DEFAULT_FONT "DejaVu Sans Mono 15"

// Log file settings
#define LOG_FILE_NAME "cofi.log"
#define LOG_FILE_MAX_SIZE 10485760  // 10MB

// Instance lock file
#define INSTANCE_LOCK_FILE "/tmp/cofi.lock"

// Config directory and file
#define CONFIG_DIR_NAME ".config"
#define CONFIG_FILE_NAME "cofi.json"

// Window list update delay (milliseconds)
#define WINDOW_LIST_UPDATE_DELAY_MS 100

// Keyboard repeat settings
#define KEY_REPEAT_INITIAL_DELAY 1
#define KEY_REPEAT_INTERVAL 100

// Error codes for standardized error handling
typedef enum {
    COFI_SUCCESS = 0,           // Operation succeeded
    COFI_ERROR = -1,            // Generic error
    COFI_ERROR_X11 = -2,        // X11 operation failed
    COFI_ERROR_MEMORY = -3,     // Memory allocation failed
    COFI_ERROR_FILE = -4,       // File operation failed
    COFI_ERROR_INVALID = -5,    // Invalid parameter or state
    COFI_ERROR_NOT_FOUND = -6   // Resource not found
} CofiResult;

#endif // CONSTANTS_H