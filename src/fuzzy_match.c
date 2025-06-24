#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "fuzzy_match.h"
#include "window_info.h"

// Helper to check if a character is a word boundary
static int is_word_boundary(char c) {
    return (c == ' ' || c == '-' || c == '_' || c == '.' || c == '/' || c == '\\' ||
            c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
            c == ':' || c == ';' || c == ',' || c == '!' || c == '?' || c == '@' ||
            c == '#' || c == '$' || c == '%' || c == '^' || c == '&' || c == '*' ||
            c == '+' || c == '=' || c == '|' || c == '<' || c == '>' || c == '"' ||
            c == '\'' || c == '`' || c == '~');
}

// Extract word start characters from haystack
static void get_word_starts(const char *haystack, char *word_starts, int *word_count) {
    int at_word_start = 1;
    *word_count = 0;
    
    for (int i = 0; haystack[i]; i++) {
        if (at_word_start && !is_word_boundary(haystack[i])) {
            if (*word_count < MAX_TITLE_LEN) {
                word_starts[(*word_count)++] = tolower(haystack[i]);
            }
        }
        at_word_start = is_word_boundary(haystack[i]);
    }
    word_starts[*word_count] = '\0';
}

// Check if needle matches as initials in the haystack
static int matches_initials(const char *needle, const char *haystack) {
    char word_starts[MAX_TITLE_LEN];
    int word_count;
    
    get_word_starts(haystack, word_starts, &word_count);
    
    // Check if needle matches the word starts exactly
    int needle_len = strlen(needle);
    if (needle_len > word_count) {
        return 0;
    }
    
    // Case-insensitive comparison of needle with word starts
    for (int i = 0; i < needle_len; i++) {
        if (tolower(needle[i]) != word_starts[i]) {
            return 0;
        }
    }
    
    return 1;
}

// Calculate match score for a single character match
static int char_score(const char *haystack, int hay_idx, int is_word_start, int consecutive) {
    int score = 1;  // Base score for any match
    
    // Bonus for matching at word start
    if (is_word_start) {
        score += 10;
    }
    
    // Bonus for consecutive matches
    if (consecutive) {
        score += 5;
    }
    
    // Bonus for matching at the very beginning
    if (hay_idx == 0) {
        score += 15;
    }
    
    return score;
}

// Try to match needle as consecutive capitals in haystack
static int match_consecutive_capitals(const char *needle, const char *haystack, int *matched_count) {
    int needle_idx = 0;
    int needle_len = strlen(needle);
    *matched_count = 0;
    
    for (int i = 0; haystack[i] && needle_idx < needle_len; i++) {
        // Look for capital letters
        if (haystack[i] >= 'A' && haystack[i] <= 'Z') {
            if (tolower(haystack[i]) == tolower(needle[needle_idx])) {
                needle_idx++;
                (*matched_count)++;
            }
        }
    }
    
    return needle_idx == needle_len;
}

// Try multiple matching strategies and return the best score
static int try_multiple_strategies(const char *needle, const char *haystack, const WindowInfo *win) {
    int best_score = 0;
    int score = 0;
    
    // Strategy 1: Try initials match on the full search string
    if (matches_initials(needle, haystack)) {
        best_score = 500;  // Good score for full initials match
    }
    
    // Strategy 2: Try initials match on just the title
    if (win && matches_initials(needle, win->title)) {
        score = 1000;  // Very high score for title initials match
        if (score > best_score) best_score = score;
    }
    
    // Strategy 3: Try consecutive capitals match on title
    if (win) {
        int matched_count = 0;
        if (match_consecutive_capitals(needle, win->title, &matched_count)) {
            score = 800 + matched_count * 10;  // High score for capital letters match
            if (score > best_score) best_score = score;
        }
    }
    
    return best_score;
}

// Simple fuzzy matching with scoring
int fuzzy_match(const char *needle, const char *haystack, int *out_score) {
    if (!needle || !haystack || !out_score) {
        return 0;
    }
    
    *out_score = 0;
    
    int needle_len = strlen(needle);
    int haystack_len = strlen(haystack);
    
    if (needle_len == 0) {
        *out_score = 100;  // Empty needle matches everything with high score
        return 1;
    }
    
    if (needle_len > haystack_len) {
        return 0;
    }
    
    // Try multiple matching strategies (pass NULL for win since we don't have it here)
    int strategy_score = try_multiple_strategies(needle, haystack, NULL);
    if (strategy_score > 0) {
        *out_score = strategy_score;
        return 1;
    }
    
    // Now do regular fuzzy matching
    int needle_idx = 0;
    int prev_match_idx = -1;
    int score = 0;
    int consecutive = 0;
    
    for (int hay_idx = 0; hay_idx < haystack_len && needle_idx < needle_len; hay_idx++) {
        if (tolower(haystack[hay_idx]) == tolower(needle[needle_idx])) {
            // Check if this is a word start
            int is_word_start = (hay_idx == 0) || 
                               (hay_idx > 0 && is_word_boundary(haystack[hay_idx-1]));
            
            // Check if consecutive with previous match
            consecutive = (prev_match_idx >= 0 && hay_idx == prev_match_idx + 1);
            
            score += char_score(haystack, hay_idx, is_word_start, consecutive);
            
            prev_match_idx = hay_idx;
            needle_idx++;
        } else {
            consecutive = 0;
        }
    }
    
    // Did we match all characters?
    if (needle_idx == needle_len) {
        // Penalty for longer strings (but not too harsh)
        score = score - (haystack_len - needle_len) / 10;
        if (score < 1) score = 1;  // Minimum score of 1 for any match
        
        *out_score = score;
        return 1;
    }
    
    return 0;
}

// Enhanced fuzzy match that considers window fields separately
int fuzzy_match_window(const char *needle, const WindowInfo *win, int *out_score) {
    if (!needle || !win || !out_score) {
        return 0;
    }
    
    *out_score = 0;
    
    // First create the full search string
    char search_string[1024];
    create_search_string(win, search_string, sizeof(search_string));
    
    // Try multiple matching strategies with window info
    int strategy_score = try_multiple_strategies(needle, search_string, win);
    if (strategy_score > 0) {
        *out_score = strategy_score;
        return 1;
    }
    
    // Fall back to regular fuzzy match on full search string
    return fuzzy_match(needle, search_string, out_score);
}

// Create a concatenated string from window info in display order
void create_search_string(const WindowInfo *win, char *buffer, size_t buffer_size) {
    // Format: "[desktop] instance title class"
    snprintf(buffer, buffer_size, "[%d] %s %s %s", 
             win->desktop, win->instance, win->title, win->class_name);
}