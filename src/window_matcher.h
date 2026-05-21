#ifndef WINDOW_MATCHER_H
#define WINDOW_MATCHER_H

#include <stdbool.h>
#include "window_info.h"

typedef enum {
    TITLE_MATCH_MODE_LEGACY_WILDCARD = 0,  // '*' + '.' semantics via wildcard_match
    TITLE_MATCH_MODE_EXACT = 1,            // strcmp
    TITLE_MATCH_MODE_GLOB = 2              // '*' + '?' semantics via glob_match
} TitleMatchMode;

// Check if two windows match with fuzzy title matching (same class, instance, type, but title can differ)
bool windows_match_fuzzy(const WindowInfo *window1, const WindowInfo *window2);

// Extract the base part of a title before a dash (e.g., "Firefox - Page 1" -> "Firefox ")
// Returns the length of the base part, or 0 if no dash found
int get_title_base_length(const char *title);

// Check if two titles match with fuzzy logic
bool titles_match_fuzzy(const char *title1, const char *title2);

// Shared exact class/instance/type + title matcher core with mode-controlled title semantics.
bool window_matches_identity_and_title_pattern(const WindowInfo *window,
                                               const char *class_name,
                                               const char *instance,
                                               const char *type,
                                               const char *title_pattern,
                                               TitleMatchMode title_mode);

// Wildcard matching function
// '*' matches any sequence of characters, '.' matches any single character
bool wildcard_match(const char *pattern, const char *str);

// Glob matching function
// '*' matches any sequence of characters, '?' matches any single character
bool glob_match(const char *pattern, const char *str);

#endif // WINDOW_MATCHER_H
