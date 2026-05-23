#ifndef COFI_JSON_IO_H
#define COFI_JSON_IO_H

#include <stdbool.h>

#include <json-glib/json-glib.h>

JsonParser *cofi_json_load_object_file(const char *path);
bool cofi_json_save_root(const char *path, JsonNode *root);

const char *cofi_json_obj_str_or(JsonObject *obj, const char *key, const char *fallback, gboolean *present);
int cofi_json_obj_int_or(JsonObject *obj, const char *key, int fallback, gboolean *present);
gboolean cofi_json_obj_bool_or(JsonObject *obj, const char *key, gboolean fallback, gboolean *present);

JsonArray *cofi_json_obj_array(JsonObject *obj, const char *key);
JsonObject *cofi_json_obj_object(JsonObject *obj, const char *key);

#endif
