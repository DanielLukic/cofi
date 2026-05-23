#include "match_entry_config.h"

#include <json-glib/json-glib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cofi_json_io.h"
#include "log.h"
#include "utils.h"

static const char *get_match_entries_config_path(void) {
    static char path[512];
    const char *home = getenv("HOME");
    if (!home) {
        home = ".";
    }

    snprintf(path, sizeof(path), "%s/.config", home);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi", home);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi/matching.json", home);
    return path;
}

static const char *match_mode_to_string(TitleMatchMode mode) {
    return mode == TITLE_MATCH_MODE_GLOB ? "GLOB" : "EXACT";
}

static TitleMatchMode parse_match_mode(JsonObject *entry_obj) {
    JsonNode *node = json_object_get_member(entry_obj, "match_mode");
    if (!node) {
        return TITLE_MATCH_MODE_EXACT;
    }

    if (JSON_NODE_HOLDS_VALUE(node)) {
        GType value_type = json_node_get_value_type(node);
        if (value_type == G_TYPE_STRING) {
            const char *mode_str = json_node_get_string(node);
            return g_strcmp0(mode_str, "GLOB") == 0 ? TITLE_MATCH_MODE_GLOB : TITLE_MATCH_MODE_EXACT;
        }
        if (value_type == G_TYPE_INT64 || value_type == G_TYPE_INT) {
            int mode_int = (int)json_node_get_int(node);
            return mode_int == TITLE_MATCH_MODE_GLOB ? TITLE_MATCH_MODE_GLOB : TITLE_MATCH_MODE_EXACT;
        }
    }

    return TITLE_MATCH_MODE_EXACT;
}

static int parse_assigned(JsonObject *entry_obj) {
    JsonNode *node = json_object_get_member(entry_obj, "assigned");
    if (!node || !JSON_NODE_HOLDS_VALUE(node)) {
        return 0;
    }

    GType value_type = json_node_get_value_type(node);
    if (value_type == G_TYPE_BOOLEAN) {
        return json_node_get_boolean(node) ? 1 : 0;
    }
    if (value_type == G_TYPE_INT64 || value_type == G_TYPE_INT) {
        return json_node_get_int(node) != 0 ? 1 : 0;
    }
    return 0;
}

static void normalize_loaded_match_entries(MatchEntryManager *manager) {
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
}

void save_match_entries(const MatchEntryManager *manager) {
    if (!manager) {
        return;
    }

    const char *path = get_match_entries_config_path();
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "next_match_id");
    json_builder_add_int_value(builder, manager->next_match_id > 0 ? manager->next_match_id : 1);

    json_builder_set_member_name(builder, "match_entries");
    json_builder_begin_array(builder);
    for (int i = 0; i < manager->count; i++) {
        const MatchEntry *entry = &manager->entries[i];
        json_builder_begin_object(builder);

        json_builder_set_member_name(builder, "match_id");
        json_builder_add_int_value(builder, entry->match_id);
        json_builder_set_member_name(builder, "bound_x11_id");
        json_builder_add_int_value(builder, (gint64)entry->bound_x11_id);
        json_builder_set_member_name(builder, "custom_name");
        json_builder_add_string_value(builder, entry->custom_name);
        json_builder_set_member_name(builder, "original_title");
        json_builder_add_string_value(builder, entry->original_title);
        json_builder_set_member_name(builder, "class_name");
        json_builder_add_string_value(builder, entry->class_name);
        json_builder_set_member_name(builder, "instance");
        json_builder_add_string_value(builder, entry->instance);
        json_builder_set_member_name(builder, "type");
        json_builder_add_string_value(builder, entry->type);
        json_builder_set_member_name(builder, "match_mode");
        json_builder_add_string_value(builder, match_mode_to_string(entry->match_mode));
        json_builder_set_member_name(builder, "assigned");
        json_builder_add_boolean_value(builder, entry->assigned != 0);

        json_builder_end_object(builder);
    }
    json_builder_end_array(builder);
    json_builder_end_object(builder);

    JsonNode *root = json_builder_get_root(builder);
    bool ok = cofi_json_save_root(path, root);
    json_node_unref(root);
    g_object_unref(builder);

    if (ok) {
        log_debug("Saved %d match entries to %s", manager->count, path);
    }
}

void load_match_entries(MatchEntryManager *manager) {
    if (!manager) {
        return;
    }

    match_entry_manager_init(manager);

    const char *path = get_match_entries_config_path();
    JsonParser *parser = cofi_json_load_object_file(path);
    if (!parser) {
        return;
    }

    JsonObject *root = json_node_get_object(json_parser_get_root(parser));
    manager->next_match_id = cofi_json_obj_int_or(root, "next_match_id", 1, NULL);

    JsonArray *entries = cofi_json_obj_array(root, "match_entries");
    if (entries) {
        guint n = json_array_get_length(entries);
        for (guint i = 0; i < n && manager->count < MAX_WINDOWS; i++) {
            JsonNode *node = json_array_get_element(entries, i);
            if (!node || !JSON_NODE_HOLDS_OBJECT(node)) {
                continue;
            }

            JsonObject *entry_obj = json_node_get_object(node);
            MatchEntry entry;
            memset(&entry, 0, sizeof(entry));
            entry.match_mode = TITLE_MATCH_MODE_EXACT;

            entry.match_id = cofi_json_obj_int_or(entry_obj, "match_id", 0, NULL);
            entry.bound_x11_id = (Window)cofi_json_obj_int_or(entry_obj, "bound_x11_id", 0, NULL);
            g_strlcpy(entry.custom_name,
                      cofi_json_obj_str_or(entry_obj, "custom_name", "", NULL),
                      sizeof(entry.custom_name));
            g_strlcpy(entry.original_title,
                      cofi_json_obj_str_or(entry_obj, "original_title", "", NULL),
                      sizeof(entry.original_title));
            g_strlcpy(entry.class_name,
                      cofi_json_obj_str_or(entry_obj, "class_name", "", NULL),
                      sizeof(entry.class_name));
            g_strlcpy(entry.instance,
                      cofi_json_obj_str_or(entry_obj, "instance", "", NULL),
                      sizeof(entry.instance));
            g_strlcpy(entry.type,
                      cofi_json_obj_str_or(entry_obj, "type", "", NULL),
                      sizeof(entry.type));
            entry.match_mode = parse_match_mode(entry_obj);
            entry.assigned = parse_assigned(entry_obj);

            manager->entries[manager->count++] = entry;
        }
    }

    normalize_loaded_match_entries(manager);
    g_object_unref(parser);
    log_info("Loaded %d match entries from %s", manager->count, path);
}
