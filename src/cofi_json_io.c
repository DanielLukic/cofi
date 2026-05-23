#include "cofi_json_io.h"

#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "log.h"

static void set_present(gboolean *present, gboolean value) {
    if (present) {
        *present = value;
    }
}

JsonParser *cofi_json_load_object_file(const char *path) {
    if (!path || path[0] == '\0') {
        return NULL;
    }

    GStatBuf st;
    if (g_stat(path, &st) != 0) {
        if (errno == ENOENT) {
            return NULL;
        }
        log_error("cofi_json_io: failed to stat '%s': %s", path, g_strerror(errno));
        return NULL;
    }

    if (st.st_size == 0) {
        log_error("cofi_json_io: JSON file is empty: %s", path);
        return NULL;
    }

    JsonParser *parser = json_parser_new();
    GError *error = NULL;
    if (!json_parser_load_from_file(parser, path, &error)) {
        log_error("cofi_json_io: failed to parse JSON '%s': %s", path,
                  error ? error->message : "unknown error");
        g_clear_error(&error);
        g_object_unref(parser);
        return NULL;
    }

    JsonNode *root = json_parser_get_root(parser);
    if (!root || !JSON_NODE_HOLDS_OBJECT(root)) {
        log_error("cofi_json_io: JSON root is not an object: %s", path);
        g_object_unref(parser);
        return NULL;
    }

    return parser;
}

bool cofi_json_save_root(const char *path, JsonNode *root) {
    if (!path || path[0] == '\0' || !root) {
        return false;
    }

    JsonGenerator *generator = json_generator_new();
    json_generator_set_pretty(generator, TRUE);
    json_generator_set_root(generator, root);

    char tmp_path[1024];
    g_snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    GError *error = NULL;
    if (!json_generator_to_file(generator, tmp_path, &error)) {
        log_error("cofi_json_io: failed to write temp JSON '%s': %s", tmp_path,
                  error ? error->message : "unknown error");
        g_clear_error(&error);
        g_object_unref(generator);
        return false;
    }

    if (rename(tmp_path, path) != 0) {
        log_error("cofi_json_io: failed to rename '%s' to '%s': %s",
                  tmp_path, path, g_strerror(errno));
        unlink(tmp_path);
        g_object_unref(generator);
        return false;
    }

    g_object_unref(generator);
    return true;
}

const char *cofi_json_obj_str_or(JsonObject *obj, const char *key, const char *fallback, gboolean *present) {
    if (!fallback) {
        fallback = "";
    }
    if (!obj || !key || key[0] == '\0') {
        set_present(present, FALSE);
        return fallback;
    }
    if (!json_object_has_member(obj, key)) {
        log_trace("cofi_json_io: missing string key '%s'", key);
        set_present(present, FALSE);
        return fallback;
    }
    JsonNode *node = json_object_get_member(obj, key);
    if (!node || json_node_is_null(node) || !JSON_NODE_HOLDS_VALUE(node) ||
        json_node_get_value_type(node) != G_TYPE_STRING) {
        log_trace("cofi_json_io: key '%s' is not a string", key);
        set_present(present, FALSE);
        return fallback;
    }
    set_present(present, TRUE);
    return json_node_get_string(node);
}

int cofi_json_obj_int_or(JsonObject *obj, const char *key, int fallback, gboolean *present) {
    if (!obj || !key || key[0] == '\0') {
        set_present(present, FALSE);
        return fallback;
    }
    if (!json_object_has_member(obj, key)) {
        log_trace("cofi_json_io: missing int key '%s'", key);
        set_present(present, FALSE);
        return fallback;
    }
    JsonNode *node = json_object_get_member(obj, key);
    if (!node || json_node_is_null(node) || !JSON_NODE_HOLDS_VALUE(node) ||
        json_node_get_value_type(node) != G_TYPE_INT64) {
        log_trace("cofi_json_io: key '%s' is not an int", key);
        set_present(present, FALSE);
        return fallback;
    }
    set_present(present, TRUE);
    return (int)json_node_get_int(node);
}

gboolean cofi_json_obj_bool_or(JsonObject *obj, const char *key, gboolean fallback, gboolean *present) {
    if (!obj || !key || key[0] == '\0') {
        set_present(present, FALSE);
        return fallback;
    }
    if (!json_object_has_member(obj, key)) {
        log_trace("cofi_json_io: missing bool key '%s'", key);
        set_present(present, FALSE);
        return fallback;
    }
    JsonNode *node = json_object_get_member(obj, key);
    if (!node || json_node_is_null(node) || !JSON_NODE_HOLDS_VALUE(node) ||
        json_node_get_value_type(node) != G_TYPE_BOOLEAN) {
        log_trace("cofi_json_io: key '%s' is not a bool", key);
        set_present(present, FALSE);
        return fallback;
    }
    set_present(present, TRUE);
    return json_node_get_boolean(node);
}

JsonArray *cofi_json_obj_array(JsonObject *obj, const char *key) {
    if (!obj || !key || !json_object_has_member(obj, key)) {
        return NULL;
    }
    JsonNode *node = json_object_get_member(obj, key);
    if (!node || json_node_get_node_type(node) != JSON_NODE_ARRAY) {
        log_trace("cofi_json_io: key '%s' is not an array", key);
        return NULL;
    }
    return json_node_get_array(node);
}

JsonObject *cofi_json_obj_object(JsonObject *obj, const char *key) {
    if (!obj || !key || !json_object_has_member(obj, key)) {
        return NULL;
    }
    JsonNode *node = json_object_get_member(obj, key);
    if (!node || json_node_get_node_type(node) != JSON_NODE_OBJECT) {
        log_trace("cofi_json_io: key '%s' is not an object", key);
        return NULL;
    }
    return json_node_get_object(node);
}
