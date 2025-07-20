#include "harpoon_config.h"
#include "log.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

// Helper function to get harpoon config file path
static const char* get_harpoon_config_path() {
    static char path[512];
    const char *home = getenv("HOME");
    if (!home) {
        home = ".";
    }
    
    // Create .config directory if it doesn't exist
    snprintf(path, sizeof(path), "%s/.config", home);
    mkdir(path, 0755);
    
    // Create .config/cofi directory if it doesn't exist
    snprintf(path, sizeof(path), "%s/.config/cofi", home);
    mkdir(path, 0755);
    
    // Return full path to harpoon.json
    snprintf(path, sizeof(path), "%s/.config/cofi/harpoon.json", home);
    return path;
}

// Helper function to parse harpoon slot data from a line
static void parse_harpoon_slot_line(const char *line, HarpoonSlot *temp_slot, int *slot) {
    if (strstr(line, "\"slot\":")) {
        sscanf(line, " \"slot\": %d", slot);
    } else if (strstr(line, "\"window_id\":")) {
        sscanf(line, " \"window_id\": %lu", &temp_slot->id);
    } else if (strstr(line, "\"title\":")) {
        char *colon = strchr(line, ':');
        if (colon) {
            char *start = strchr(colon + 1, '"');
            if (start) {
                start++;  // Move past the opening quote
                char *end = strchr(start, '"');
                if (end) {
                    int len = end - start;
                    if (len >= (int)sizeof(temp_slot->title)) len = sizeof(temp_slot->title) - 1;
                    safe_string_copy(temp_slot->title, start, len + 1);
                }
            }
        }
    } else if (strstr(line, "\"class_name\":")) {
        char *colon = strchr(line, ':');
        if (colon) {
            char *start = strchr(colon + 1, '"');
            if (start) {
                start++;  // Move past the opening quote
                char *end = strchr(start, '"');
                if (end) {
                    int len = end - start;
                    if (len >= (int)sizeof(temp_slot->class_name)) len = sizeof(temp_slot->class_name) - 1;
                    safe_string_copy(temp_slot->class_name, start, len + 1);
                }
            }
        }
    } else if (strstr(line, "\"instance\":")) {
        char *colon = strchr(line, ':');
        if (colon) {
            char *start = strchr(colon + 1, '"');
            if (start) {
                start++;  // Move past the opening quote
                char *end = strchr(start, '"');
                if (end) {
                    int len = end - start;
                    if (len >= (int)sizeof(temp_slot->instance)) len = sizeof(temp_slot->instance) - 1;
                    safe_string_copy(temp_slot->instance, start, len + 1);
                }
            }
        }
    } else if (strstr(line, "\"type\":")) {
        char *colon = strchr(line, ':');
        if (colon) {
            char *start = strchr(colon + 1, '"');
            if (start) {
                start++;  // Move past the opening quote
                char *end = strchr(start, '"');
                if (end) {
                    int len = end - start;
                    if (len >= (int)sizeof(temp_slot->type)) len = sizeof(temp_slot->type) - 1;
                    safe_string_copy(temp_slot->type, start, len + 1);
                }
            }
        }
    }
}

// Save harpoon slots to separate config file
void save_harpoon_slots(const HarpoonManager *harpoon) {
    if (!harpoon) return;
    
    const char *path = get_harpoon_config_path();
    FILE *file = fopen(path, "w");
    if (!file) {
        log_error("Failed to open harpoon config file for writing: %s", path);
        return;
    }
    
    fprintf(file, "{\n");
    fprintf(file, "  \"harpoon_slots\": [\n");
    
    int first = 1;
    for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
        if (harpoon->slots[i].assigned) {
            if (!first) fprintf(file, ",\n");
            first = 0;
            
            fprintf(file, "    {\n");
            fprintf(file, "      \"slot\": %d,\n", i);
            fprintf(file, "      \"window_id\": %lu,\n", harpoon->slots[i].id);
            fprintf(file, "      \"title\": \"%s\",\n", harpoon->slots[i].title);
            fprintf(file, "      \"class_name\": \"%s\",\n", harpoon->slots[i].class_name);
            fprintf(file, "      \"instance\": \"%s\",\n", harpoon->slots[i].instance);
            fprintf(file, "      \"type\": \"%s\"\n", harpoon->slots[i].type);
            fprintf(file, "    }");
        }
    }
    
    fprintf(file, "\n  ]\n");
    fprintf(file, "}\n");
    
    fclose(file);
    log_debug("Saved harpoon slots to %s", path);
}

// Load harpoon slots from separate config file
void load_harpoon_slots(HarpoonManager *harpoon) {
    if (!harpoon) return;
    
    const char *path = get_harpoon_config_path();
    FILE *file = fopen(path, "r");
    if (!file) {
        if (errno != ENOENT) {
            log_error("Failed to open harpoon config file for reading: %s", path);
        }
        return;
    }
    
    // Parse the JSON file line by line (simple parser)
    char line[1024];
    int in_slots = 0;
    int slot = -1;
    HarpoonSlot temp_slot = {0};
    
    while (fgets(line, sizeof(line), file)) {
        // Trim whitespace
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        
        // Check for section markers
        if (strstr(p, "\"harpoon_slots\":")) {
            in_slots = 1;
        } else if (strstr(p, "}")) {
            if (in_slots && slot >= 0 && slot < MAX_HARPOON_SLOTS && temp_slot.id != 0) {
                // End of slot entry, save it
                harpoon->slots[slot] = temp_slot;
                harpoon->slots[slot].assigned = 1;
                slot = -1;
                memset(&temp_slot, 0, sizeof(temp_slot));
            }
        }
        
        // Parse content if in slots section
        if (in_slots) {
            parse_harpoon_slot_line(p, &temp_slot, &slot);
        }
    }
    
    fclose(file);
    log_info("Loaded harpoon slots from %s", path);
}
