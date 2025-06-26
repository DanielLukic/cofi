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

// Save both harpoon slots and window position
void save_config_with_position(const HarpoonManager *manager, int has_position, int x, int y) {
    if (!manager) return;
    
    const char *path = get_config_path();
    FILE *file = fopen(path, "w");
    if (!file) {
        log_error("Failed to open config file for writing: %s", path);
        return;
    }
    
    fprintf(file, "{\n");
    
    // Save window position
    fprintf(file, "  \"window_position\": {\n");
    fprintf(file, "    \"x\": %d,\n", x);
    fprintf(file, "    \"y\": %d,\n", y);
    fprintf(file, "    \"saved\": %s\n", has_position ? "true" : "false");
    fprintf(file, "  },\n");
    
    // Save harpoon slots
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
    log_info("Saved config with position to %s", path);
}

void save_harpoon_config(const HarpoonManager *manager) {
    // Use save_config_with_position but don't save position (saved = false)
    save_config_with_position(manager, 0, 0, 0);
}

static const char* alignment_to_string(WindowAlignment align) {
    switch (align) {
        case ALIGN_CENTER: return "center";
        case ALIGN_TOP: return "top";
        case ALIGN_TOP_LEFT: return "top_left";
        case ALIGN_TOP_RIGHT: return "top_right";
        case ALIGN_LEFT: return "left";
        case ALIGN_RIGHT: return "right";
        case ALIGN_BOTTOM: return "bottom";
        case ALIGN_BOTTOM_LEFT: return "bottom_left";
        case ALIGN_BOTTOM_RIGHT: return "bottom_right";
        default: return "center";
    }
}

static WindowAlignment string_to_alignment(const char *str) {
    if (!str) return ALIGN_CENTER;
    if (strcmp(str, "top") == 0) return ALIGN_TOP;
    if (strcmp(str, "top_left") == 0) return ALIGN_TOP_LEFT;
    if (strcmp(str, "top_right") == 0) return ALIGN_TOP_RIGHT;
    if (strcmp(str, "left") == 0) return ALIGN_LEFT;
    if (strcmp(str, "right") == 0) return ALIGN_RIGHT;
    if (strcmp(str, "bottom") == 0) return ALIGN_BOTTOM;
    if (strcmp(str, "bottom_left") == 0) return ALIGN_BOTTOM_LEFT;
    if (strcmp(str, "bottom_right") == 0) return ALIGN_BOTTOM_RIGHT;
    return ALIGN_CENTER;
}

// Save full config with all options
void save_full_config(const HarpoonManager *manager, int has_position, int x, int y, 
                      int close_on_focus_loss, WindowAlignment align) {
    if (!manager) return;
    
    const char *path = get_config_path();
    FILE *file = fopen(path, "w");
    if (!file) {
        log_error("Failed to open config file for writing: %s", path);
        return;
    }
    
    fprintf(file, "{\n");
    
    // Save options
    fprintf(file, "  \"options\": {\n");
    fprintf(file, "    \"close_on_focus_loss\": %s,\n", close_on_focus_loss ? "true" : "false");
    fprintf(file, "    \"align\": \"%s\"\n", alignment_to_string(align));
    fprintf(file, "  },\n");
    
    // Save window position
    fprintf(file, "  \"window_position\": {\n");
    fprintf(file, "    \"x\": %d,\n", x);
    fprintf(file, "    \"y\": %d,\n", y);
    fprintf(file, "    \"saved\": %s\n", has_position ? "true" : "false");
    fprintf(file, "  },\n");
    
    // Save harpoon slots
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
    log_info("Saved full config to %s", path);
}

// Load both harpoon slots and window position
void load_config_with_position(HarpoonManager *manager, int *has_position, int *x, int *y) {
    if (!manager) return;
    
    // Initialize output parameters
    if (has_position) *has_position = 0;
    if (x) *x = 0;
    if (y) *y = 0;
    
    const char *path = get_config_path();
    FILE *file = fopen(path, "r");
    if (!file) {
        if (errno != ENOENT) {
            log_error("Failed to open config file for reading: %s", path);
        }
        return;
    }
    
    // For simplicity, we'll use a basic parser
    char line[1024];
    int in_position = 0;
    int in_slots = 0;
    int slot = -1;
    HarpoonSlot temp_slot = {0};
    
    while (fgets(line, sizeof(line), file)) {
        // Skip whitespace
        char *p = line;
        while (*p && (*p == ' ' || *p == '\t')) p++;
        
        // Check for window_position section
        if (strstr(p, "\"window_position\":")) {
            in_position = 1;
            in_slots = 0;
        } else if (strstr(p, "\"harpoon_slots\":")) {
            in_position = 0;
            in_slots = 1;
        }
        
        // Parse window position
        if (in_position) {
            if (strstr(p, "\"x\":") && x) {
                sscanf(p, " \"x\": %d", x);
            } else if (strstr(p, "\"y\":") && y) {
                sscanf(p, " \"y\": %d", y);
            } else if (strstr(p, "\"saved\":") && has_position) {
                if (strstr(p, "true")) {
                    *has_position = 1;
                }
            }
        }
        
        // Parse harpoon slots
        if (in_slots) {
            if (strstr(p, "\"slot\":")) {
                if (sscanf(p, " \"slot\": %d", &slot) != 1) {
                    log_warn("Failed to parse slot number from JSON");
                }
            } else if (strstr(p, "\"window_id\":")) {
                if (sscanf(p, " \"window_id\": %lu", &temp_slot.id) != 1) {
                    log_warn("Failed to parse window_id from JSON");
                }
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
                            if (len >= sizeof(temp_slot.type)) len = sizeof(temp_slot.type) - 1;
                            safe_string_copy(temp_slot.type, start, len + 1);
                        }
                    }
                }
            } else if (strstr(p, "}") && slot >= 0 && slot < MAX_HARPOON_SLOTS) {
                // End of slot entry, save it
                manager->slots[slot] = temp_slot;
                manager->slots[slot].assigned = 1;
                slot = -1;
                memset(&temp_slot, 0, sizeof(temp_slot));
            }
        }
    }
    
    fclose(file);
    log_info("Loaded config from %s", path);
}

