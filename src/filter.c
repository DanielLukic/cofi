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

// Try initials match on the display string
static score_t try_initials_on_display(const char *filter, const char *display) {
    int filter_len = strlen(filter);
    int filter_idx = 0;

    for (int i = 0; display[i] && filter_idx < filter_len; i++) {
        int at_word_start = (i == 0 || display[i-1] == ' ' || display[i-1] == '-' ||
                            display[i-1] == '_' || display[i-1] == '.' ||
                            display[i-1] == '(' || display[i-1] == '|');
        if (at_word_start && tolower(display[i]) == tolower(filter[filter_idx])) {
            filter_idx++;
        }
    }

    if (filter_idx == filter_len) {
        return SCORE_INITIALS_MATCH;
    }
    return SCORE_MIN;
}

// Match a window against filter and return best score
// Uses fzy on the full display string (what the user sees), plus initials bonus
static score_t match_window(const char *filter, const WindowInfo *win) {
    // Compose the same string the user sees
    char display[1024];
    compose_display_string(win, display, sizeof(display));

    // Primary: fzf match on full display string
    score_t best_score = SCORE_MIN;
    if (fzf_has_match(filter, display)) {
        best_score = fzf_fuzzy_match(filter, display);
        log_debug("FZF: '%s' -> '%s' (score: %.0f)", filter, display, best_score);
    }

    // Bonus: initials match (e.g., "ddl" -> "Daniel Dario Lukic")
    score_t initials = try_initials_on_display(filter, display);
    if (initials > best_score) {
        best_score = initials;
        log_debug("INITIALS: '%s' -> '%s' (score: %.0f)", filter, display, initials);
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
                score_t workspace_bonus = 25;
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
        app->command_mode.state != CMD_MODE_COMMAND) {
        
        // Set selection to index 1 for alt-tab behavior
        app->selection.window_index = 1;
        if (app->filtered_count > 1) {
            app->selection.selected_window_id = app->filtered[1].id;
        }
        
        log_debug("Alt-tab selection: set selection to index 1 (previous window)");
    }
}

