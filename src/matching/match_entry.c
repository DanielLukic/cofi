#include "matching/match_entry.h"
#include <string.h>
#include <stdio.h>
#include "core/log/log.h"
#include "matching/window_matcher.h"
#include "core/utils/utils.h"

static void escape_title_wildcards(char *dst, const char *src, size_t dst_size) {
    // On capture, replace literal '*' in window titles with '.' so captured
    // patterns do not broaden into wildcard '*' matches. User-edited patterns
    // are saved raw (no inverse conversion).
    if (!dst || dst_size == 0) return;
    dst[0] = '\0';
    if (!src) return;

    size_t i = 0;
    while (src[i] != '\0' && i + 1 < dst_size) {
        dst[i] = (src[i] == '*') ? '.' : src[i];
        i++;
    }
    dst[i] = '\0';
}

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
        manager->entries[i].class_name[0] = '\0';
        manager->entries[i].instance[0] = '\0';
        manager->entries[i].type[0] = '\0';
        manager->entries[i].original_title[0] = '\0';
    }
}

int match_entry_is_bound_window(const MatchEntryManager *manager, Window id) {
    return match_entry_find_index_by_window(manager, id) >= 0;
}

bool match_entry_matches_window(const MatchEntry *entry, const WindowInfo *window) {
    if (!entry || !window) return false;
    if (!wildcard_match(entry->original_title, window->title)) {
        return false;
    }

    if (entry->class_name[0] != '\0' && strcmp(entry->class_name, window->class_name) != 0) {
        return false;
    }
    if (entry->instance[0] != '\0' && strcmp(entry->instance, window->instance) != 0) {
        return false;
    }
    if (entry->type[0] != '\0' && strcmp(entry->type, window->type) != 0) {
        return false;
    }

    return true;
}

bool match_entry_reassign_live_windows(MatchEntryManager *manager, WindowInfo *windows, int window_count) {
    if (!manager || !windows) return false;
    
    log_trace("match_entry_reassign_live_windows: checking %d windows against %d entries",
             window_count, manager->count);
    
    int config_changed = 0;
    
    // Every entry in [0, count) is live. assigned=0 means currently unbound.
    for (int i = 0; i < manager->count; i++) {
        MatchEntry *entry = &manager->entries[i];

        log_trace("Checking match entry %d: bound 0x%lx (%s), assigned=%d",
                  i, entry->bound_x11_id, entry->original_title, entry->assigned);
        Window old_id = entry->bound_x11_id;
        int has_valid_binding = 0;

        // At runtime an established binding sticks as long as the window is alive; title pattern is the fallback only when bound_x11_id is dead.
        if (entry->bound_x11_id != 0) {
            const WindowInfo *bound = find_live_window_by_id(windows, window_count, entry->bound_x11_id);
            if (bound) {
                has_valid_binding = 1;
                entry->assigned = 1;
            } else {
                entry->bound_x11_id = 0;
                entry->assigned = 0;
                config_changed = 1;
            }
        }

        if (!has_valid_binding) {
            log_trace("Window 0x%lx for pattern '%s' is unbound or invalid, looking for replacement",
                      old_id, entry->original_title);
            log_trace("Looking for title pattern '%s'",
                      entry->original_title);

            entry->assigned = 0;
            entry->bound_x11_id = 0;

            for (int j = 0; j < window_count; j++) {
                if (match_entry_matches_window(entry, &windows[j])) {
                    entry->bound_x11_id = windows[j].id;
                    entry->assigned = 1;
                    config_changed = 1;
                    log_info("Automatically rebound match entry %d from window 0x%lx to 0x%lx",
                             entry->match_id, old_id, windows[j].id);
                    break;
                }
            }

            if (!entry->assigned) {
                log_trace("Could not find matching window for match entry %d, marked as orphaned",
                          entry->match_id);
            }
        }
    }
    
    if (config_changed) {
        log_debug("Match entries were automatically rebound");
    }
    return config_changed;
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
        match_entry_delete(manager, i);
        removed++;
    }

    return removed;
}

void match_entry_delete(MatchEntryManager *manager, int index) {
    if (!manager || index < 0 || index >= manager->count) return;
    
    log_info("Deleting match entry %d for window 0x%lx",
            manager->entries[index].match_id, manager->entries[index].bound_x11_id);
    
    for (int i = index; i < manager->count - 1; i++) {
        manager->entries[i] = manager->entries[i + 1];
    }
    
    // Clear the last entry
    manager->entries[manager->count - 1].assigned = 0;
    manager->entries[manager->count - 1].match_id = 0;
    manager->entries[manager->count - 1].bound_x11_id = 0;
    manager->entries[manager->count - 1].class_name[0] = '\0';
    manager->entries[manager->count - 1].instance[0] = '\0';
    manager->entries[manager->count - 1].type[0] = '\0';
    manager->entries[manager->count - 1].original_title[0] = '\0';
    
    manager->count--;
}

void match_entry_delete_by_match_id(MatchEntryManager *manager, int match_id) {
    if (!manager || match_id <= 0) return;
    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].match_id == match_id) {
            match_entry_delete(manager, i);
            return;
        }
    }
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

int matching_create_entry(MatchEntryManager *manager, const WindowInfo *w) {
    if (!manager || !w) return -1;

    if (manager->count >= MAX_WINDOWS) {
        return -1;
    }

    MatchEntry *entry = &manager->entries[manager->count++];
    memset(entry, 0, sizeof(*entry));
    entry->match_id = allocate_match_id(manager);
    entry->bound_x11_id = w->id;
    safe_string_copy(entry->class_name, w->class_name, MAX_CLASS_LEN);
    safe_string_copy(entry->instance, w->instance, MAX_CLASS_LEN);
    safe_string_copy(entry->type, w->type, sizeof(entry->type));
    escape_title_wildcards(entry->original_title, w->title, MAX_TITLE_LEN);
    entry->assigned = 1;
    if (!match_entry_matches_window(entry, w)) {
        log_warn("matching: freshly-created entry %d does not match source window 0x%lx (class='%s' title='%s')",
                 entry->match_id, w->id, w->class_name, w->title);
    }
    return entry->match_id;
}

int matching_create_pattern_entry(MatchEntryManager *manager, const char *pattern) {
    if (!manager || !pattern || pattern[0] == '\0') return -1;

    if (manager->count >= MAX_WINDOWS) {
        return -1;
    }

    MatchEntry *entry = &manager->entries[manager->count++];
    memset(entry, 0, sizeof(*entry));
    entry->match_id = allocate_match_id(manager);
    entry->bound_x11_id = 0;
    safe_string_copy(entry->original_title, pattern, MAX_TITLE_LEN);
    entry->assigned = 0;
    return entry->match_id;
}
