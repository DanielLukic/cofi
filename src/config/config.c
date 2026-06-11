#include "config/config.h"
#include "core/json/cofi_json_io.h"
#include "core/log/log.h"
#include "core/utils/utils.h"
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#define MAX_REGISTERED_CONFIG_ENTRIES 64

static CofiConfigSpec s_config_specs[MAX_REGISTERED_CONFIG_ENTRIES];
static int s_config_spec_count = 0;

int cofi_register_config_entry(const CofiConfigSpec *spec) {
    if (!spec || !spec->key || spec->key[0] == '\0' ||
        !spec->get_value || !spec->set_value) {
        return -1;
    }
    if (strlen(spec->key) >= CONFIG_KEY_LEN) return -1;

    for (int i = 0; i < s_config_spec_count; i++) {
        if (strcmp(s_config_specs[i].key, spec->key) == 0) {
            return -1;
        }
    }
    if (s_config_spec_count >= MAX_REGISTERED_CONFIG_ENTRIES) return -1;

    s_config_specs[s_config_spec_count++] = *spec;
    return 0;
}

int cofi_config_entry_count(void) {
    return s_config_spec_count;
}

const CofiConfigSpec *cofi_config_entry_at(int index) {
    if (index < 0 || index >= s_config_spec_count) return NULL;
    return &s_config_specs[index];
}

const CofiConfigSpec *cofi_config_entry_for_key(const char *key) {
    if (!key) return NULL;
    for (int i = 0; i < s_config_spec_count; i++) {
        if (strcmp(s_config_specs[i].key, key) == 0) {
            return &s_config_specs[i];
        }
    }
    return NULL;
}

void cofi_config_registry_reset(void) {
    s_config_spec_count = 0;
}

static const char* get_config_path(void) {
    static char path[512];
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(path, sizeof(path), "%s/.config", home);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi", home);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi/options.json", home);
    return path;
}

const char* alignment_to_string(WindowAlignment align) {
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
    if (strcmp(str, "center") == 0) return ALIGN_CENTER;
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

const char* slot_sort_order_to_string(SlotSortOrder order) {
    switch (order) {
        case SLOT_SORT_COLUMN_FIRST: return "column";
        default: return "row";
    }
}

SlotSortOrder string_to_slot_sort_order(const char *str) {
    if (!str) return SLOT_SORT_ROW_FIRST;
    if (strcmp(str, "column") == 0) return SLOT_SORT_COLUMN_FIRST;
    return SLOT_SORT_ROW_FIRST;
}

const char* window_order_mode_to_string(WindowOrderMode mode) {
    switch (mode) {
        case WINDOW_ORDER_NATIVE: return "native";
        default: return "cofi";
    }
}

WindowOrderMode string_to_window_order_mode(const char *str) {
    if (!str) return WINDOW_ORDER_COFI;
    if (strcmp(str, "native") == 0) return WINDOW_ORDER_NATIVE;
    return WINDOW_ORDER_COFI;
}

static void save_options_section(JsonBuilder *builder, const CofiConfig *config) {
    int registered_count = cofi_config_entry_count();
    json_builder_set_member_name(builder, "options");
    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "close_on_focus_loss");
    json_builder_add_boolean_value(builder, config->close_on_focus_loss ? TRUE : FALSE);
    json_builder_set_member_name(builder, "align");
    json_builder_add_string_value(builder, alignment_to_string(config->alignment));
    json_builder_set_member_name(builder, "workspaces_per_row");
    json_builder_add_int_value(builder, config->workspaces_per_row);
    json_builder_set_member_name(builder, "tile_columns");
    json_builder_add_int_value(builder, config->tile_columns);
    json_builder_set_member_name(builder, "digit_slot_mode");
    json_builder_add_string_value(builder, digit_slot_mode_to_string(config->digit_slot_mode));
    json_builder_set_member_name(builder, "slot_overlay_duration_ms");
    json_builder_add_int_value(builder, config->slot_overlay_duration_ms);
    json_builder_set_member_name(builder, "ripple_enabled");
    json_builder_add_boolean_value(builder, config->ripple_enabled ? TRUE : FALSE);
    json_builder_set_member_name(builder, "slot_sort_order");
    json_builder_add_string_value(builder, slot_sort_order_to_string(config->slot_sort_order));
    json_builder_set_member_name(builder, "log_level");
    json_builder_add_string_value(builder, config->log_level);
    json_builder_set_member_name(builder, "window_order_mode");
    json_builder_add_string_value(builder, window_order_mode_to_string(config->window_order_mode));
    json_builder_set_member_name(builder, "show_all_tabs");
    json_builder_add_boolean_value(builder, config->show_all_tabs ? TRUE : FALSE);
    json_builder_set_member_name(builder, "rules.show_all_tags");
    json_builder_add_boolean_value(builder, config->rules_show_all_tags ? TRUE : FALSE);
    json_builder_set_member_name(builder, "disabled_providers");
    json_builder_add_string_value(builder, config->disabled_providers);
    json_builder_set_member_name(builder, "slot_occlusion_threshold");
    json_builder_add_int_value(builder, config->slot_occlusion_threshold_pct);

    for (int i = 0; i < registered_count; i++) {
        const CofiConfigSpec *spec = cofi_config_entry_at(i);
        char value[CONFIG_VALUE_LEN] = {0};
        if (!spec || !spec->get_value(config, value, sizeof(value))) continue;
        json_builder_set_member_name(builder, spec->key);
        json_builder_add_string_value(builder, value);
    }
    json_builder_end_object(builder);
}

