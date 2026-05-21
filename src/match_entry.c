#include "match_entry.h"
#include <string.h>
#include <stdio.h>
#include "log.h"
#include "window_matcher.h"
#include "utils.h"

static int allocate_match_id(MatchEntryManager *manager) {
    if (!manager) return -1;
    if (manager->next_match_id <= 0) {
        manager->next_match_id = 1;
    }
    return manager->next_match_id++;
}

static const WindowInfo *find_live_window_by_id(const WindowInfo *windows, int window_count, Window id) {
    if (!windows || id == 0) return NULL;
    for (int i = 0; i < window_count; i++) {
        if (windows[i].id == id) return &windows[i];
    }
    return NULL;
}

static int match_id_is_referenced(const int *referenced_ids, int referenced_count, int match_id) {
    if (!referenced_ids || referenced_count <= 0 || match_id <= 0) return 0;
    for (int i = 0; i < referenced_count; i++) {
        if (referenced_ids[i] == match_id) return 1;
    }
    return 0;
}

void match_entry_manager_init(MatchEntryManager *manager) {
    if (!manager) return;
    
    manager->count = 0;
    manager->next_match_id = 1;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        manager->entries[i].assigned = 0;
        manager->entries[i].match_id = 0;
        manager->entries[i].bound_x11_id = 0;
        manager->entries[i].custom_name[0] = '\0';
        manager->entries[i].original_title[0] = '\0';
        manager->entries[i].class_name[0] = '\0';
        manager->entries[i].instance[0] = '\0';
        manager->entries[i].type[0] = '\0';
        manager->entries[i].match_mode = TITLE_MATCH_MODE_EXACT;
        manager->entries[i].geom_x = 0;
        manager->entries[i].geom_y = 0;
        manager->entries[i].geom_w = 0;
        manager->entries[i].geom_h = 0;
        manager->entries[i].geom_desktop = 0;
        manager->entries[i].has_geom = 0;
    }
}

void match_entry_assign_custom_name(MatchEntryManager *manager, const WindowInfo *window, const char *custom_name) {
    if (!manager || !window || !custom_name || strlen(custom_name) == 0) return;
    
    // Check if window already has a name
    int existing_idx = match_entry_find_index_by_window(manager, window->id);
    
    if (existing_idx >= 0) {
        // Update existing name
        safe_string_copy(manager->entries[existing_idx].custom_name, custom_name, MAX_TITLE_LEN);
        log_info("Updated custom name for window 0x%lx to '%s'", window->id, custom_name);
    } else {
        // Add new named window
        if (manager->count >= MAX_WINDOWS) {
            log_error("Cannot add more named windows, limit reached");
            return;
        }
        
        MatchEntry *entry = &manager->entries[manager->count];
        entry->match_id = allocate_match_id(manager);
        entry->bound_x11_id = window->id;
        safe_string_copy(entry->custom_name, custom_name, MAX_TITLE_LEN);
        safe_string_copy(entry->original_title, window->title, MAX_TITLE_LEN);
        
        safe_string_copy(entry->class_name, window->class_name, MAX_CLASS_LEN);
        safe_string_copy(entry->instance, window->instance, MAX_CLASS_LEN);
        safe_string_copy(entry->type, window->type, 16);
        entry->match_mode = TITLE_MATCH_MODE_EXACT;
        entry->assigned = 1;
        
        manager->count++;
        log_info("Assigned custom name '%s' to window 0x%lx", custom_name, window->id);
    }
}

const char* match_entry_get_custom_name(const MatchEntryManager *manager, Window id) {
    if (!manager || id == 0) return NULL;
    
    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].bound_x11_id == id && manager->entries[i].assigned) {
            return manager->entries[i].custom_name;
        }
    }
    return NULL;
}

int match_entry_is_bound_window(const MatchEntryManager *manager, Window id) {
    return match_entry_get_custom_name(manager, id) != NULL;
}

// Helper function to check if a window matches a named window entry
static int window_matches_named_entry(const WindowInfo *window, const MatchEntry *entry) {
    if (!window || !entry) return 0;

    return window_matches_identity_and_title_pattern(window,
                                                     entry->class_name,
                                                     entry->instance,
                                                     entry->type,
                                                     entry->original_title,
                                                     entry->match_mode);
}

