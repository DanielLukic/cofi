#ifndef MATCH_ENTRY_H
#define MATCH_ENTRY_H

#include <X11/Xlib.h>
#include <stdbool.h>
#include "x11/window_info.h"
#include "core/utils/constants.h"

// Structure to store a matching entry.
typedef struct MatchEntry {
    int match_id;                       // Stable persistent key (never reused)
    Window bound_x11_id;                // Live X11 binding (reassigned via title pattern)
    char class_name[MAX_CLASS_LEN];    // Optional class anchor (exact if set)
    char instance[MAX_CLASS_LEN];      // Optional instance anchor (exact if set)
    char type[16];                     // Optional type anchor (exact if set)
    char original_title[MAX_TITLE_LEN]; // Captured title or pattern
    int assigned;                      // 1 if matched to existing window, 0 if orphaned
} MatchEntry;

// Manager for all matching entries.
typedef struct {
    MatchEntry entries[MAX_WINDOWS];
    int count;
    int next_match_id;
} MatchEntryManager;

// Initialize the match entry manager.
void match_entry_manager_init(MatchEntryManager *manager);

// Check if a window is already bound in the registry.
int match_entry_is_bound_window(const MatchEntryManager *manager, Window id);

// Check and reassign orphaned entries to matching windows.
// Returns true if any entries were reassigned.
bool match_entry_reassign_live_windows(MatchEntryManager *manager, WindowInfo *windows, int window_count);

// Delete entries that are not referenced by any consumer match_id list.
// Returns the number of entries removed.
int match_entry_gc(MatchEntryManager *manager, const int *referenced_ids, int referenced_count);

// Delete a match entry by index.
void match_entry_delete(MatchEntryManager *manager, int index);

// Delete a match entry by stable match id. Missing ids are a no-op.
void match_entry_delete_by_match_id(MatchEntryManager *manager, int match_id);

// Get entry by index.
MatchEntry* match_entry_get_by_index(MatchEntryManager *manager, int index);

// Find entry index by bound window ID.
int match_entry_find_index_by_window(const MatchEntryManager *manager, Window id);
int match_entry_find_index_by_match_id(const MatchEntryManager *manager, int match_id);

// Match a live window against a stored entry's title pattern.
bool match_entry_matches_window(const MatchEntry *entry, const WindowInfo *window);

// Create a new match entry for a live window and return its stable match_id.
int matching_create_entry(MatchEntryManager *manager, const WindowInfo *w);
int matching_create_pattern_entry(MatchEntryManager *manager, const char *pattern);

#endif // MATCH_ENTRY_H
