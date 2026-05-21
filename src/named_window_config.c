#include "named_window_config.h"
#include "log.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

// Helper function to get named windows config file path
static const char* get_named_windows_config_path() {
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
    
    // Return full path to names.json
    snprintf(path, sizeof(path), "%s/.config/cofi/names.json", home);
    return path;
}

// Helper function to escape quotes in strings for JSON
static void escape_json_string(const char *input, char *output, size_t output_size) {
    size_t j = 0;
    for (size_t i = 0; input[i] && j < output_size - 2; i++) {
        if (input[i] == '"' || input[i] == '\\') {
            if (j < output_size - 3) {
                output[j++] = '\\';
                output[j++] = input[i];
            }
        } else {
            output[j++] = input[i];
        }
    }
    output[j] = '\0';
}

void save_named_windows(const NamedWindowManager *manager) {
    if (!manager) return;
    
    const char *path = get_named_windows_config_path();
    FILE *file = fopen(path, "w");
    if (!file) {
        log_error("Failed to open named windows config file for writing: %s", path);
        return;
    }
    
    fprintf(file, "{\n");
    fprintf(file, "  \"next_match_id\": %d,\n", manager->next_match_id > 0 ? manager->next_match_id : 1);
    fprintf(file, "  \"named_windows\": [\n");
    
    int first = 1;
    for (int i = 0; i < manager->count; i++) {
        const NamedWindow *entry = &manager->entries[i];
        
        if (!first) fprintf(file, ",\n");
        first = 0;
        
        // Escape strings for JSON
        char escaped_name[MAX_TITLE_LEN * 2];
        char escaped_title[MAX_TITLE_LEN * 2];
        char escaped_class[MAX_CLASS_LEN * 2];
        char escaped_instance[MAX_CLASS_LEN * 2];
        
        escape_json_string(entry->custom_name, escaped_name, sizeof(escaped_name));
        escape_json_string(entry->original_title, escaped_title, sizeof(escaped_title));
        escape_json_string(entry->class_name, escaped_class, sizeof(escaped_class));
        escape_json_string(entry->instance, escaped_instance, sizeof(escaped_instance));
        
        fprintf(file, "    {\n");
        fprintf(file, "      \"match_id\": %d,\n", entry->match_id);
        fprintf(file, "      \"bound_x11_id\": %lu,\n", entry->bound_x11_id);
        fprintf(file, "      \"custom_name\": \"%s\",\n", escaped_name);
        fprintf(file, "      \"original_title\": \"%s\",\n", escaped_title);
        fprintf(file, "      \"class_name\": \"%s\",\n", escaped_class);
        fprintf(file, "      \"instance\": \"%s\",\n", escaped_instance);
        fprintf(file, "      \"type\": \"%s\",\n", entry->type);
        fprintf(file, "      \"match_mode\": \"%s\",\n",
                entry->match_mode == TITLE_MATCH_MODE_GLOB ? "GLOB" : "EXACT");
        fprintf(file, "      \"assigned\": %d\n", entry->assigned);
        fprintf(file, "    }");
    }
    
    fprintf(file, "\n  ]\n");
    fprintf(file, "}\n");
    
    fclose(file);
    log_debug("Saved %d named windows to %s", manager->count, path);
}

