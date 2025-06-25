#include "harpoon.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include "log.h"
#include "window_matcher.h"
#include "utils.h"

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
    safe_string_copy(manager->slots[slot].title, window->title, MAX_TITLE_LEN);
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

static const char* get_config_path() {
    static char path[512];
    const char *home = getenv("HOME");
    if (!home) {
        home = ".";
    }
    
    // Create .config directory if it doesn't exist
    snprintf(path, sizeof(path), "%s/.config", home);
    mkdir(path, 0755);
    
    // Return full path to cofi.json
    snprintf(path, sizeof(path), "%s/.config/cofi.json", home);
    return path;
}

void save_harpoon_config(const HarpoonManager *manager) {
    if (!manager) return;
    
    const char *path = get_config_path();
    FILE *file = fopen(path, "w");
    if (!file) {
        log_error("Failed to open config file for writing: %s", path);
        return;
    }
    
    fprintf(file, "{\n");
    fprintf(file, "  \"harpoon_slots\": [\n");
    
    int first = 1;
    for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
        if (manager->slots[i].assigned) {
            if (!first) fprintf(file, ",\n");
            first = 0;
            
            fprintf(file, "    {\n");
            fprintf(file, "      \"slot\": %d,\n", i);
            fprintf(file, "      \"window_id\": %lu,\n", manager->slots[i].id);
            fprintf(file, "      \"title\": \"%s\",\n", manager->slots[i].title);
            fprintf(file, "      \"class_name\": \"%s\",\n", manager->slots[i].class_name);
            fprintf(file, "      \"instance\": \"%s\",\n", manager->slots[i].instance);
            fprintf(file, "      \"type\": \"%s\"\n", manager->slots[i].type);
            fprintf(file, "    }");
        }
    }
    
    fprintf(file, "\n  ]\n");
    fprintf(file, "}\n");
    
    fclose(file);
    log_info("Saved harpoon config to %s", path);
}

void load_harpoon_config(HarpoonManager *manager) {
    if (!manager) return;
    
    const char *path = get_config_path();
    FILE *file = fopen(path, "r");
    if (!file) {
        if (errno != ENOENT) {
            log_error("Failed to open config file for reading: %s", path);
        }
        return;
    }
    
    // For simplicity, we'll use a basic parser
    // In production, you'd want to use a proper JSON library
    char line[1024];
    int in_slot = 0;
    int slot = -1;
    HarpoonSlot temp_slot = {0};
    
    while (fgets(line, sizeof(line), file)) {
        // Skip whitespace
        char *p = line;
        while (*p && (*p == ' ' || *p == '\t')) p++;
        
        if (strstr(p, "\"slot\":")) {
            sscanf(p, " \"slot\": %d", &slot);
        } else if (strstr(p, "\"window_id\":")) {
            sscanf(p, " \"window_id\": %lu", &temp_slot.id);
        } else if (strstr(p, "\"title\":")) {
            char *colon = strchr(p, ':');
            if (colon) {
                char *start = strchr(colon + 1, '"');
                if (start) {
                    start++;  // Move past the opening quote
                    char *end = strchr(start, '"');
                    if (end) {
                        int len = end - start;
                        if (len >= MAX_TITLE_LEN) len = MAX_TITLE_LEN - 1;
                        safe_string_copy(temp_slot.title, start, len + 1);
                    }
                }
            }
        } else if (strstr(p, "\"class_name\":")) {
            char *colon = strchr(p, ':');
            if (colon) {
                char *start = strchr(colon + 1, '"');
                if (start) {
                    start++;  // Move past the opening quote
                    char *end = strchr(start, '"');
                    if (end) {
                        int len = end - start;
                        if (len >= MAX_CLASS_LEN) len = MAX_CLASS_LEN - 1;
                        safe_string_copy(temp_slot.class_name, start, len + 1);
                    }
                }
            }
        } else if (strstr(p, "\"instance\":")) {
            char *colon = strchr(p, ':');
            if (colon) {
                char *start = strchr(colon + 1, '"');
                if (start) {
                    start++;  // Move past the opening quote
                    char *end = strchr(start, '"');
                    if (end) {
                        int len = end - start;
                        if (len >= MAX_CLASS_LEN) len = MAX_CLASS_LEN - 1;
                        safe_string_copy(temp_slot.instance, start, len + 1);
                    }
                }
            }
        } else if (strstr(p, "\"type\":")) {
            char *colon = strchr(p, ':');
            if (colon) {
                char *start = strchr(colon + 1, '"');
                if (start) {
                    start++;  // Move past the opening quote
                    char *end = strchr(start, '"');
                    if (end) {
                        int len = end - start;
                        if (len >= 15) len = 15;
                        safe_string_copy(temp_slot.type, start, len + 1);
                    }
                }
            }
        } else if (strchr(p, '}') && slot >= 0 && slot < MAX_HARPOON_SLOTS) {
            // End of slot object
            temp_slot.assigned = 1;
            manager->slots[slot] = temp_slot;
            log_debug("Loaded slot %d: window 0x%lx, title '%s'", slot, temp_slot.id, temp_slot.title);
            
            // Reset for next slot
            slot = -1;
            memset(&temp_slot, 0, sizeof(temp_slot));
        }
    }
    
    fclose(file);
    log_info("Loaded harpoon config from %s", path);
}

