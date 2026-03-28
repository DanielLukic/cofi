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
    
    // Create .config/cofi directory if it doesn't exist
    snprintf(path, sizeof(path), "%s/.config/cofi", home);
    mkdir(path, 0755);
    
    // Return full path to options.json
    snprintf(path, sizeof(path), "%s/.config/cofi/options.json", home);
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

// Digit slot mode string conversion
const char* digit_slot_mode_to_string(DigitSlotMode mode) {
    switch (mode) {
        case DIGIT_MODE_PER_WORKSPACE: return "per-workspace";
        case DIGIT_MODE_WORKSPACES: return "workspaces";
        default: return "default";
    }
}

DigitSlotMode string_to_digit_slot_mode(const char *str) {
    if (!str) return DIGIT_MODE_DEFAULT;
    if (strcmp(str, "per-workspace") == 0) return DIGIT_MODE_PER_WORKSPACE;
    if (strcmp(str, "workspaces") == 0) return DIGIT_MODE_WORKSPACES;
    return DIGIT_MODE_DEFAULT;
}

// Helper function to save options section
static void save_options_section(FILE *file, const CofiConfig *config) {
    fprintf(file, "  \"options\": {\n");
    fprintf(file, "    \"close_on_focus_loss\": %s,\n", config->close_on_focus_loss ? "true" : "false");
    fprintf(file, "    \"align\": \"%s\",\n", alignment_to_string(config->alignment));
    fprintf(file, "    \"workspaces_per_row\": %d,\n", config->workspaces_per_row);
    fprintf(file, "    \"tile_columns\": %d,\n", config->tile_columns);
    fprintf(file, "    \"digit_slot_mode\": \"%s\",\n", digit_slot_mode_to_string(config->digit_slot_mode));
    fprintf(file, "    \"slot_overlay_duration_ms\": %d,\n", config->slot_overlay_duration_ms);
    fprintf(file, "    \"ripple_enabled\": %s,\n", config->ripple_enabled ? "true" : "false");
    fprintf(file, "    \"hotkey_windows\": \"%s\",\n", config->hotkey_windows);
    fprintf(file, "    \"hotkey_command\": \"%s\",\n", config->hotkey_command);
    fprintf(file, "    \"hotkey_workspaces\": \"%s\"\n", config->hotkey_workspaces);
    fprintf(file, "  }");
}



