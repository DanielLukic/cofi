#include "window_matcher.h"
#include <string.h>
#include <stdbool.h>

bool windows_match_fuzzy(const WindowInfo *window1, const WindowInfo *window2) {
    if (!window1 || !window2) return false;
    
    // First check if class, instance, and type match
    if (strcmp(window1->class_name, window2->class_name) != 0 ||
        strcmp(window1->instance, window2->instance) != 0 ||
        strcmp(window1->type, window2->type) != 0) {
        return false;
    }
    
    // Then check if titles match with fuzzy logic
    return titles_match_fuzzy(window1->title, window2->title);
}

int get_title_base_length(const char *title) {
    if (!title) return 0;
    
    char *dash = strchr(title, '-');
    if (!dash) return 0;
    
    // Return length including the space before the dash
    return dash - title;
}

bool titles_match_fuzzy(const char *title1, const char *title2) {
    if (!title1 || !title2) return false;
    
    // Exact match
    if (strcmp(title1, title2) == 0) return true;
    
    // Check if both have dashes and share the same base
    int base_len1 = get_title_base_length(title1);
    int base_len2 = get_title_base_length(title2);
    
    if (base_len1 > 0 && base_len2 > 0) {
        // Both have dashes, check if base parts match
        if (base_len1 == base_len2 && strncmp(title1, title2, base_len1) == 0) {
            return true;
        }
    }
    
    // Check if one title contains the other
    if (strstr(title1, title2) || strstr(title2, title1)) {
        return true;
    }
    
    return false;
}

// Helper function to match a string against a pattern with wildcards
// '*' matches any sequence of characters (including empty)
// '.' matches any single character
static bool wildcard_match(const char *pattern, const char *str) {
    if (!pattern || !str) return false;
    
    while (*pattern && *str) {
        if (*pattern == '*') {
            // Skip consecutive stars
            while (*pattern == '*') pattern++;
            
            // If pattern ends with *, it matches everything remaining
            if (!*pattern) return true;
            
            // Try to match the rest of the pattern at each position
            while (*str) {
                if (wildcard_match(pattern, str)) {
                    return true;
                }
                str++;
            }
            return false;
        } else if (*pattern == '.') {
            // '.' matches any single character
            pattern++;
            str++;
        } else if (*pattern == *str) {
            // Exact character match
            pattern++;
            str++;
        } else {
            // Characters don't match
            return false;
        }
    }
    
    // Skip any trailing stars in the pattern
    while (*pattern == '*') pattern++;
    
    // Both should be at the end for a match
    return !*pattern && !*str;
}

// Check if window matches harpoon slot with wildcard support
bool window_matches_harpoon_slot(const WindowInfo *window, const HarpoonSlot *slot) {
    if (!window || !slot || !slot->assigned) return false;
    
    // Class and instance must match exactly
    if (strcmp(window->class_name, slot->class_name) != 0 ||
        strcmp(window->instance, slot->instance) != 0 ||
        strcmp(window->type, slot->type) != 0) {
        return false;
    }
    
    // Title can use wildcard matching
    return wildcard_match(slot->title, window->title);
}