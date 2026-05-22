#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include "app_data.h"
#include "filter.h"
#include "window_info.h"
#include "history.h"
#include "log.h"
#include <strings.h>

#include "match.h"
#include "fzf_algo.h"
#include "constants.h"
#include "selection.h"
#include "x11_utils.h"
#include "named_window.h"
#include <X11/Xatom.h>

#define UNUSED __attribute__((unused))

// Structure to hold window info with match score
typedef struct {
    WindowInfo window;
    score_t score;
} ScoredWindow;

// Comparison function for qsort
static int compare_scores(const void *a, const void *b) {
    const ScoredWindow *wa = (const ScoredWindow *)a;
    const ScoredWindow *wb = (const ScoredWindow *)b;
    // For fzf-style scoring, HIGHER scores are better (less negative)
    // -90 is better than -500
    if (wa->score > wb->score) return -1;
    if (wa->score < wb->score) return 1;
    return 0;
}

// Compose the full display string for a window (same content the user sees)
static void compose_display_string(const WindowInfo *win, char *out, size_t out_size) {
    char desktop_str[8];
    if (win->desktop < 0 || win->desktop > 99) {
        strcpy(desktop_str, "[S]");
    } else {
        snprintf(desktop_str, sizeof(desktop_str), "[%d]", win->desktop + 1);
    }

    // Apply same instance/class swap as display.c
    const char *display_instance = win->instance;
    const char *display_class = win->class_name;
    if (win->instance[0] >= 'A' && win->instance[0] <= 'Z') {
        display_instance = win->class_name;
        display_class = win->instance;
    }

    snprintf(out, out_size, "%s %s %s %s",
             desktop_str, display_instance, win->title, display_class);
}

// Return byte offset in 'display' where the TITLE field starts.
// Format: "[N] instance title class" — title is after "[N] instance ".
static int title_start_offset(const WindowInfo *win) {
    int desktop_len = (win->desktop < 0 || win->desktop > 99) ? 3 :
                      (win->desktop + 1 < 10) ? 3 : 4;
    const char *inst = (win->instance[0] >= 'A' && win->instance[0] <= 'Z')
                       ? win->class_name : win->instance;
    return desktop_len + 1 + (int)strlen(inst) + 1;
}

// Signal A: title-relative position bonus for TIER_DIRECT matches.
// Finds the earliest word-boundary contiguous match of filter within the
// title portion of display (starting at title_start) and returns
// max(0, MATCH_EARLY_BONUS_MAX - title_relative_offset).
// Returns 0 if no such match exists in the title portion.
static score_t early_title_bonus(const char *filter, const char *display,
                                  int title_start) {
    int flen = (int)strlen(filter);
    if (flen == 0) return 0;
    for (int i = title_start; display[i]; i++) {
        bool at_boundary = (i == title_start) ||
            (display[i-1] == ' ' || display[i-1] == '-' || display[i-1] == '_' ||
             display[i-1] == '.' || display[i-1] == '(' || display[i-1] == '|' ||
             display[i-1] == '/');
        if (!at_boundary) continue;
        int j = 0;
        while (j < flen && display[i+j] &&
               tolower((unsigned char)display[i+j]) == tolower((unsigned char)filter[j]))
            j++;
        if (j == flen) {
            int offset = i - title_start;
            return (offset < MATCH_EARLY_BONUS_MAX) ? (score_t)(MATCH_EARLY_BONUS_MAX - offset) : 0;
        }
    }
    return 0;
}

// Signal B: count pairs of adjacent query characters where both are matched
// at word-start positions with no other word-start between them.
// Uses a greedy left-to-right word-start scan.  Returns 0 if not all query
// characters can be matched at word-start positions.
static bool is_word_start(const char *s, int pos) {
    if (pos == 0) return true;
    char p = s[pos-1];
    return p == ' ' || p == '-' || p == '_' || p == '.' ||
           p == '(' || p == '|' || p == '/';
}