void init_config_defaults(CofiConfig *config) {
    if (!config) return;

    config->close_on_focus_loss = 1;
    config->alignment = ALIGN_CENTER;
    config->workspaces_per_row = 0;
    config->tile_columns = 2;
    config->digit_slot_mode = DIGIT_MODE_DEFAULT;
    config->slot_overlay_duration_ms = 750;
    config->ripple_enabled = 1;
    config->slot_sort_order = SLOT_SORT_ROW_FIRST;
    config->slot_occlusion_threshold_pct = 5;
    config->show_all_tabs = 0;
    config->rules_show_all_tags = 0;
    strncpy(config->log_level, "debug", sizeof(config->log_level) - 1);
    config->window_order_mode = WINDOW_ORDER_COFI;
    config->disabled_providers[0] = '\0';
    config->projects_tmux_path[0] = '\0';
    config->projects_zellij_path[0] = '\0';
    config->projects_zoxide_path[0] = '\0';
    config->projects_file_explorer_path[0] = '\0';
    config->projects_locate_enabled = 1;
    g_strlcpy(config->projects_locate_excludes,
              "~/.cache/*,~/.local/*,~/.config/*,~/.var/app/*,~/snap/*,~/.gradle/*,~/.npm/*,~/.nvm/*,*/node_modules/*,*/__pycache__/*,*/.git/*,*/caches/*,*/cache/*",
              sizeof(config->projects_locate_excludes));
    config->projects_locate_search_roots[0] = '\0';
    config->projects_locate_timeout_ms = 1500;
    config->files_enabled = 1;
    config->files_fd_path[0] = '\0';
    config->files_excludes[0] = '\0';
}

void save_config(const CofiConfig *config) {
    if (!config) return;

    const char *path = get_config_path();
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    save_options_section(builder, config);
    json_builder_end_object(builder);
    JsonNode *root = json_builder_get_root(builder);
    bool ok = cofi_json_save_root(path, root);
    json_node_unref(root);
    g_object_unref(builder);
    if (ok) log_debug("Saved config options to %s", path);
}

