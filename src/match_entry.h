#ifndef MATCH_ENTRY_H
#define MATCH_ENTRY_H

#include <X11/Xlib.h>
#include <stdbool.h>
#include "window_info.h"
#include "constants.h"
#include "window_matcher.h"

// Structure to store a matching entry.
typedef struct MatchEntry {
    int match_id;                       // Stable persistent key (never reused)
    Window bound_x11_id;                // Live X11 binding (validated against criteria)
    char custom_name[MAX_TITLE_LEN];   // User-defined custom name
    char original_title[MAX_TITLE_LEN]; // Captured title or pattern
    char class_name[MAX_CLASS_LEN];    // Window class name
    char instance[MAX_CLASS_LEN];      // Window instance name
    char type[16];                     // Window type ("Normal" or "Special")
    TitleMatchMode match_mode;         // EXACT(default) or GLOB (future UI edit)
    int assigned;                      // 1 if matched to existing window, 0 if orphaned
    int geom_x;
    int geom_y;
    int geom_w;
    int geom_h;
    int geom_desktop;
    int has_geom;
} MatchEntry;

// Manager for all matching entries.
typedef struct {
    MatchEntry entries[MAX_WINDOWS];
    int count;
    int next_match_id;
} MatchEntryManager;

// Initialize the match entry manager.
void match_entry_manager_init(MatchEntryManager *manager);

// Assign a custom label and capture criteria for a window.
void match_entry_assign_custom_name(MatchEntryManager *manager, const WindowInfo *window, const char *custom_name);

// Get the custom label for a window (returns NULL if no custom label).
const char* match_entry_get_custom_name(const MatchEntryManager *manager, Window id);

// Check if a window is already bound in the registry.
int match_entry_is_bound_window(const MatchEntryManager *manager, Window id);

// Check and reassign orphaned entries to matching windows.
// Returns true if any entries were reassigned.
bool match_entry_reassign_live_windows(MatchEntryManager *manager, WindowInfo *windows, int window_count);

// Collect match_ids referenced by the naming/display consumer.
int match_entry_collect_labeled_ids(const MatchEntryManager *manager, int *out, int max);

// Collect match_ids referenced by the geometry consumer.
int match_entry_collect_geom_ids(const MatchEntryManager *manager, int *out, int max);

// Delete entries that are not referenced by any consumer match_id list.
// Returns the number of entries removed.
int match_entry_gc(MatchEntryManager *manager, const int *referenced_ids, int referenced_count);

// Delete a custom label by index.
void match_entry_delete_custom_name(MatchEntryManager *manager, int index);

// Update an existing custom label.
void match_entry_update_custom_name(MatchEntryManager *manager, int index, const char *new_name);

// Get entry by index.
MatchEntry* match_entry_get_by_index(MatchEntryManager *manager, int index);

// Find entry index by bound window ID.
int match_entry_find_index_by_window(const MatchEntryManager *manager, Window id);
int match_entry_find_index_by_match_id(const MatchEntryManager *manager, int match_id);

// Find entry index by custom label.
int match_entry_find_index_by_custom_name(const MatchEntryManager *manager, const char *custom_name);

// Capture or deduplicate a match entry for a live window and return its stable match_id.
int matching_capture_or_get(MatchEntryManager *manager,
                            WindowInfo *windows,
                            int window_count,
                            const WindowInfo *w);

#endif // MATCH_ENTRY_H
