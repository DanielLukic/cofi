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

// Maximum number of lines to display (to fit in 3/4 screen height)
#define MAX_DISPLAY_LINES 20

// Filter scoring constants
#define SCORE_INITIALS_MATCH 1900

// Desktop indicator
#define DESKTOP_STICKY_INDICATOR "[S] "
#define DESKTOP_FORMAT "[%d] "

// Selection indicator
#define SELECTION_INDICATOR "> "
#define NO_SELECTION_INDICATOR "  "

// Font settings
#define DEFAULT_FONT "DejaVu Sans Mono 15"

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