// Initialize config with default values
void init_config_defaults(CofiConfig *config) {
    if (!config) return;

    config->close_on_focus_loss = 1;  // Default to true
    config->alignment = ALIGN_CENTER;  // Default to center
    config->workspaces_per_row = 0;   // Default to linear layout
    config->tile_columns = 2;         // Default to 2 columns (2x2 grid)
    config->digit_slot_mode = DIGIT_MODE_DEFAULT;
    config->slot_overlay_duration_ms = 750;
    config->ripple_enabled = 1;  // Default: ripple on
    strncpy(config->hotkey_windows,    "Mod1+Tab",       sizeof(config->hotkey_windows) - 1);
    strncpy(config->hotkey_command,    "Mod1+grave",     sizeof(config->hotkey_command) - 1);
    strncpy(config->hotkey_workspaces, "Mod1+BackSpace", sizeof(config->hotkey_workspaces) - 1);
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
    log_debug("Saved config options to %s", path);
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
    } else if (strstr(line, "\"digit_slot_mode\":")) {
        char *colon = strchr(line, ':');
        if (colon) {
            char *start = strchr(colon + 1, '"');
            if (start) {
                start++;
                char *end = strchr(start, '"');
                if (end) {
                    char mode_str[16] = {0};
                    int len = end - start;
                    if (len >= 16) len = 15;
                    strncpy(mode_str, start, len);
                    config->digit_slot_mode = string_to_digit_slot_mode(mode_str);
                }
            }
        }
    } else if (strstr(line, "\"slot_overlay_duration_ms\":")) {
        sscanf(line, " \"slot_overlay_duration_ms\": %d", &config->slot_overlay_duration_ms);
    } else if (strstr(line, "\"ripple_enabled\":")) {
        config->ripple_enabled = strstr(line, "true") ? 1 : 0;
    } else if (strstr(line, "\"hotkey_windows\":") || strstr(line, "\"hotkey_command\":") ||
               strstr(line, "\"hotkey_workspaces\":")) {
        char *colon = strchr(line, ':');
        if (colon) {
            char *start = strchr(colon + 1, '"');
            char val[64] = {0};
            if (start) {
                start++;
                char *end = strchr(start, '"');
                if (end) {
                    int len = (int)(end - start);
                    if (len >= 64) len = 63;
                    strncpy(val, start, len);
                }
            }
            if (strstr(line, "\"hotkey_windows\":"))
                strncpy(config->hotkey_windows, val, sizeof(config->hotkey_windows) - 1);
            else if (strstr(line, "\"hotkey_command\":"))
                strncpy(config->hotkey_command, val, sizeof(config->hotkey_command) - 1);
            else
                strncpy(config->hotkey_workspaces, val, sizeof(config->hotkey_workspaces) - 1);
        }
    } else if (strstr(line, "\"quick_assign_hotkey\":")) {
        // Ignored — hotkey removed, kept for backwards compat with old config files
    } else if (strstr(line, "\"quick_workspace_slots\":")) {
        // Migration: old boolean -> new digit_slot_mode
        if (strstr(line, "true")) {
            config->digit_slot_mode = DIGIT_MODE_WORKSPACES;
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

// Parse a boolean value string. Returns 1/0 on success, -1 on invalid.
static int parse_bool_value(const char *value) {
    if (!value) return -1;
    if (strcmp(value, "true") == 0 || strcmp(value, "on") == 0 || strcmp(value, "1") == 0) return 1;
    if (strcmp(value, "false") == 0 || strcmp(value, "off") == 0 || strcmp(value, "0") == 0) return 0;
    return -1;
}

int apply_config_setting(CofiConfig *config, const char *key, const char *value,
                         char *err_buf, size_t err_size) {
    if (!config || !key || !value) {
        if (err_buf) snprintf(err_buf, err_size, "NULL argument");
        return 0;
    }

    // Boolean fields
    if (strcmp(key, "close_on_focus_loss") == 0) {
        int v = parse_bool_value(value);
        if (v < 0) { snprintf(err_buf, err_size, "Expected true/false/on/off/1/0"); return 0; }
        config->close_on_focus_loss = v;
        return 1;
    }
    if (strcmp(key, "ripple_enabled") == 0) {
        int v = parse_bool_value(value);
        if (v < 0) { snprintf(err_buf, err_size, "Expected true/false/on/off/1/0"); return 0; }
        config->ripple_enabled = v;
        return 1;
    }

    // Enum: alignment
    if (strcmp(key, "align") == 0) {
        WindowAlignment a = string_to_alignment(value);
        // string_to_alignment returns ALIGN_CENTER for unknown — detect by checking round-trip
        if (a == ALIGN_CENTER && strcmp(value, "center") != 0) {
            snprintf(err_buf, err_size, "Unknown alignment: %s", value);
            return 0;
        }
        config->alignment = a;
        return 1;
    }

    // Enum: digit_slot_mode
    if (strcmp(key, "digit_slot_mode") == 0) {
        DigitSlotMode m = string_to_digit_slot_mode(value);
        if (m == DIGIT_MODE_DEFAULT && strcmp(value, "default") != 0) {
            snprintf(err_buf, err_size, "Unknown mode: %s (use default/per-workspace/workspaces)", value);
            return 0;
        }
        config->digit_slot_mode = m;
        return 1;
    }

    // Integer: workspaces_per_row (>= 0)
    if (strcmp(key, "workspaces_per_row") == 0) {
        int v = atoi(value);
        if (v < 0) { snprintf(err_buf, err_size, "Must be >= 0"); return 0; }
        config->workspaces_per_row = v;
        return 1;
    }

    // Integer: tile_columns (2 or 3)
    if (strcmp(key, "tile_columns") == 0) {
        int v = atoi(value);
        if (v != 2 && v != 3) { snprintf(err_buf, err_size, "Must be 2 or 3"); return 0; }
        config->tile_columns = v;
        return 1;
    }

    // Integer: slot_overlay_duration_ms (>= 0)
    if (strcmp(key, "slot_overlay_duration_ms") == 0) {
        int v = atoi(value);
        if (v < 0) { snprintf(err_buf, err_size, "Must be >= 0"); return 0; }
        config->slot_overlay_duration_ms = v;
        return 1;
    }

    // String: hotkeys
    if (strcmp(key, "hotkey_windows") == 0) {
        strncpy(config->hotkey_windows, value, sizeof(config->hotkey_windows) - 1);
        config->hotkey_windows[sizeof(config->hotkey_windows) - 1] = '\0';
        return 1;
    }
    if (strcmp(key, "hotkey_command") == 0) {
        strncpy(config->hotkey_command, value, sizeof(config->hotkey_command) - 1);
        config->hotkey_command[sizeof(config->hotkey_command) - 1] = '\0';
        return 1;
    }
    if (strcmp(key, "hotkey_workspaces") == 0) {
        strncpy(config->hotkey_workspaces, value, sizeof(config->hotkey_workspaces) - 1);
        config->hotkey_workspaces[sizeof(config->hotkey_workspaces) - 1] = '\0';
        return 1;
    }

    snprintf(err_buf, err_size, "Unknown config key: %s", key);
    return 0;
}

int format_config_display(const CofiConfig *config, char *buf, size_t buf_size) {
    if (!config || !buf || buf_size == 0) return 0;

    return snprintf(buf, buf_size,
        "close_on_focus_loss    %s\n"
        "align                  %s\n"
        "workspaces_per_row     %d\n"
        "tile_columns           %d\n"
        "digit_slot_mode        %s\n"
        "slot_overlay_duration_ms %d\n"
        "ripple_enabled         %s\n"
        "hotkey_windows         %s\n"
        "hotkey_command         %s\n"
        "hotkey_workspaces      %s\n",
        config->close_on_focus_loss ? "true" : "false",
        alignment_to_string(config->alignment),
        config->workspaces_per_row,
        config->tile_columns,
        digit_slot_mode_to_string(config->digit_slot_mode),
        config->slot_overlay_duration_ms,
        config->ripple_enabled ? "true" : "false",
        config->hotkey_windows,
        config->hotkey_command,
        config->hotkey_workspaces
    );
}
