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
#include "constants.h"
#include "selection.h"
#include "x11_utils.h"
#include "named_window.h"

#define UNUSED __attribute__((unused))

// Structure to hold window info with match score
typedef struct {
    WindowInfo window;
    score_t score;
} ScoredWindow;

// Structure to track word boundary matches
typedef struct {
    int total_words_matched;      // Total number of words that matched
    int consecutive_words;        // Max consecutive words matched
    int match_at_start;          // 1 if match starts at beginning of title
} WordMatchInfo;


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

// Check if a character is a word boundary separator
static int is_word_boundary(char c) {
    return c == ' ' || c == '-' || c == '_' || c == '.' || c == '(' || c == '|';
}

// Count how many consecutive words match the filter characters
static WordMatchInfo analyze_word_matches(const char *filter, const char *title) {
    WordMatchInfo info = {0, 0, 0};
    int filter_len = strlen(filter);
    
    // For each starting position, count consecutive word matches
    for (int start = 0; filter[start]; start++) {
        int filter_idx = start;
        int consecutive = 0;
        int word_position = 0;
        
        for (int i = 0; title[i] && filter_idx < filter_len; i++) {
            int at_word_start = (i == 0 || is_word_boundary(title[i-1]));
            
            if (at_word_start) {
                word_position++;
                // Check if this word starts with the next filter character
                if (tolower(title[i]) == tolower(filter[filter_idx])) {
                    filter_idx++;
                    consecutive++;
                    
                    // Check if this is the best sequence so far
                    if (consecutive > info.consecutive_words) {
                        info.consecutive_words = consecutive;
                        info.total_words_matched = consecutive;
                        if (start == 0 && word_position == 1) {
                            info.match_at_start = 1;
                        }
                    }
                } else {
                    break; // This sequence ended
                }
            }
        }
    }
    
    return info;
}

// Try word boundary match - returns score if matched, SCORE_MIN otherwise
static score_t try_word_boundary_match(const char *filter, const WindowInfo *win) {
    int filter_len = strlen(filter);
    const char *title = win->title;
    
    // First check if filter appears consecutively at start of any word
    for (int i = 0; title[i] && i + filter_len <= (int)strlen(title); i++) {
        int at_word_start = (i == 0 || is_word_boundary(title[i-1]));
        
        if (at_word_start) {
            // Check if the filter matches starting from this word boundary
            int matches = 1;
            for (int j = 0; j < filter_len; j++) {
                if (tolower(title[i + j]) != tolower(filter[j])) {
                    matches = 0;
                    break;
                }
            }
            if (matches) {
                // Found a word boundary match - calculate enhanced score
                score_t base_score = SCORE_WORD_BOUNDARY;
                
                // Analyze the entire title for matching words
                WordMatchInfo word_info = analyze_word_matches(filter, title);
                
                // Add bonus for consecutive word matches
                if (word_info.consecutive_words > 1) {
                    base_score += 300 * (word_info.consecutive_words - 1);
                }
                
                // Add bonus for matching at start of title
                if (i == 0) {
                    base_score += 100;
                }
                
                log_debug("WORD START MATCH: '%s' -> '%s' (base: %.0f, consecutive: %d, score: %.0f)", 
                         filter, title, SCORE_WORD_BOUNDARY, word_info.consecutive_words, base_score);
                return base_score;
            }
        }
    }
    
    return SCORE_MIN;
}


// Try initials match - returns score if matched, SCORE_MIN otherwise
static score_t try_initials_match(const char *filter, const WindowInfo *win) {
    int filter_len = strlen(filter);
    const char *title = win->title;
    int filter_idx = 0;
    int words_matched = 0;
    
    for (int i = 0; title[i] && filter_idx < filter_len; i++) {
        int at_word_start = (i == 0 || is_word_boundary(title[i-1]));
        
        if (at_word_start && tolower(title[i]) == tolower(filter[filter_idx])) {
            filter_idx++;
            words_matched++;
        }
    }
    
    if (filter_idx == filter_len) {
        score_t base_score = SCORE_INITIALS_MATCH;
        
        // Prefer matches where each character matches a word initial
        // (e.g., "tdd" matching "Test Driven Development" vs just "TDD")
        if (words_matched > filter_len) {
            base_score += 50 * (words_matched - filter_len);
        }
        
        log_debug("INITIALS MATCH: '%s' -> '%s' (words: %d, score: %f)", 
                 filter, title, words_matched, base_score);
        return base_score;
    }
    
    return SCORE_MIN;
}

