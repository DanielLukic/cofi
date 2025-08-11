#include "named_window.h"
#include <string.h>
#include <stdio.h>
#include "log.h"
#include "window_matcher.h"
#include "utils.h"

void init_named_window_manager(NamedWindowManager *manager) {
    if (!manager) return;
    
    manager->count = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        manager->entries[i].assigned = 0;
        manager->entries[i].id = 0;
        manager->entries[i].custom_name[0] = '\0';
        manager->entries[i].original_title[0] = '\0';
        manager->entries[i].class_name[0] = '\0';
        manager->entries[i].instance[0] = '\0';
        manager->entries[i].type[0] = '\0';
    }
}

void assign_custom_name(NamedWindowManager *manager, const WindowInfo *window, const char *custom_name) {
    if (!manager || !window || !custom_name || strlen(custom_name) == 0) return;
    
    // Check if window already has a name
    int existing_idx = find_named_window_index(manager, window->id);
    
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
        
        NamedWindow *entry = &manager->entries[manager->count];
        entry->id = window->id;
        safe_string_copy(entry->custom_name, custom_name, MAX_TITLE_LEN);
        
        // Store original title with '*' replaced by '.' for wildcard matching
        const char *src = window->title;
        char *dst = entry->original_title;
        size_t i = 0;
        
        while (*src && i < MAX_TITLE_LEN - 1) {
            if (*src == '*') {
                dst[i] = '.';
            } else {
                dst[i] = *src;
            }
            src++;
            i++;
        }
        dst[i] = '\0';
        
        safe_string_copy(entry->class_name, window->class_name, MAX_CLASS_LEN);
        safe_string_copy(entry->instance, window->instance, MAX_CLASS_LEN);
        safe_string_copy(entry->type, window->type, 16);
        entry->assigned = 1;
        
        manager->count++;
        log_info("Assigned custom name '%s' to window 0x%lx", custom_name, window->id);
    }
}

const char* get_window_custom_name(const NamedWindowManager *manager, Window id) {
    if (!manager || id == 0) return NULL;
    
    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].id == id && manager->entries[i].assigned) {
            return manager->entries[i].custom_name;
        }
    }
    return NULL;
}

int is_window_already_named(const NamedWindowManager *manager, Window id) {
    return get_window_custom_name(manager, id) != NULL;
}

// Helper function to check if a window matches a named window entry
static int window_matches_named_entry(const WindowInfo *window, const NamedWindow *entry) {
    if (!window || !entry) return 0;
    
    // Class and instance must match exactly
    if (strcmp(window->class_name, entry->class_name) != 0 ||
        strcmp(window->instance, entry->instance) != 0 ||
        strcmp(window->type, entry->type) != 0) {
        return 0;
    }
    
    // Title can use wildcard matching (using same logic as harpoon)
    return wildcard_match(entry->original_title, window->title);
}

bool check_and_reassign_names(NamedWindowManager *manager, WindowInfo *windows, int window_count) {
    if (!manager || !windows) return false;
    
    log_trace("check_and_reassign_names: checking %d windows against %d named entries",
             window_count, manager->count);
    
    int config_changed = 0;
    
    // Check each named window entry
    for (int i = 0; i < manager->count; i++) {
        NamedWindow *entry = &manager->entries[i];
        
        // Skip if not previously assigned (deleted entries)
        if (!entry->assigned) continue;
        
        log_trace("Checking named entry %d: has window 0x%lx (%s)", 
                 i, entry->id, entry->custom_name);
        
        // Check if the window ID still exists
        int window_still_exists = 0;
        for (int j = 0; j < window_count; j++) {
            if (windows[j].id == entry->id) {
                window_still_exists = 1;
                log_trace("Named window 0x%lx still exists", entry->id);
                break;
            }
        }
        
        // If window doesn't exist anymore, try to find a matching window
        if (!window_still_exists) {
            log_trace("Window 0x%lx with name '%s' no longer exists, looking for replacement",
                     entry->id, entry->custom_name);
            log_trace("Looking for: class='%s', instance='%s', type='%s', title='%s'",
                     entry->class_name, entry->instance, entry->type, entry->original_title);
            
            // Mark as orphaned first
            Window old_id = entry->id;
            entry->assigned = 0;
            
            // Look for a matching window
            for (int j = 0; j < window_count; j++) {
                // CRITICAL: Skip if this window already has a custom name
                if (is_window_already_named(manager, windows[j].id)) {
                    log_trace("Window 0x%lx already has a custom name, skipping", windows[j].id);
                    continue;
                }
                
                log_trace("Checking window %d: class='%s', instance='%s', type='%s', title='%s'",
                         j, windows[j].class_name, windows[j].instance, windows[j].type, windows[j].title);
                
                // Use wildcard matching
                if (window_matches_named_entry(&windows[j], entry)) {
                    // Found a match! Reassign the name
                    entry->id = windows[j].id;
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

void delete_custom_name(NamedWindowManager *manager, int index) {
    if (!manager || index < 0 || index >= manager->count) return;
    
    log_info("Deleting custom name '%s' for window 0x%lx",
            manager->entries[index].custom_name, manager->entries[index].id);
    
    // Move all entries after this one back by one position
    for (int i = index; i < manager->count - 1; i++) {
        manager->entries[i] = manager->entries[i + 1];
    }
    
    // Clear the last entry
    manager->entries[manager->count - 1].assigned = 0;
    manager->entries[manager->count - 1].id = 0;
    
    manager->count--;
}

void update_custom_name(NamedWindowManager *manager, int index, const char *new_name) {
    if (!manager || index < 0 || index >= manager->count || !new_name) return;
    
    log_info("Updating custom name from '%s' to '%s' for window 0x%lx",
            manager->entries[index].custom_name, new_name, manager->entries[index].id);
    
    safe_string_copy(manager->entries[index].custom_name, new_name, MAX_TITLE_LEN);
}

NamedWindow* get_named_window_by_index(NamedWindowManager *manager, int index) {
    if (!manager || index < 0 || index >= manager->count) return NULL;
    return &manager->entries[index];
}

int find_named_window_index(const NamedWindowManager *manager, Window id) {
    if (!manager || id == 0) return -1;
    
    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].id == id) {
            return i;
        }
    }
    return -1;
}