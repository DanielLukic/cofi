#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "filter.h"
#include "window_info.h"
#include "history.h"
#include "log.h"
#include <strings.h>

#include "match.h"
#include "constants.h"

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

// Check if a character is a word boundary separator
static int is_word_boundary(char c) {
    return c == ' ' || c == '-' || c == '_' || c == '.' || c == '(';
}

// Try word boundary match - returns score if matched, SCORE_MIN otherwise
static score_t try_word_boundary_match(const char *filter, const WindowInfo *win) {
    int filter_len = strlen(filter);
    const char *title = win->title;
    
    // Check if filter appears consecutively at start of any word
    for (int i = 0; title[i] && i + filter_len <= strlen(title); i++) {
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
                log_debug("WORD START MATCH: '%s' -> '%s' (score: %f)", filter, title, SCORE_WORD_BOUNDARY);
                return SCORE_WORD_BOUNDARY;
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
    
    for (int i = 0; title[i] && filter_idx < filter_len; i++) {
        int at_word_start = (i == 0 || is_word_boundary(title[i-1]));
        
        if (at_word_start && tolower(title[i]) == tolower(filter[filter_idx])) {
            filter_idx++;
        }
    }
    
    if (filter_idx == filter_len) {
        log_debug("INITIALS MATCH: '%s' -> '%s' (score: %f)", filter, title, SCORE_INITIALS_MATCH);
        return SCORE_INITIALS_MATCH;
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

// Match a window against filter and return best score
static score_t match_window(const char *filter, const WindowInfo *win) {
    score_t best_score = SCORE_MIN;
    
    // Stage 1: Try word boundary match
    best_score = try_word_boundary_match(filter, win);
    if (best_score > SCORE_MIN) {
        return best_score;
    }
    
    // Stage 2: Try initials match
    best_score = try_initials_match(filter, win);
    if (best_score > SCORE_MIN) {
        return best_score;
    }
    
    // Stage 3: Try subsequence match on title
    best_score = try_subsequence_match(filter, win->title, "TITLE");
    if (best_score > SCORE_MIN) {
        return best_score;
    }
    
    // Stage 4: Try fuzzy match on title
    best_score = try_fuzzy_match(filter, win->title, "TITLE");
    
    // Stage 5: Try subsequence match on class/instance (if no good title match)
    if (best_score < SCORE_SUBSEQUENCE_MATCH) {
        score_t class_score = try_subsequence_match(filter, win->class_name, "CLASS");
        if (class_score > best_score) best_score = class_score;
        
        score_t instance_score = try_subsequence_match(filter, win->instance, "INSTANCE");
        if (instance_score > best_score) best_score = instance_score;
    }
    
    // Stage 6: Try fuzzy match on class/instance (final fallback)
    if (best_score <= SCORE_MIN) {
        score_t class_score = try_fuzzy_match(filter, win->class_name, "CLASS");
        if (class_score > best_score) best_score = class_score;
        
        score_t instance_score = try_fuzzy_match(filter, win->instance, "INSTANCE");
        if (instance_score > best_score) best_score = instance_score;
    }
    
    return best_score;
}

// Filter windows based on search text (now works with history-ordered windows)
void filter_windows(AppData *app, const char *filter) {
    app->filtered_count = 0;
    
    log_debug("filter_windows() called with filter='%s'", filter);
    log_trace("Before pipeline - history_count=%d", app->history_count);
    
    // First, update the complete window processing pipeline
    update_history(app);
    partition_and_reorder(app);
    
    log_trace("After pipeline - history_count=%d", app->history_count);
    
    // Clone the history array for Alt-Tab processing
    WindowInfo cloned_history[MAX_WINDOWS];
    int cloned_count = app->history_count;
    for (int i = 0; i < app->history_count; i++) {
        cloned_history[i] = app->history[i];
    }
    
    // Apply Alt-Tab swap ONCE on the cloned list (only if we have 2+ windows)
    if (cloned_count >= 2) {
        WindowInfo temp = cloned_history[0];
        cloned_history[0] = cloned_history[1];
        cloned_history[1] = temp;
        log_debug("Alt-Tab swap applied: [0]='%s' [1]='%s'", 
                 cloned_history[0].title, cloned_history[1].title);
    }
    
    // Temporary array to hold scored windows
    ScoredWindow scored_windows[MAX_WINDOWS];
    int scored_count = 0;
    
    // Now filter the processed and swapped clone
    for (int i = 0; i < cloned_count; i++) {
        WindowInfo *win = &cloned_history[i];
        
        if (strlen(filter) == 0) {
            // No filter - include all windows with max score
            if (scored_count < MAX_WINDOWS) {
                scored_windows[scored_count].window = *win;
                scored_windows[scored_count].score = 1000; // Max score for no filter
                scored_count++;
            }
        } else {
            // Use the new match_window function for all matching logic
            score_t best_score = match_window(filter, win);
            
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
    
    // Sort by score if we have a filter
    if (strlen(filter) > 0 && scored_count > 0) {
        qsort(scored_windows, scored_count, sizeof(ScoredWindow), compare_scores);
        
        // Debug: print sorted results
        log_debug("=== Sorted results for filter '%s' ===", filter);
        for (int i = 0; i < scored_count && i < 5; i++) {
            log_debug("%d: %s (score: %f)", i, scored_windows[i].window.title, scored_windows[i].score);
        }
        log_debug("=====================================");
    }
    
    // Copy sorted windows to filtered array
    for (int i = 0; i < scored_count && i < MAX_WINDOWS; i++) {
        app->filtered[i] = scored_windows[i].window;
        app->filtered_count++;
    }
    
    // ALWAYS select the FIRST window for Alt-Tab behavior
    // After alt-tab swap, position 0 contains the previous window (the one we want to switch to)
    app->selected_index = 0;
    
    // Ensure selection is within bounds
    if (app->filtered_count == 0) {
        app->selected_index = 0;
    } else if (app->selected_index >= app->filtered_count) {
        app->selected_index = 0;
    }
    
    log_debug("Selection reset to %d (filtered_count=%d)", app->selected_index, app->filtered_count);
}