static int consecutive_word_start_pairs(const char *filter, const char *display) {
    int flen = (int)strlen(filter);
    if (flen < 2) return 0;

    int pos[MAX_TITLE_LEN];
    int cur = 0;
    for (int i = 0; i < flen && i < MAX_TITLE_LEN; i++) {
        bool found = false;
        while (display[cur]) {
            if (is_word_start(display, cur) &&
                tolower((unsigned char)display[cur]) == tolower((unsigned char)filter[i])) {
                pos[i] = cur;
                cur++;
                found = true;
                break;
            }
            cur++;
        }
        if (!found) return 0;
    }

    int pairs = 0;
    for (int i = 0; i < flen - 1 && i < MAX_TITLE_LEN - 1; i++) {
        bool adjacent = true;
        for (int j = pos[i] + 1; j < pos[i+1]; j++) {
            if (is_word_start(display, j)) { adjacent = false; break; }
        }
        if (adjacent) pairs++;
    }
    return pairs;
}

// Returns true when the query appears as a contiguous case-insensitive
// sequence of characters starting at a word-boundary position in display.
// Word-boundary: start of string or previous char is space/-/_/./(/|//.
// This is the TIER_DIRECT gate: a contiguous word-boundary match scores
// fzf + TIER_DIRECT_BASE, placing it in a tier no indirect match can reach.
static bool is_direct_word_boundary_match(const char *filter, const char *display) {
    int flen = strlen(filter);
    if (flen == 0) return false;
    for (int i = 0; display[i]; i++) {
        if (i > 0) {
            char p = display[i-1];
            if (p != ' ' && p != '-' && p != '_' && p != '.' &&
                p != '(' && p != '|' && p != '/') continue;
        }
        int j = 0;
        while (j < flen && display[i+j] &&
               tolower((unsigned char)display[i+j]) == tolower((unsigned char)filter[j]))
            j++;
        if (j == flen) return true;
    }
    return false;
}

// Match a window against filter and return best score.
// Scores are partitioned into two tiers by construction:
//   TIER_DIRECT   fzf + TIER_DIRECT_BASE — query found as a contiguous run
//                 at a word boundary.  Covers "brew" in "brew | dl",
//                 "Chat" in "Chat | ...", "ch" in "chrome" or "Chat".
//   TIER_INDIRECT fzf only              — all other fzf matches (scattered
//                 word-start initials, mid-word fzf, etc.)
// The gap between tiers (TIER_DIRECT_BASE = 10000) is wide enough that no
// fzf score or workspace bonus on an indirect match can reach a direct one.
static score_t match_window(const char *filter, const WindowInfo *win) {
    char display[1024];
    compose_display_string(win, display, sizeof(display));

    if (!fzf_has_match(filter, display)) {
        return SCORE_MIN;
    }

    score_t score = fzf_fuzzy_match(filter, display);

    // Signal B: consecutive word-start pair bonus (applies to all matches).
    // Rewards acronym-style queries where matched chars are at adjacent
    // word-starts (e.g. "gcse" → google·chrome·Software·engineering).
    int pairs = consecutive_word_start_pairs(filter, display);
    if (pairs > 0) {
        score += pairs * CONSECUTIVE_WORD_START_PAIR_BONUS;
        log_debug("WORD_START_PAIRS: '%s' -> '%s' pairs=%d (+%.0f)",
                  filter, display, pairs, (score_t)pairs * CONSECUTIVE_WORD_START_PAIR_BONUS);
    }

    if (is_direct_word_boundary_match(filter, display)) {
        score += TIER_DIRECT_BASE;
        // Signal A: title-relative position bonus (TIER_DIRECT only).
        // Boosts matches that land early in the title over boilerplate
        // "Google Chrome" matches near the end of the title.
        int ts = title_start_offset(win);
        score_t pos_bonus = early_title_bonus(filter, display, ts);
        score += pos_bonus;
        log_debug("TIER_DIRECT: '%s' -> '%s' pos_bonus=%.0f (%.0f)",
                  filter, display, pos_bonus, score);
    } else {
        log_debug("TIER_INDIRECT: '%s' -> '%s' (%.0f)", filter, display, score);
    }

    return score;
}


