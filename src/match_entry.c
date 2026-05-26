#include "match_entry.h"
#include <string.h>
#include <stdio.h>
#include "log.h"
#include "window_matcher.h"
#include "utils.h"

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
        manager->entries[i].custom_name[0] = '\0';
        manager->entries[i].original_title[0] = '\0';
    }
}

void match_entry_assign_custom_name(MatchEntryManager *manager, const WindowInfo *window, const char *custom_name) {
    if (!manager || !window || !custom_name || strlen(custom_name) == 0) return;

    for (int i = 0; i < manager->count; i++) {
        MatchEntry *entry = &manager->entries[i];
        if (entry->custom_name[0] == '\0') continue;
        if (!match_entry_matches_window(entry, window)) continue;
        safe_string_copy(entry->custom_name, custom_name, MAX_TITLE_LEN);
        entry->bound_x11_id = window->id;
        entry->assigned = 1;
        log_info("Updated custom name for window 0x%lx to '%s'", window->id, custom_name);
        return;
    }

    int match_id = matching_create_entry(manager, window);
    if (match_id <= 0) {
        log_error("Cannot add more named windows, limit reached");
        return;
    }

    int idx = match_entry_find_index_by_match_id(manager, match_id);
    if (idx >= 0) {
        MatchEntry *entry = &manager->entries[idx];
        safe_string_copy(entry->custom_name, custom_name, MAX_TITLE_LEN);
        entry->bound_x11_id = window->id;
        entry->assigned = 1;
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

        // Persisted X11 ids are not trusted across restarts; always rebind by title pattern.
        if (entry->bound_x11_id != 0) {
            const WindowInfo *bound = find_live_window_by_id(windows, window_count, entry->bound_x11_id);
            if (bound && match_entry_matches_window(entry, bound)) {
                has_valid_binding = 1;
                entry->assigned = 1;
            } else {
                entry->bound_x11_id = 0;
                entry->assigned = 0;
                config_changed = 1;
            }
        }

        if (!has_valid_binding) {
            log_trace("Window 0x%lx with name '%s' is unbound or invalid, looking for replacement",
                      old_id, entry->custom_name);
            log_trace("Looking for title pattern '%s'",
                      entry->original_title);

            entry->assigned = 0;
            entry->bound_x11_id = 0;

            for (int j = 0; j < window_count; j++) {
                if (match_entry_is_bound_window(manager, windows[j].id)) {
                    log_trace("Window 0x%lx already has a custom name, skipping", windows[j].id);
                    continue;
                }

                if (match_entry_matches_window(entry, &windows[j])) {
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
    manager->entries[manager->count - 1].class_name[0] = '\0';
    manager->entries[manager->count - 1].instance[0] = '\0';
    manager->entries[manager->count - 1].type[0] = '\0';
    
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
    return entry->match_id;
}

int matching_find_or_create_pattern_entry(MatchEntryManager *manager, const char *pattern) {
    if (!manager || !pattern || pattern[0] == '\0') return -1;

    for (int i = 0; i < manager->count; i++) {
        MatchEntry *entry = &manager->entries[i];
        if (entry->class_name[0] != '\0' || entry->instance[0] != '\0' || entry->type[0] != '\0') {
            continue;
        }
        if (strcmp(entry->original_title, pattern) != 0) {
            continue;
        }
        return entry->match_id;
    }

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