bool match_entry_reassign_live_windows(MatchEntryManager *manager, WindowInfo *windows, int window_count) {
    if (!manager || !windows) return false;
    
    log_trace("match_entry_reassign_live_windows: checking %d windows against %d named entries",
             window_count, manager->count);
    
    int config_changed = 0;
    
    // Every entry in [0, count) is live. assigned=0 means currently unbound.
    for (int i = 0; i < manager->count; i++) {
        MatchEntry *entry = &manager->entries[i];

        log_trace("Checking named entry %d: bound 0x%lx (%s), assigned=%d",
                  i, entry->bound_x11_id, entry->custom_name, entry->assigned);
        Window old_id = entry->bound_x11_id;
        int has_valid_binding = 0;

        // Validate persisted binding by class/instance/type only (title can drift).
        if (entry->bound_x11_id != 0) {
            const WindowInfo *bound = find_live_window_by_id(windows, window_count, entry->bound_x11_id);
            if (bound &&
                strcmp(bound->class_name, entry->class_name) == 0 &&
                strcmp(bound->instance, entry->instance) == 0 &&
                strcmp(bound->type, entry->type) == 0) {
                has_valid_binding = 1;
                entry->assigned = 1;
                log_trace("Named window binding 0x%lx validated by class/instance/type", entry->bound_x11_id);
            } else if (bound) {
                log_trace("Named window binding 0x%lx failed validation; clearing binding", entry->bound_x11_id);
                entry->bound_x11_id = 0;
                entry->assigned = 0;
                config_changed = 1;
            } else {
                log_trace("Named window binding 0x%lx is stale; clearing binding", entry->bound_x11_id);
                entry->bound_x11_id = 0;
                entry->assigned = 0;
                config_changed = 1;
            }
        }

        if (!has_valid_binding) {
            log_trace("Window 0x%lx with name '%s' is unbound or invalid, looking for replacement",
                      old_id, entry->custom_name);
            log_trace("Looking for: class='%s', instance='%s', type='%s', title='%s'",
                      entry->class_name, entry->instance, entry->type, entry->original_title);

            entry->assigned = 0;
            entry->bound_x11_id = 0;

            for (int j = 0; j < window_count; j++) {
                if (match_entry_is_bound_window(manager, windows[j].id)) {
                    log_trace("Window 0x%lx already has a custom name, skipping", windows[j].id);
                    continue;
                }

                log_trace("Checking window %d: class='%s', instance='%s', type='%s', title='%s'",
                          j, windows[j].class_name, windows[j].instance, windows[j].type, windows[j].title);

                if (window_matches_named_entry(&windows[j], entry)) {
                    entry->bound_x11_id = windows[j].id;
                    entry->assigned = 1;
                    config_changed = 1;
                    log_info("Automatically reassigned name '%s' from window 0x%lx to 0x%lx",
                             entry->custom_name, old_id, windows[j].id);
                    break;
                }
            }

            if (!entry->assigned) {
                log_trace("Could not find matching window for name '%s', marked as orphaned",
                          entry->custom_name);
            }
        }
    }
    
    if (config_changed) {
        log_debug("Named windows were automatically reassigned");
    }
    return config_changed;
}

int match_entry_collect_labeled_ids(const MatchEntryManager *manager, int *out, int max) {
    if (!manager || !out || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < manager->count && count < max; i++) {
        if (manager->entries[i].custom_name[0] == '\0' || manager->entries[i].match_id <= 0) {
            continue;
        }
        out[count++] = manager->entries[i].match_id;
    }

    return count;
}

int match_entry_collect_geom_ids(const MatchEntryManager *manager, int *out, int max) {
    if (!manager || !out || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < manager->count && count < max; i++) {
        if (!manager->entries[i].has_geom || manager->entries[i].match_id <= 0) {
            continue;
        }
        out[count++] = manager->entries[i].match_id;
    }

    return count;
}

int match_entry_gc(MatchEntryManager *manager, const int *referenced_ids, int referenced_count) {
    if (!manager) return 0;

    int removed = 0;
    for (int i = 0; i < manager->count; ) {
        MatchEntry *entry = &manager->entries[i];
        if (match_id_is_referenced(referenced_ids, referenced_count, entry->match_id)) {
            i++;
            continue;
        }

        log_info("GC removing unreferenced match entry %d (match_id=%d)",
                 i, entry->match_id);
        match_entry_delete_custom_name(manager, i);
        removed++;
    }

    return removed;
}

