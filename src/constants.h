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
// Added to fzf score when the query appears as a contiguous case-insensitive
// run starting at a word boundary in the composite display string.  The gap
// is wide enough that no amount of fzf score or bonus in a non-direct match
// can reach the direct-match tier (max realistic indirect score << 10000).
#define TIER_DIRECT_BASE 10000

// Signal A: maximum bonus awarded to a TIER_DIRECT match that lands at the
// very start of the title field (title-relative offset 0).  Decays linearly
// to 0 as the match moves toward the end of the title.  Fixes the "ch →
// Chat" case where every Chrome window is TIER_DIRECT via "Google Chrome"
// boilerplate, while "Chat" lands at the title start and Chrome lands ~30
// chars in.
#define MATCH_EARLY_BONUS_MAX 50

// Signal B: bonus per adjacent pair of query characters where both are
// matched at word-start positions with no other word-start between them.
// Rewards "gcse" matching g(oogle-chrome)·c(hrome)·S(oftware)·e(ngineering)
// (two consecutive pairs) over scattered word-start matches in other windows.
// Must be > 6 so that 2-pair windows beat 1-pair windows that have ≤6 higher
// base fzf score.
#define CONSECUTIVE_WORD_START_PAIR_BONUS 8

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