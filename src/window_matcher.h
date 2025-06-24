#ifndef WINDOW_MATCHER_H
#define WINDOW_MATCHER_H

#include <stdbool.h>
#include "window_info.h"

// Check if two windows match exactly (same class, instance, type, and title)
bool windows_match_exact(const WindowInfo *window1, const WindowInfo *window2);

// Check if two windows match with fuzzy title matching (same class, instance, type, but title can differ)
bool windows_match_fuzzy(const WindowInfo *window1, const WindowInfo *window2);

// Extract the base part of a title before a dash (e.g., "Firefox - Page 1" -> "Firefox ")
// Returns the length of the base part, or 0 if no dash found
int get_title_base_length(const char *title);

// Check if two titles match with fuzzy logic
bool titles_match_fuzzy(const char *title1, const char *title2);

#endif // WINDOW_MATCHER_H