void match_entry_delete_custom_name(MatchEntryManager *manager, int index) {
    if (!manager || index < 0 || index >= manager->count) return;
    
    log_info("Deleting custom name '%s' for window 0x%lx",
            manager->entries[index].custom_name, manager->entries[index].bound_x11_id);
    
    // Move all entries after this one back by one position
    for (int i = index; i < manager->count - 1; i++) {
        manager->entries[i] = manager->entries[i + 1];
    }
    
    // Clear the last entry
    manager->entries[manager->count - 1].assigned = 0;
    manager->entries[manager->count - 1].match_id = 0;
    manager->entries[manager->count - 1].bound_x11_id = 0;
    
    manager->count--;
}

void match_entry_update_custom_name(MatchEntryManager *manager, int index, const char *new_name) {
    if (!manager || index < 0 || index >= manager->count || !new_name) return;
    
    log_info("Updating custom name from '%s' to '%s' for window 0x%lx",
            manager->entries[index].custom_name, new_name, manager->entries[index].bound_x11_id);
    
    safe_string_copy(manager->entries[index].custom_name, new_name, MAX_TITLE_LEN);
}

MatchEntry* match_entry_get_by_index(MatchEntryManager *manager, int index) {
    if (!manager || index < 0 || index >= manager->count) return NULL;
    return &manager->entries[index];
}

int match_entry_find_index_by_window(const MatchEntryManager *manager, Window id) {
    if (!manager || id == 0) return -1;

    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].bound_x11_id == id) {
            return i;
        }
    }
    return -1;
}

int match_entry_find_index_by_match_id(const MatchEntryManager *manager, int match_id) {
    if (!manager || match_id <= 0) return -1;
    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].match_id == match_id) {
            return i;
        }
    }
    return -1;
}

int match_entry_find_index_by_custom_name(const MatchEntryManager *manager, const char *custom_name) {
    if (!manager || !custom_name) return -1;

    for (int i = 0; i < manager->count; i++) {
        if (strcmp(manager->entries[i].custom_name, custom_name) == 0) {
            return i;
        }
    }
    return -1;
}

int matching_capture_or_get(MatchEntryManager *manager,
                            WindowInfo *windows,
                            int window_count,
                            const WindowInfo *w) {
    if (!manager || !w) return -1;

    // First pass: exact live-bound match to this window id.
    for (int i = 0; i < manager->count; i++) {
        MatchEntry *entry = &manager->entries[i];
        if (entry->assigned && entry->bound_x11_id == w->id) {
            return entry->match_id;
        }
    }

    // Second pass: dedup by exact criteria, skipping stale/other-live-bound entries.
    for (int i = 0; i < manager->count; i++) {
        MatchEntry *entry = &manager->entries[i];
        if (entry->assigned && entry->bound_x11_id != 0) {
            const WindowInfo *bound = find_live_window_by_id(windows, window_count,
                                                             entry->bound_x11_id);
            if (!bound) {
                // Stale binding: allow this entry to dedup by criteria for current capture.
                entry->assigned = 0;
                entry->bound_x11_id = 0;
            }
        }
        if (entry->assigned && entry->bound_x11_id != 0) {
            if (entry->bound_x11_id != w->id) continue; // currently bound to a different live window
        }

        if (entry->match_mode == TITLE_MATCH_MODE_EXACT &&
            strcmp(entry->class_name, w->class_name) == 0 &&
            strcmp(entry->instance, w->instance) == 0 &&
            strcmp(entry->type, w->type) == 0 &&
            strcmp(entry->original_title, w->title) == 0) {
            entry->bound_x11_id = w->id;
            entry->assigned = 1;
            return entry->match_id;
        }
    }

    if (manager->count >= MAX_WINDOWS) {
        return -1;
    }

    MatchEntry *entry = &manager->entries[manager->count++];
    memset(entry, 0, sizeof(*entry));
    entry->match_id = allocate_match_id(manager);
    entry->bound_x11_id = w->id;
    safe_string_copy(entry->original_title, w->title, MAX_TITLE_LEN);
    safe_string_copy(entry->class_name, w->class_name, MAX_CLASS_LEN);
    safe_string_copy(entry->instance, w->instance, MAX_CLASS_LEN);
    safe_string_copy(entry->type, w->type, sizeof(entry->type));
    entry->match_mode = TITLE_MATCH_MODE_EXACT;
    entry->assigned = 1;
    return entry->match_id;
}