// Helper function to parse named window data from a line
static void parse_named_window_line(const char *line, NamedWindow *temp_entry, int *in_entry) {
    if (strstr(line, "{")) {
        *in_entry = 1;
        memset(temp_entry, 0, sizeof(NamedWindow));
        temp_entry->match_mode = TITLE_MATCH_MODE_EXACT;
    } else if (strstr(line, "\"window_id\":")) {
        sscanf(line, " \"window_id\": %lu", &temp_entry->bound_x11_id);
    } else if (strstr(line, "\"bound_x11_id\":")) {
        sscanf(line, " \"bound_x11_id\": %lu", &temp_entry->bound_x11_id);
    } else if (strstr(line, "\"match_id\":")) {
        sscanf(line, " \"match_id\": %d", &temp_entry->match_id);
    } else if (strstr(line, "\"custom_name\":")) {
        char *colon = strchr(line, ':');
        if (colon) {
            char *start = strchr(colon + 1, '"');
            if (start) {
                start++;  // Move past the opening quote
                char *end = strrchr(start, '"');
                if (end) {
                    int len = end - start;
                    if (len >= (int)sizeof(temp_entry->custom_name)) len = sizeof(temp_entry->custom_name) - 1;
                    safe_string_copy(temp_entry->custom_name, start, len + 1);
                }
            }
        }
    } else if (strstr(line, "\"original_title\":")) {
        char *colon = strchr(line, ':');
        if (colon) {
            char *start = strchr(colon + 1, '"');
            if (start) {
                start++;  // Move past the opening quote
                char *end = strrchr(start, '"');
                if (end) {
                    int len = end - start;
                    if (len >= (int)sizeof(temp_entry->original_title)) len = sizeof(temp_entry->original_title) - 1;
                    safe_string_copy(temp_entry->original_title, start, len + 1);
                }
            }
        }
    } else if (strstr(line, "\"class_name\":")) {
        char *colon = strchr(line, ':');
        if (colon) {
            char *start = strchr(colon + 1, '"');
            if (start) {
                start++;  // Move past the opening quote
                char *end = strrchr(start, '"');
                if (end) {
                    int len = end - start;
                    if (len >= (int)sizeof(temp_entry->class_name)) len = sizeof(temp_entry->class_name) - 1;
                    safe_string_copy(temp_entry->class_name, start, len + 1);
                }
            }
        }
    } else if (strstr(line, "\"instance\":")) {
        char *colon = strchr(line, ':');
        if (colon) {
            char *start = strchr(colon + 1, '"');
            if (start) {
                start++;  // Move past the opening quote
                char *end = strrchr(start, '"');
                if (end) {
                    int len = end - start;
                    if (len >= (int)sizeof(temp_entry->instance)) len = sizeof(temp_entry->instance) - 1;
                    safe_string_copy(temp_entry->instance, start, len + 1);
                }
            }
        }
    } else if (strstr(line, "\"type\":")) {
        char *colon = strchr(line, ':');
        if (colon) {
            char *start = strchr(colon + 1, '"');
            if (start) {
                start++;  // Move past the opening quote
                char *end = strrchr(start, '"');
                if (end) {
                    int len = end - start;
                    if (len >= (int)sizeof(temp_entry->type)) len = sizeof(temp_entry->type) - 1;
                    safe_string_copy(temp_entry->type, start, len + 1);
                }
            }
        }
    } else if (strstr(line, "\"match_mode\":")) {
        if (strstr(line, "GLOB")) {
            temp_entry->match_mode = TITLE_MATCH_MODE_GLOB;
        } else {
            temp_entry->match_mode = TITLE_MATCH_MODE_EXACT;
        }
    } else if (strstr(line, "\"assigned\":")) {
        int assigned;
        if (sscanf(line, " \"assigned\": %d", &assigned) == 1) {
            temp_entry->assigned = assigned;
        }
    }
}

void load_named_windows(NamedWindowManager *manager) {
    if (!manager) return;
    
    // Initialize manager first
    init_named_window_manager(manager);
    
    const char *path = get_named_windows_config_path();
    FILE *file = fopen(path, "r");
    if (!file) {
        if (errno != ENOENT) {
            log_error("Failed to open named windows config file for reading: %s", path);
        }
        return;
    }
    
    // Parse the JSON file line by line (simple parser)
    char line[1024];
    int in_array = 0;
    int in_entry = 0;
    NamedWindow temp_entry = {0};
    
    while (fgets(line, sizeof(line), file)) {
        // Trim whitespace
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        
        // Check for section markers
        if (strstr(p, "\"next_match_id\":")) {
            int next = 0;
            if (sscanf(p, "\"next_match_id\": %d", &next) == 1 && next > 0) {
                manager->next_match_id = next;
            }
        } else if (strstr(p, "\"named_windows\":")) {
            in_array = 1;
        } else if (in_array && strstr(p, "}")) {
            if (in_entry) {
                // End of entry, save it
                if (manager->count < MAX_WINDOWS) {
                    manager->entries[manager->count] = temp_entry;
                    manager->count++;
                }
                in_entry = 0;
                memset(&temp_entry, 0, sizeof(temp_entry));
            }
        }
        
        // Parse content if in array
        if (in_array) {
            parse_named_window_line(p, &temp_entry, &in_entry);
        }
    }

    // Normalize match IDs: repair malformed/duplicate ids and keep next_match_id monotonic.
    if (manager->next_match_id <= 0) {
        manager->next_match_id = 1;
    }
    int seen_ids[MAX_WINDOWS];
    int seen_count = 0;
    for (int i = 0; i < manager->count; i++) {
        int id = manager->entries[i].match_id;
        int duplicate = 0;
        for (int j = 0; j < seen_count; j++) {
            if (seen_ids[j] == id) {
                duplicate = 1;
                break;
            }
        }
        if (id <= 0 || duplicate) {
            id = manager->next_match_id++;
            manager->entries[i].match_id = id;
        } else if (id >= manager->next_match_id) {
            manager->next_match_id = id + 1;
        }
        if (seen_count < MAX_WINDOWS) {
            seen_ids[seen_count++] = id;
        }
        if (manager->entries[i].match_mode != TITLE_MATCH_MODE_GLOB) {
            manager->entries[i].match_mode = TITLE_MATCH_MODE_EXACT;
        }
    }
    
    fclose(file);
    log_info("Loaded %d named windows from %s", manager->count, path);
}
