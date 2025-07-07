#include "harpoon.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include "log.h"
#include "window_matcher.h"
#include "utils.h"
#include "app_data.h"  // For WindowAlignment

void init_harpoon_manager(HarpoonManager *manager) {
    if (!manager) return;
    
    // Initialize all slots as unassigned
    for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
        manager->slots[i].assigned = 0;
        manager->slots[i].id = 0;
        manager->slots[i].title[0] = '\0';
        manager->slots[i].class_name[0] = '\0';
        manager->slots[i].instance[0] = '\0';
        manager->slots[i].type[0] = '\0';
    }
}

void assign_window_to_slot(HarpoonManager *manager, int slot, const WindowInfo *window) {
    if (!manager || !window || slot < 0 || slot >= MAX_HARPOON_SLOTS) return;
    
    // Store window information in the slot
    manager->slots[slot].id = window->id;
    
    // Copy title, replacing '*' with '.' to escape wildcards
    const char *src = window->title;
    char *dst = manager->slots[slot].title;
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
    
    safe_string_copy(manager->slots[slot].class_name, window->class_name, MAX_CLASS_LEN);
    safe_string_copy(manager->slots[slot].instance, window->instance, MAX_CLASS_LEN);
    safe_string_copy(manager->slots[slot].type, window->type, 16);
    manager->slots[slot].assigned = 1;
}

void unassign_slot(HarpoonManager *manager, int slot) {
    if (!manager || slot < 0 || slot >= MAX_HARPOON_SLOTS) return;
    
    manager->slots[slot].assigned = 0;
    manager->slots[slot].id = 0;
}

int get_window_slot(const HarpoonManager *manager, Window id) {
    if (!manager || id == 0) return -1;
    
    for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
        if (manager->slots[i].assigned && manager->slots[i].id == id) {
            return i;
        }
    }
    return -1;
}

Window get_slot_window(const HarpoonManager *manager, int slot) {
    if (!manager || slot < 0 || slot >= MAX_HARPOON_SLOTS) return 0;
    
    if (manager->slots[slot].assigned) {
        return manager->slots[slot].id;
    }
    return 0;
}

int is_slot_assigned(const HarpoonManager *manager, int slot) {
    if (!manager || slot < 0 || slot >= MAX_HARPOON_SLOTS) return 0;
    
    return manager->slots[slot].assigned;
}










void check_and_reassign_windows(HarpoonManager *manager, WindowInfo *windows, int window_count) {
    if (!manager || !windows) return;
    
    log_trace("check_and_reassign_windows: checking %d windows against %d slots",
             window_count, MAX_HARPOON_SLOTS);
    
    int config_changed = 0;  // Track if we need to save config
    
    // Check each harpoon slot
    for (int slot = 0; slot < MAX_HARPOON_SLOTS; slot++) {
        if (!manager->slots[slot].assigned) continue;
        
        log_trace("Checking slot %d: has window 0x%lx (%s)",
                 slot, manager->slots[slot].id, manager->slots[slot].title);

        // Check if the window ID is still valid
        int window_still_exists = 0;
        for (int i = 0; i < window_count; i++) {
            if (windows[i].id == manager->slots[slot].id) {
                window_still_exists = 1;
                log_trace("Slot %d window 0x%lx still exists in current window list",
                         slot, manager->slots[slot].id);
                break;
            }
        }
        
        // If window doesn't exist anymore, try to find a matching window
        if (!window_still_exists) {
            log_trace("Window 0x%lx in slot %d no longer exists, looking for replacement",
                     manager->slots[slot].id, slot);
            log_trace("Looking for: class='%s', instance='%s', type='%s', title='%s'",
                     manager->slots[slot].class_name, manager->slots[slot].instance,
                     manager->slots[slot].type, manager->slots[slot].title);
            
            // Look for a matching window using wildcard support
            for (int i = 0; i < window_count; i++) {
                // Skip if this window is already assigned to a slot
                int already_assigned = 0;
                for (int j = 0; j < MAX_HARPOON_SLOTS; j++) {
                    if (manager->slots[j].assigned && manager->slots[j].id == windows[i].id) {
                        already_assigned = 1;
                        break;
                    }
                }
                if (already_assigned) continue;
                
                log_trace("Checking window %d: class='%s', instance='%s', type='%s', title='%s'",
                         i, windows[i].class_name, windows[i].instance, windows[i].type, windows[i].title);
                
                // Use wildcard matching
                if (window_matches_harpoon_slot(&windows[i], &manager->slots[slot])) {
                    // Found a match! Reassign the slot
                    Window old_id = manager->slots[slot].id;
                    manager->slots[slot].id = windows[i].id;
                    config_changed = 1;
                    log_info("Automatically reassigned slot %d from window 0x%lx to 0x%lx (wildcard match: %s)",
                            slot, old_id, windows[i].id, windows[i].title);
                    break;
                }
            }
        }
    }
    
    // Note: Config will be saved by the calling code when appropriate
    if (config_changed) {
        log_debug("Harpoon slots were automatically reassigned");
    }
}