// Reorder app->history to match _NET_CLIENT_LIST_STACKING (top-of-stack first)
static void apply_native_stacking_order(AppData *app) {
    Atom atom = XInternAtom(app->display, "_NET_CLIENT_LIST_STACKING", False);
    Atom actual_type;
    int actual_format;
    unsigned long n_items, bytes_after;
    unsigned char *prop = NULL;

    if (XGetWindowProperty(app->display, DefaultRootWindow(app->display), atom,
                           0, 4096, False, XA_WINDOW,
                           &actual_type, &actual_format, &n_items, &bytes_after,
                           &prop) != Success || !prop) {
        log_warn("Could not fetch _NET_CLIENT_LIST_STACKING, keeping current order");
        return;
    }

    Window *stack = (Window *)prop;
    WindowInfo new_order[MAX_WINDOWS];
    int new_count = 0;

    // Stack is bottom-to-top; we want top-to-bottom (top = most recently raised first)
    for (long i = (long)n_items - 1; i >= 0 && new_count < MAX_WINDOWS; i--) {
        for (int j = 0; j < app->history_count; j++) {
            if (app->history[j].id == stack[i]) {
                new_order[new_count++] = app->history[j];
                break;
            }
        }
    }

    // Copy any windows not in the stacking list (shouldn't happen, but be safe)
    for (int j = 0; j < app->history_count && new_count < MAX_WINDOWS; j++) {
        int found = 0;
        for (int k = 0; k < new_count; k++) {
            if (new_order[k].id == app->history[j].id) { found = 1; break; }
        }
        if (!found) new_order[new_count++] = app->history[j];
    }

    for (int i = 0; i < new_count; i++) app->history[i] = new_order[i];
    app->history_count = new_count;

    XFree(prop);
    log_debug("Native stacking order applied: %d windows", new_count);
}

// Prepare windows for filtering by updating history and partitioning
static void prepare_windows_for_filtering(AppData *app) {
    log_trace("Before pipeline - history_count=%d", app->history_count);

    // First, update the complete window processing pipeline
    update_history(app);
    if (app->config.window_order_mode == WINDOW_ORDER_NATIVE) {
        apply_native_stacking_order(app);
    } else {
        partition_and_reorder(app);
    }

    // Second, update window titles to include custom names for filtering
    for (int i = 0; i < app->history_count; i++) {
        const char *custom_name = get_window_custom_name(&app->names, app->history[i].id);
        if (custom_name) {
            // Store original title and create modified title for filtering
            char original_title[MAX_TITLE_LEN];
            strncpy(original_title, app->history[i].title, sizeof(original_title) - 1);
            original_title[sizeof(original_title) - 1] = '\0';

            // Format as "custom_name - original_title"
            snprintf(app->history[i].title, sizeof(app->history[i].title), "%s - %s", custom_name, original_title);
        }
    }

    log_trace("After pipeline - history_count=%d", app->history_count);
}

// Score and filter windows based on search text
static int score_and_filter_windows(AppData *app, const char *filter, 
                                   const WindowInfo *windows, int window_count,
                                   ScoredWindow *scored_windows) {
    int scored_count = 0;
    int current_desktop = get_current_desktop(app->display);
    
    // Filter and score windows
    for (int i = 0; i < window_count; i++) {
        const WindowInfo *win = &windows[i];
        
        if (strlen(filter) == 0) {
            // No filter - include all windows with max score
            if (scored_count < MAX_WINDOWS) {
                scored_windows[scored_count].window = *win;
                scored_windows[scored_count].score = 1000; // Max score for no filter
                scored_count++;
            }
        } else {
            // Use the match_window function for all matching logic
            score_t best_score = match_window(filter, win);
            
            // Add workspace bonus if window is on current workspace
            if (best_score > SCORE_MIN && win->desktop == current_desktop && win->desktop != -1) {
                // Workspace is a pure tiebreaker: +1 is enough to prefer the
                // current desktop when two windows have identical scores, but
                // cannot override even a 2-point fzf advantage (e.g. BOUNDARY_WHITE
                // vs BOUNDARY_DELIMITER for the same 2-char query).
                score_t workspace_bonus = 1;
                best_score += workspace_bonus;
                log_debug("Window '%s' on current workspace %d - added bonus %.0f (new score: %.0f)",
                         win->title, current_desktop, workspace_bonus, best_score);
            }
            
            // Add to scored list if we have a match
            if (best_score > SCORE_MIN) {
                if (scored_count < MAX_WINDOWS) {
                    scored_windows[scored_count].window = *win;
                    scored_windows[scored_count].score = best_score;
                    scored_count++;
                    log_debug("Window '%s' matched with final score: %f", win->title, best_score);
                }
            }
        }
    }
    
    return scored_count;
}

