#ifndef FUZZY_MATCH_H
#define FUZZY_MATCH_H

#include "window_info.h"

// Fuzzy matching with scoring
int fuzzy_match(const char *needle, const char *haystack, int *out_score);

// Enhanced fuzzy match that considers window fields separately
int fuzzy_match_window(const char *needle, const WindowInfo *win, int *out_score);

// Create a concatenated search string from window info in display order
void create_search_string(const WindowInfo *win, char *buffer, size_t buffer_size);

#endif // FUZZY_MATCH_H