// Try subsequence match - returns score if matched, SCORE_MIN otherwise
static score_t try_subsequence_match(const char *filter, const char *text, const char *label) {
    int filter_len = strlen(filter);
    int filter_idx = 0;
    
    for (int i = 0; text[i] && filter_idx < filter_len; i++) {
        if (tolower(text[i]) == tolower(filter[filter_idx])) {
            filter_idx++;
        }
    }
    
    if (filter_idx == filter_len) {
        score_t score = (label && strstr(label, "TITLE")) ? SCORE_SUBSEQUENCE_MATCH : SCORE_CLASS_INSTANCE_MATCH;
        log_debug("%s SUBSEQUENCE: '%s' -> '%s' (score: %f)", label ? label : "TEXT", filter, text, score);
        return score;
    }
    
    return SCORE_MIN;
}

// Try fuzzy match - returns the match score
static score_t try_fuzzy_match(const char *filter, const char *text, const char *label) {
    if (has_match(filter, text)) {
        score_t score = match(filter, text);
        log_debug("%s FUZZY MATCH: '%s' -> '%s' (score: %f)", label ? label : "TEXT", filter, text, score);
        return score;
    }
    return SCORE_MIN;
}

// Try a match function and update best score if better
static void try_match_and_update(const char *filter, const char *text, const char *label,
                                 score_t (*match_func)(const char*, const char*, const char*),
                                 score_t *best_score) {
    score_t score = match_func(filter, text, label);
    if (score > *best_score) {
        *best_score = score;
    }
}

// Match a window against filter and return best score
static score_t match_window(const char *filter, const WindowInfo *win) {
    score_t best_score = SCORE_MIN;
    
    // Create workspace-aware title for filtering (not displayed)
    char filter_title[MAX_TITLE_LEN + 10];
    if (win->desktop >= 0) {
        snprintf(filter_title, sizeof(filter_title), "%s %d", win->title, win->desktop + 1);
    } else {
        // Sticky windows (-1) - just use original title
        strncpy(filter_title, win->title, sizeof(filter_title) - 1);
        filter_title[sizeof(filter_title) - 1] = '\0';
    }
    
    // Create modified WindowInfo for workspace-aware word boundary and initials matching
    WindowInfo filter_win = *win;
    strncpy(filter_win.title, filter_title, sizeof(filter_win.title) - 1);
    filter_win.title[sizeof(filter_win.title) - 1] = '\0';
    
    // Priority 1: Word boundary match (highest priority, return immediately)
    best_score = try_word_boundary_match(filter, &filter_win);
    if (best_score > SCORE_MIN) {
        return best_score;
    }
    
    // Priority 2: Initials match (second highest, return immediately)
    best_score = try_initials_match(filter, &filter_win);
    if (best_score > SCORE_MIN) {
        return best_score;
    }
    
    // Priority 3: Subsequence match on workspace-aware title (return immediately)
    best_score = try_subsequence_match(filter, filter_title, "TITLE");
    if (best_score > SCORE_MIN) {
        return best_score;
    }
    
    // Priority 4: Try fuzzy match on workspace-aware title
    best_score = try_fuzzy_match(filter, filter_title, "TITLE");
    
    // Priority 5: Try class/instance matching (only if no good title match)
    if (best_score < SCORE_SUBSEQUENCE_MATCH) {
        // Try subsequence on class and instance
        try_match_and_update(filter, win->class_name, "CLASS", 
                            try_subsequence_match, &best_score);
        try_match_and_update(filter, win->instance, "INSTANCE", 
                            try_subsequence_match, &best_score);
    }
    
    // Priority 6: Fuzzy fallback on class/instance (only if no matches yet)
    if (best_score <= SCORE_MIN) {
        try_match_and_update(filter, win->class_name, "CLASS", 
                            try_fuzzy_match, &best_score);
        try_match_and_update(filter, win->instance, "INSTANCE", 
                            try_fuzzy_match, &best_score);
    }
    
    return best_score;
}


// Prepare windows for filtering by updating history and partitioning
static void prepare_windows_for_filtering(AppData *app) {
    log_trace("Before pipeline - history_count=%d", app->history_count);
    
    // First, update the complete window processing pipeline
    update_history(app);
    partition_and_reorder(app);
    
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
                // Add significant bonus for current workspace windows
                // This should be enough to prioritize them but not override better matches
                score_t workspace_bonus = 500;
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
    
    // Step 4.1: Push Special windows to the end
    if (app->filtered_count > 0) {
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
        app->command_mode.state != CMD_MODE_COMMAND) {
        
        // Set selection to index 1 for alt-tab behavior
        app->selection.window_index = 1;
        if (app->filtered_count > 1) {
            app->selection.selected_window_id = app->filtered[1].id;
        }
        
        log_debug("Alt-tab selection: set selection to index 1 (previous window)");
    }
}