// Sort scored windows by score (highest first)
static void sort_scored_windows(ScoredWindow *scored_windows, int count, const char *filter) {
    if (strlen(filter) > 0 && count > 0) {
        qsort(scored_windows, count, sizeof(ScoredWindow), compare_scores);
        
        // Debug: print sorted results
        log_debug("=== Sorted results for filter '%s' ===", filter);
        for (int i = 0; i < count && i < 5; i++) {
            log_debug("%d: %s (score: %f)", i, scored_windows[i].window.title, scored_windows[i].score);
        }
        log_debug("=====================================");
    }
}

// Finalize filter results by copying to app's filtered array
static void finalize_filter_results(AppData *app, const ScoredWindow *scored_windows, int count) {
    app->filtered_count = 0;
    
    // Copy sorted windows to filtered array
    for (int i = 0; i < count && i < MAX_WINDOWS; i++) {
        app->filtered[i] = scored_windows[i].window;
        app->filtered_count++;
    }
    
    // Don't reset selection here - it will be handled by the caller
    // to preserve the selected window by ID
}

// Filter windows based on search text (now works with history-ordered windows)
void filter_windows(AppData *app, const char *filter) {
    log_trace("filter_windows() called with filter='%s'", filter);

    // Preserve current selection
    preserve_selection(app);
    
    // Step 1: Prepare windows (update history and partition)
    prepare_windows_for_filtering(app);


    // Step 2: Score and filter windows directly from history
    ScoredWindow scored_windows[MAX_WINDOWS];
    int scored_count = score_and_filter_windows(app, filter, app->history, 
                                               app->history_count, scored_windows);
    
    // Step 3: Sort by score
    sort_scored_windows(scored_windows, scored_count, filter);
    
    // Step 4: Finalize results
    finalize_filter_results(app, scored_windows, scored_count);
    
    // Step 4.1: Push Special windows to the end (only when not filtering)
    // When filtering, score-based ordering should be respected
    if (app->filtered_count > 0 && strlen(filter) == 0) {
        WindowInfo normal_windows[MAX_WINDOWS];
        WindowInfo special_windows[MAX_WINDOWS];
        int normal_count = 0;
        int special_count = 0;
        
        // Separate Normal and Special windows
        for (int i = 0; i < app->filtered_count; i++) {
            if (strcmp(app->filtered[i].type, "Normal") == 0) {
                normal_windows[normal_count++] = app->filtered[i];
            } else {
                special_windows[special_count++] = app->filtered[i];
            }
        }
        
        // Rebuild filtered array with Normal windows first, then Special
        int idx = 0;
        for (int i = 0; i < normal_count; i++) {
            app->filtered[idx++] = normal_windows[i];
        }
        for (int i = 0; i < special_count; i++) {
            app->filtered[idx++] = special_windows[i];
        }
        
        log_trace("Separated windows: %d Normal, %d Special", normal_count, special_count);
    }

    // Step 5: Restore and validate selection
    restore_selection(app);
    validate_selection(app);
    
    // Step 6: Apply alt-tab selection if conditions are met
    apply_alt_tab_selection(app, filter);
}

// Apply alt-tab selection: set selection to index 1 when conditions are met
void apply_alt_tab_selection(AppData *app, const char *filter) {
    if (!app || app->current_tab != TAB_WINDOWS) return;
    
    // Check alt-tab conditions:
    // 1. Windows tab active (already checked above)
    // 2. No filter text (empty search string)
    // 3. At least 2 windows
    // 4. Not starting in command mode
    // 5. User has not typed ":" (not in command mode)
    
    if (app->filtered_count >= 2 && 
        filter && strlen(filter) == 0 &&
        !app->start_in_command_mode &&
        !app->start_in_run_mode &&
        app->command_mode.state == CMD_MODE_NORMAL) {
        
        // Set selection to index 1 for alt-tab behavior
        app->selection.window_index = 1;
        if (app->filtered_count > 1) {
            app->selection.selected_window_id = app->filtered[1].id;
        }
        
        log_debug("Alt-tab selection: set selection to index 1 (previous window)");
    }
}