void load_config(CofiConfig *config) {
    if (!config) return;

    init_config_defaults(config);

    const char *path = get_config_path();
    GStatBuf st;
    if (g_stat(path, &st) != 0) {
        if (errno == ENOENT) {
            save_config(config);
        } else {
            log_error("Failed to open config file for reading: %s", path);
        }
        return;
    }

    JsonParser *parser = cofi_json_load_object_file(path);
    if (!parser) {
        log_error("Failed to load config file, using defaults: %s", path);
        return;
    }

    JsonObject *root = json_node_get_object(json_parser_get_root(parser));
    JsonObject *options = cofi_json_obj_object(root, "options");
    if (!options) {
        log_trace("Config missing options object; using defaults");
        g_object_unref(parser);
        return;
    }

    gboolean present = FALSE;
    config->close_on_focus_loss = cofi_json_obj_bool_or(
        options, "close_on_focus_loss", config->close_on_focus_loss, &present) ? 1 : 0;
    if (!present) log_trace("Config missing key: close_on_focus_loss; using default");

    const char *s = cofi_json_obj_str_or(options, "align", "", &present);
    if (present) config->alignment = string_to_alignment(s);
    else log_trace("Config missing key: align; using default");

    config->workspaces_per_row = cofi_json_obj_int_or(
        options, "workspaces_per_row", config->workspaces_per_row, &present);
    if (!present) log_trace("Config missing key: workspaces_per_row; using default");

    int columns = cofi_json_obj_int_or(options, "tile_columns", config->tile_columns, &present);
    if (!present) {
        log_trace("Config missing key: tile_columns; using default");
    } else if (columns == 2 || columns == 3) {
        config->tile_columns = columns;
    } else {
        log_warn("Invalid tile_columns value %d, using default 3", columns);
        config->tile_columns = 3;
    }

    s = cofi_json_obj_str_or(options, "digit_slot_mode", "", &present);
    if (present) config->digit_slot_mode = string_to_digit_slot_mode(s);
    else log_trace("Config missing key: digit_slot_mode; using default");

    config->slot_overlay_duration_ms = cofi_json_obj_int_or(
        options, "slot_overlay_duration_ms", config->slot_overlay_duration_ms, &present);
    if (!present) log_trace("Config missing key: slot_overlay_duration_ms; using default");

    config->ripple_enabled = cofi_json_obj_bool_or(
        options, "ripple_enabled", config->ripple_enabled, &present) ? 1 : 0;
    if (!present) log_trace("Config missing key: ripple_enabled; using default");

    s = cofi_json_obj_str_or(options, "slot_sort_order", "", &present);
    if (present) config->slot_sort_order = string_to_slot_sort_order(s);
    else log_trace("Config missing key: slot_sort_order; using default");

    s = cofi_json_obj_str_or(options, "window_order_mode", "", &present);
    if (present) config->window_order_mode = string_to_window_order_mode(s);
    else log_trace("Config missing key: window_order_mode; using default");

    config->show_all_tabs = cofi_json_obj_bool_or(
        options, "show_all_tabs", config->show_all_tabs, &present) ? 1 : 0;
    if (!present) log_trace("Config missing key: show_all_tabs; using default");
    config->rules_show_all_tags = cofi_json_obj_bool_or(
        options, "rules.show_all_tags", config->rules_show_all_tags, &present) ? 1 : 0;
    if (!present) {
        config->rules_show_all_tags = cofi_json_obj_bool_or(
            options, "rules_show_all_tags", config->rules_show_all_tags, &present) ? 1 : 0;
    }
    if (!present) log_trace("Config missing key: rules.show_all_tags; using default");

    s = cofi_json_obj_str_or(options, "disabled_providers", "", &present);
    if (present) {
        g_strlcpy(config->disabled_providers, s, sizeof(config->disabled_providers));
    } else {
        log_trace("Config missing key: disabled_providers; using default");
    }

    JsonNode *threshold_node = json_object_get_member(options, "slot_occlusion_threshold");
    if (!threshold_node || !JSON_NODE_HOLDS_VALUE(threshold_node)) {
        log_trace("Config missing key: slot_occlusion_threshold; using default");
    } else {
        int pct = 0;
        GType threshold_type = json_node_get_value_type(threshold_node);
        if (threshold_type == G_TYPE_INT64 || threshold_type == G_TYPE_INT) {
            pct = (int)json_node_get_int(threshold_node);
        } else if (threshold_type == G_TYPE_DOUBLE) {
            double raw = json_node_get_double(threshold_node);
            if (raw >= 0.0 && raw < 1.0) pct = (int)(raw * 100.0 + 0.5);
            else pct = (int)(raw + 0.5);
        }
        if (pct >= 1 && pct <= 100) {
            config->slot_occlusion_threshold_pct = pct;
        }
    }

    s = cofi_json_obj_str_or(options, "log_level", "", &present);
    if (present) {
        g_strlcpy(config->log_level, s, sizeof(config->log_level));
    }

    gboolean quick_slots = cofi_json_obj_bool_or(options, "quick_workspace_slots", FALSE, &present);
    if (present && quick_slots) {
        config->digit_slot_mode = DIGIT_MODE_WORKSPACES;
    }

    for (int i = 0; i < cofi_config_entry_count(); i++) {
        const CofiConfigSpec *spec = cofi_config_entry_at(i);
        if (!spec || !spec->key) continue;
        gboolean key_present = FALSE;
        const char *value = cofi_json_obj_str_or(options, spec->key, "", &key_present);
        if (!key_present) continue;
        char err[128] = {0};
        if (!spec->set_value(config, value, err, sizeof(err))) {
            log_warn("Ignoring invalid config value for %s: %s",
                     spec->key, err[0] ? err : "invalid value");
        }
    }

    g_object_unref(parser);
    log_info("Loaded config options from %s", path);
}

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
    if (strcmp(key, "show_all_tabs") == 0) {
        int v = parse_bool_value(value);
        if (v < 0) { snprintf(err_buf, err_size, "Expected true/false/on/off/1/0"); return 0; }
        config->show_all_tabs = v;
        return 1;
    }
    if (strcmp(key, "rules.show_all_tags") == 0 ||
        strcmp(key, "rules_show_all_tags") == 0) {
        int v = parse_bool_value(value);
        if (v < 0) { snprintf(err_buf, err_size, "Expected true/false/on/off/1/0"); return 0; }
        config->rules_show_all_tags = v;
        return 1;
    }
    if (strcmp(key, "disabled_providers") == 0) {
        if (strlen(value) >= sizeof(config->disabled_providers)) {
            snprintf(err_buf, err_size, "Provider list too long");
            return 0;
        }
        strncpy(config->disabled_providers, value, sizeof(config->disabled_providers) - 1);
        config->disabled_providers[sizeof(config->disabled_providers) - 1] = '\0';
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

    // Enum: slot_sort_order
    if (strcmp(key, "slot_sort_order") == 0) {
        SlotSortOrder o = string_to_slot_sort_order(value);
        if (o == SLOT_SORT_ROW_FIRST && strcmp(value, "row") != 0) {
            snprintf(err_buf, err_size, "Unknown order: %s (use row/column)", value);
            return 0;
        }
        config->slot_sort_order = o;
        return 1;
    }

    // Enum: window_order_mode
    if (strcmp(key, "window_order_mode") == 0) {
        WindowOrderMode m = string_to_window_order_mode(value);
        if (m == WINDOW_ORDER_COFI && strcmp(value, "cofi") != 0) {
            snprintf(err_buf, err_size, "Unknown mode: %s (use cofi/native)", value);
            return 0;
        }
        config->window_order_mode = m;
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

    // Integer: slot_occlusion_threshold (1-100 percent)
    if (strcmp(key, "slot_occlusion_threshold") == 0) {
        char *end = NULL;
        long v = strtol(value, &end, 10);
        if (end == value || *end != '\0') { snprintf(err_buf, err_size, "Must be integer 1-100"); return 0; }
        if (v < 1 || v > 100) { snprintf(err_buf, err_size, "Must be 1-100"); return 0; }
        config->slot_occlusion_threshold_pct = (int)v;
        return 1;
    }

    // Enum: log_level (applies immediately)
    if (strcmp(key, "log_level") == 0) {
        int level = -1;
        if (strcasecmp(value, "trace") == 0) level = 0;
        else if (strcasecmp(value, "debug") == 0) level = 1;
        else if (strcasecmp(value, "info") == 0) level = 2;
        else if (strcasecmp(value, "warn") == 0) level = 3;
        else if (strcasecmp(value, "error") == 0) level = 4;
        else if (strcasecmp(value, "fatal") == 0) level = 5;
        if (level < 0) {
            snprintf(err_buf, err_size, "Unknown log level: %s (use trace/debug/info/warn/error/fatal)", value);
            return 0;
        }
        strncpy(config->log_level, value, sizeof(config->log_level) - 1);
        config->log_level[sizeof(config->log_level) - 1] = '\0';
        log_set_level(level);
        return 1;
    }

    const CofiConfigSpec *spec = cofi_config_entry_for_key(key);
    if (spec) {
        return spec->set_value(config, value, err_buf, err_size);
    }

    snprintf(err_buf, err_size, "Unknown config key: %s", key);
    return 0;
}

const char* get_next_enum_value(const char *key, const char *current_value) {
    if (!key || !current_value) return NULL;

    static const char *align_values[] = {
        "center", "top", "top_left", "top_right",
        "left", "right", "bottom", "bottom_left", "bottom_right"
    };
    static const char *digit_slot_mode_values[] = { "default", "per-workspace", "workspaces" };
    static const char *slot_sort_order_values[] = { "row", "column" };
    static const char *log_level_values[] = { "trace", "debug", "info", "warn", "error", "fatal" };
    static const char *window_order_mode_values[] = { "cofi", "native" };

    const char **values = NULL;
    int count = 0;

    if (strcmp(key, "align") == 0) {
        values = align_values;
        count = 9;
    } else if (strcmp(key, "digit_slot_mode") == 0) {
        values = digit_slot_mode_values;
        count = 3;
    } else if (strcmp(key, "slot_sort_order") == 0) {
        values = slot_sort_order_values;
        count = 2;
    } else if (strcmp(key, "log_level") == 0) {
        values = log_level_values;
        count = 6;
    } else if (strcmp(key, "window_order_mode") == 0) {
        values = window_order_mode_values;
        count = 2;
    } else {
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        if (strcmp(values[i], current_value) == 0)
            return values[(i + 1) % count];
    }
    return values[0];  // current value not found, wrap to first
}

void build_config_entries(const CofiConfig *config, ConfigEntry *entries, int *count) {
    *count = 0;

    #define ADD_BOOL(k, val) do { \
        strncpy(entries[*count].key, k, CONFIG_KEY_LEN - 1); \
        strncpy(entries[*count].value, (val) ? "true" : "false", CONFIG_VALUE_LEN - 1); \
        strncpy(entries[*count].display_value, entries[*count].value, CONFIG_VALUE_LEN - 1); \
        entries[*count].type = CONFIG_TYPE_BOOL; \
        (*count)++; \
    } while(0)

    #define ADD_INT(k, val) do { \
        strncpy(entries[*count].key, k, CONFIG_KEY_LEN - 1); \
        snprintf(entries[*count].value, CONFIG_VALUE_LEN, "%d", val); \
        strncpy(entries[*count].display_value, entries[*count].value, CONFIG_VALUE_LEN - 1); \
        entries[*count].type = CONFIG_TYPE_INT; \
        (*count)++; \
    } while(0)

    #define ADD_ENUM(k, val) do { \
        strncpy(entries[*count].key, k, CONFIG_KEY_LEN - 1); \
        strncpy(entries[*count].value, val, CONFIG_VALUE_LEN - 1); \
        strncpy(entries[*count].display_value, entries[*count].value, CONFIG_VALUE_LEN - 1); \
        entries[*count].type = CONFIG_TYPE_ENUM; \
        (*count)++; \
    } while(0)
    ADD_BOOL("close_on_focus_loss", config->close_on_focus_loss);
    ADD_ENUM("align", alignment_to_string(config->alignment));
    ADD_INT("workspaces_per_row", config->workspaces_per_row);
    ADD_INT("tile_columns", config->tile_columns);
    ADD_ENUM("digit_slot_mode", digit_slot_mode_to_string(config->digit_slot_mode));
    ADD_ENUM("slot_sort_order", slot_sort_order_to_string(config->slot_sort_order));
    ADD_ENUM("log_level", config->log_level);
    ADD_ENUM("window_order_mode", window_order_mode_to_string(config->window_order_mode));
    ADD_BOOL("show_all_tabs", config->show_all_tabs);
    ADD_BOOL("rules.show_all_tags", config->rules_show_all_tags);
    strncpy(entries[*count].key, "disabled_providers", CONFIG_KEY_LEN - 1);
    const char *disabled_value = config->disabled_providers[0] ?
        config->disabled_providers : "(none)";
    strncpy(entries[*count].value, disabled_value, CONFIG_VALUE_LEN - 1);
    entries[*count].value[CONFIG_VALUE_LEN - 1] = '\0';
    strncpy(entries[*count].display_value, entries[*count].value, CONFIG_VALUE_LEN - 1);
    entries[*count].display_value[CONFIG_VALUE_LEN - 1] = '\0';
    entries[*count].type = CONFIG_TYPE_PROVIDER_LIST;
    (*count)++;
    ADD_INT("slot_overlay_duration_ms", config->slot_overlay_duration_ms);
    ADD_BOOL("ripple_enabled", config->ripple_enabled);
    ADD_INT("slot_occlusion_threshold", config->slot_occlusion_threshold_pct);
    for (int i = 0; i < cofi_config_entry_count() && *count < MAX_CONFIG_ENTRIES; i++) {
        const CofiConfigSpec *spec = cofi_config_entry_at(i);
        if (!spec || !spec->get_value) continue;
        strncpy(entries[*count].key, spec->key, CONFIG_KEY_LEN - 1);
        entries[*count].key[CONFIG_KEY_LEN - 1] = '\0';
        if (!spec->get_value(config, entries[*count].value, CONFIG_VALUE_LEN)) {
            entries[*count].value[0] = '\0';
        }
        entries[*count].value[CONFIG_VALUE_LEN - 1] = '\0';
        if (spec->get_display_value &&
            spec->get_display_value(config, entries[*count].display_value, CONFIG_VALUE_LEN)) {
            entries[*count].display_value[CONFIG_VALUE_LEN - 1] = '\0';
        } else {
            strncpy(entries[*count].display_value, entries[*count].value, CONFIG_VALUE_LEN - 1);
            entries[*count].display_value[CONFIG_VALUE_LEN - 1] = '\0';
        }
        entries[*count].type = spec->type;
        (*count)++;
    }
    #undef ADD_BOOL
    #undef ADD_INT
    #undef ADD_ENUM
}