void check_and_reassign_windows(HarpoonManager *manager, WindowInfo *windows, int window_count) {
    if (!manager || !windows) return;
    
    log_debug("check_and_reassign_windows: checking %d windows against %d slots", 
             window_count, MAX_HARPOON_SLOTS);
    
    int config_changed = 0;  // Track if we need to save config
    
    // Check each harpoon slot
    for (int slot = 0; slot < MAX_HARPOON_SLOTS; slot++) {
        if (!manager->slots[slot].assigned) continue;
        
        log_debug("Checking slot %d: has window 0x%lx (%s)", 
                 slot, manager->slots[slot].id, manager->slots[slot].title);
        
        // Check if the window ID is still valid
        int window_still_exists = 0;
        for (int i = 0; i < window_count; i++) {
            if (windows[i].id == manager->slots[slot].id) {
                window_still_exists = 1;
                log_debug("Slot %d window 0x%lx still exists in current window list", 
                         slot, manager->slots[slot].id);
                break;
            }
        }
        
        // If window doesn't exist anymore, try to find a matching window
        if (!window_still_exists) {
            log_debug("Window 0x%lx in slot %d no longer exists, looking for replacement", 
                     manager->slots[slot].id, slot);
            log_debug("Looking for: class='%s', instance='%s', type='%s', title='%s'",
                     manager->slots[slot].class_name, manager->slots[slot].instance,
                     manager->slots[slot].type, manager->slots[slot].title);
            
            // Convert slot to WindowInfo for easier comparison
            WindowInfo slot_window;
            slot_window.id = manager->slots[slot].id;
            strcpy(slot_window.title, manager->slots[slot].title);
            strcpy(slot_window.class_name, manager->slots[slot].class_name);
            strcpy(slot_window.instance, manager->slots[slot].instance);
            strcpy(slot_window.type, manager->slots[slot].type);
            
            // First, look for exact match
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
                
                log_debug("Checking window %d: class='%s', instance='%s', type='%s', title='%s'",
                         i, windows[i].class_name, windows[i].instance, windows[i].type, windows[i].title);
                
                // Use pure function for exact match
                if (windows_match_exact(&slot_window, &windows[i])) {
                    // Found a match! Reassign the slot
                    Window old_id = manager->slots[slot].id;
                    manager->slots[slot].id = windows[i].id;
                    config_changed = 1;
                    log_info("Automatically reassigned slot %d from window 0x%lx to 0x%lx (exact match: %s)",
                            slot, old_id, windows[i].id, windows[i].title);
                    break;
                }
            }
            
            // If no exact match found, try fuzzy matching
            if (!window_still_exists && manager->slots[slot].id != 0) {
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
                    
                    // Use pure function for fuzzy match
                    if (windows_match_fuzzy(&slot_window, &windows[i])) {
                        // Found a fuzzy match!
                        Window old_id = manager->slots[slot].id;
                        manager->slots[slot].id = windows[i].id;
                        // Update the stored title to the new one
                        safe_string_copy(manager->slots[slot].title, windows[i].title, MAX_TITLE_LEN);
                        config_changed = 1;
                        log_info("Automatically reassigned slot %d from window 0x%lx to 0x%lx (fuzzy match: %s)",
                                slot, old_id, windows[i].id, windows[i].title);
                        break;
                    }
                }
            }
        }
    }
    
    // Save config if any reassignments were made
    if (config_changed) {
        save_harpoon_config(manager);
        log_debug("Saved updated harpoon config after automatic reassignment");
    }
}