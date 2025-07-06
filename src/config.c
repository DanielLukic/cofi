#include "config.h"
#include "log.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

// Helper function to get config file path
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

// Helper function to convert WindowAlignment to string
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

// Helper function to convert string to WindowAlignment
static WindowAlignment string_to_alignment(const char *str) {
    if (!str) return ALIGN_CENTER;
    
    if (strcmp(str, "center") == 0) return ALIGN_CENTER;
    if (strcmp(str, "top") == 0) return ALIGN_TOP;
    if (strcmp(str, "top_left") == 0) return ALIGN_TOP_LEFT;
    if (strcmp(str, "top_right") == 0) return ALIGN_TOP_RIGHT;
    if (strcmp(str, "left") == 0) return ALIGN_LEFT;
    if (strcmp(str, "right") == 0) return ALIGN_RIGHT;
    if (strcmp(str, "bottom") == 0) return ALIGN_BOTTOM;
    if (strcmp(str, "bottom_left") == 0) return ALIGN_BOTTOM_LEFT;
    if (strcmp(str, "bottom_right") == 0) return ALIGN_BOTTOM_RIGHT;
    
    return ALIGN_CENTER;  // Default fallback
}

// Helper function to save options section
static void save_options_section(FILE *file, const CofiConfig *config) {
    fprintf(file, "  \"options\": {\n");
    fprintf(file, "    \"close_on_focus_loss\": %s,\n", config->close_on_focus_loss ? "true" : "false");
    fprintf(file, "    \"align\": \"%s\",\n", alignment_to_string(config->alignment));
    fprintf(file, "    \"workspaces_per_row\": %d,\n", config->workspaces_per_row);
    fprintf(file, "    \"tile_columns\": %d\n", config->tile_columns);
    fprintf(file, "  }");
}



// Initialize config with default values
void init_config_defaults(CofiConfig *config) {
    if (!config) return;
    
    config->close_on_focus_loss = 1;  // Default to true
    config->alignment = ALIGN_CENTER;  // Default to center
    config->workspaces_per_row = 0;   // Default to linear layout
    config->tile_columns = 2;         // Default to 2 columns (2x2 grid)
}

// Save configuration options only (harpoon slots saved separately)
void save_config(const CofiConfig *config) {
    if (!config) return;

    const char *path = get_config_path();
    FILE *file = fopen(path, "w");
    if (!file) {
        log_error("Failed to open config file for writing: %s", path);
        return;
    }

    fprintf(file, "{\n");

    // Save options section only
    save_options_section(file, config);

    fprintf(file, "\n}\n");

    fclose(file);
    log_info("Saved config options to %s", path);
}

// Helper function to parse options section
static void parse_options_line(const char *line, CofiConfig *config) {
    char align_str[32] = {0};

    if (strstr(line, "\"close_on_focus_loss\":")) {
        if (strstr(line, "true")) {
            config->close_on_focus_loss = 1;
        } else if (strstr(line, "false")) {
            config->close_on_focus_loss = 0;
        }
    } else if (strstr(line, "\"align\":")) {
        char *colon = strchr(line, ':');
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
                    config->alignment = string_to_alignment(align_str);
                }
            }
        }
    } else if (strstr(line, "\"workspaces_per_row\":")) {
        sscanf(line, " \"workspaces_per_row\": %d", &config->workspaces_per_row);
    } else if (strstr(line, "\"tile_columns\":")) {
        int columns;
        if (sscanf(line, " \"tile_columns\": %d", &columns) == 1) {
            // Validate: only allow 2 or 3 columns
            if (columns == 2 || columns == 3) {
                config->tile_columns = columns;
            } else {
                log_warn("Invalid tile_columns value %d, using default 3", columns);
                config->tile_columns = 3;
            }
        }
    }
}



// Load configuration options only (harpoon slots loaded separately)
void load_config(CofiConfig *config) {
    if (!config) return;

    // Initialize with defaults first
    init_config_defaults(config);

    const char *path = get_config_path();
    FILE *file = fopen(path, "r");
    if (!file) {
        if (errno != ENOENT) {
            log_error("Failed to open config file for reading: %s", path);
        }
        return;
    }

    // Parse the JSON file line by line (simple parser)
    char line[1024];
    int in_options = 0;

    while (fgets(line, sizeof(line), file)) {
        // Trim whitespace
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        // Check for section markers
        if (strstr(p, "\"options\":")) {
            in_options = 1;
        } else if (strstr(p, "}")) {
            // Check if this closes the options section
            if (strstr(p, "},") || (strstr(p, "}") && !strstr(p, "},"))) {
                in_options = 0;
            }
        }

        // Parse content if in options section
        if (in_options) {
            parse_options_line(p, config);
        }
    }

    fclose(file);
    log_info("Loaded config options from %s", path);
}
