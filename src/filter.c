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
            // Multi-stage matching as suggested by user
            score_t best_score = SCORE_MIN;
            int filter_len = strlen(filter);
            
            // Stage 1: Try two types of word boundary matches
            // 1a. Check if filter appears consecutively at start of any word
            int found_consecutive_at_word_start = 0;
            for (int i = 0; win->title[i] && i + filter_len <= strlen(win->title); i++) {
                int at_word_start = (i == 0 || win->title[i-1] == ' ' || win->title[i-1] == '-' || 
                                   win->title[i-1] == '_' || win->title[i-1] == '.' || win->title[i-1] == '(');
                
                if (at_word_start) {
                    // Check if the filter matches starting from this word boundary
                    int matches = 1;
                    for (int j = 0; j < filter_len; j++) {
                        if (tolower(win->title[i + j]) != tolower(filter[j])) {
                            matches = 0;
                            break;
                        }
                    }
                    if (matches) {
                        found_consecutive_at_word_start = 1;
                        break;
                    }
                }
            }
            
            // 1b. Check if filter matches initials of words
            int found_initials_match = 0;
            if (!found_consecutive_at_word_start) {
                int filter_idx = 0;
                for (int i = 0; win->title[i] && filter_idx < filter_len; i++) {
                    int at_word_start = (i == 0 || win->title[i-1] == ' ' || win->title[i-1] == '-' || 
                                       win->title[i-1] == '_' || win->title[i-1] == '.' || win->title[i-1] == '(');
                    
                    if (at_word_start && tolower(win->title[i]) == tolower(filter[filter_idx])) {
                        filter_idx++;
                    }
                }
                if (filter_idx == filter_len) {
                    found_initials_match = 1;
                }
            }
            
            if (found_consecutive_at_word_start) {
                // Consecutive match at word start - highest priority
                best_score = 2000;
                fprintf(stderr, "WORD START MATCH: '%s' -> '%s' (score: %f)\n", filter, win->title, best_score);
            } else if (found_initials_match) {
                // Initials match - very high priority
                best_score = 1900;
                fprintf(stderr, "INITIALS MATCH: '%s' -> '%s' (score: %f)\n", filter, win->title, best_score);
            } else {
                // Stage 2: Try simple subsequence match (e.g., "dll" -> d.*l.*l)
                int filter_idx = 0;
                for (int i = 0; win->title[i] && filter_idx < filter_len; i++) {
                    if (tolower(win->title[i]) == tolower(filter[filter_idx])) {
                        filter_idx++;
                    }
                }
                
                if (filter_idx == filter_len) {
                    // Subsequence match found - give it good score but less than word boundary
                    best_score = 1500;
                    fprintf(stderr, "SUBSEQUENCE MATCH: '%s' -> '%s' (score: %f)\n", filter, win->title, best_score);
                } else {
                    // Stage 3: Fall back to fuzzy matching
                    if (has_match(filter, win->title)) {
                        best_score = match(filter, win->title);
                        fprintf(stderr, "FUZZY MATCH: '%s' -> '%s' (score: %f)\n", filter, win->title, best_score);
                    }
                }
            }
            
            // Also check class and instance with simple subsequence matching
            // Check class_name
            if (best_score < 1500) { // Only if we don't already have a good match
                int filter_idx = 0;
                for (int i = 0; win->class_name[i] && filter_idx < filter_len; i++) {
                    if (tolower(win->class_name[i]) == tolower(filter[filter_idx])) {
                        filter_idx++;
                    }
                }
                if (filter_idx == filter_len) {
                    best_score = 1400; // Slightly less than title subsequence
                    fprintf(stderr, "CLASS SUBSEQUENCE: '%s' -> '%s' (score: %f)\n", filter, win->class_name, best_score);
                }
            }
            
            // Check instance
            if (best_score < 1500) { // Only if we don't already have a good match
                int filter_idx = 0;
                for (int i = 0; win->instance[i] && filter_idx < filter_len; i++) {
                    if (tolower(win->instance[i]) == tolower(filter[filter_idx])) {
                        filter_idx++;
                    }
                }
                if (filter_idx == filter_len) {
                    best_score = 1400; // Same as class
                    fprintf(stderr, "INSTANCE SUBSEQUENCE: '%s' -> '%s' (score: %f)\n", filter, win->instance, best_score);
                }
            }
            
            // Final fallback to fuzzy matching on class/instance
            if (best_score <= SCORE_MIN) {
                if (has_match(filter, win->class_name)) {
                    score_t class_score = match(filter, win->class_name);
                    if (class_score > best_score) best_score = class_score;
                }
                
                if (has_match(filter, win->instance)) {
                    score_t instance_score = match(filter, win->instance);
                    if (instance_score > best_score) best_score = instance_score;
                }
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
    
    // Sort by score if we have a filter
    if (strlen(filter) > 0 && scored_count > 0) {
        qsort(scored_windows, scored_count, sizeof(ScoredWindow), compare_scores);
        
        // Debug: print sorted results
        fprintf(stderr, "\n=== Sorted results for filter '%s' ===\n", filter);
        for (int i = 0; i < scored_count && i < 5; i++) {
            fprintf(stderr, "%d: %s (score: %f)\n", i, scored_windows[i].window.title, scored_windows[i].score);
        }
        fprintf(stderr, "=====================================\n");
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