void load_harpoon_config(HarpoonManager *manager) {
    // Use load_config_with_position but ignore position
    load_config_with_position(manager, NULL, NULL, NULL);
}

// Load full config with all options
void load_full_config(HarpoonManager *manager, int *has_position, int *x, int *y,
                      int *close_on_focus_loss, WindowAlignment *align) {
    if (!manager) return;
    
    // Initialize output parameters with defaults
    if (has_position) *has_position = 0;
    if (x) *x = 0;
    if (y) *y = 0;
    if (close_on_focus_loss) *close_on_focus_loss = 1;  // Default to true
    if (align) *align = ALIGN_CENTER;  // Default to center
    
    const char *path = get_config_path();
    FILE *file = fopen(path, "r");
    if (!file) {
        if (errno != ENOENT) {
            log_error("Failed to open config file for reading: %s", path);
        }
        return;
    }
    
    // For simplicity, we'll use a basic parser
    char line[1024];
    int in_options = 0;
    int in_position = 0;
    int in_slots = 0;
    int slot = -1;
    HarpoonSlot temp_slot = {0};
    char align_str[32] = {0};
    
    while (fgets(line, sizeof(line), file)) {
        // Skip whitespace
        char *p = line;
        while (*p && (*p == ' ' || *p == '\t')) p++;
        
        // Check for sections
        if (strstr(p, "\"options\":")) {
            in_options = 1;
            in_position = 0;
            in_slots = 0;
        } else if (strstr(p, "\"window_position\":")) {
            in_options = 0;
            in_position = 1;
            in_slots = 0;
        } else if (strstr(p, "\"harpoon_slots\":")) {
            in_options = 0;
            in_position = 0;
            in_slots = 1;
        }
        
        // Parse options
        if (in_options) {
            if (strstr(p, "\"close_on_focus_loss\":") && close_on_focus_loss) {
                if (strstr(p, "true")) {
                    *close_on_focus_loss = 1;
                } else if (strstr(p, "false")) {
                    *close_on_focus_loss = 0;
                }
            } else if (strstr(p, "\"align\":") && align) {
                char *colon = strchr(p, ':');
                if (colon) {
                    char *start = strchr(colon + 1, '"');
                    if (start) {
                        start++;  // Move past the opening quote
                        char *end = strchr(start, '"');
                        if (end) {
                            int len = end - start;
                            if (len >= 32) len = 31;
                            strncpy(align_str, start, len);
                            align_str[len] = '\0';
                            *align = string_to_alignment(align_str);
                        }
                    }
                }
            }
        }
        
        // Parse window position
        if (in_position) {
            if (strstr(p, "\"x\":") && x) {
                sscanf(p, " \"x\": %d", x);
            } else if (strstr(p, "\"y\":") && y) {
                sscanf(p, " \"y\": %d", y);
            } else if (strstr(p, "\"saved\":") && has_position) {
                if (strstr(p, "true")) {
                    *has_position = 1;
                }
            }
        }
        
        // Parse harpoon slots (same as before)
        if (in_slots) {
            if (strstr(p, "\"slot\":")) {
                if (sscanf(p, " \"slot\": %d", &slot) != 1) {
                    log_warn("Failed to parse slot number from JSON");
                }
            } else if (strstr(p, "\"window_id\":")) {
                if (sscanf(p, " \"window_id\": %lu", &temp_slot.id) != 1) {
                    log_warn("Failed to parse window_id from JSON");
                }
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
                            if (len >= 16) len = 15;
                            safe_string_copy(temp_slot.type, start, len + 1);
                        }
                    }
                }
            } else if (strstr(p, "}")) {
                // End of a slot object, save it if we have a valid slot
                if (slot >= 0 && slot < MAX_HARPOON_SLOTS && temp_slot.id != 0) {
                    manager->slots[slot] = temp_slot;
                    manager->slots[slot].assigned = 1;
                    // Reset temp_slot for next iteration
                    memset(&temp_slot, 0, sizeof(temp_slot));
                    slot = -1;
                }
            }
        }
    }
    
    fclose(file);
    log_info("Loaded full config from %s", path);
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