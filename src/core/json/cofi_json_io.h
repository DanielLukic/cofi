#ifndef COFI_JSON_IO_H
#define COFI_JSON_IO_H

#include <stdbool.h>

#include <json-glib/json-glib.h>

/* Caller MUST keep the returned JsonParser alive for the lifetime of any
 * JsonObject/JsonNode borrowed from it.
 * Returns NULL on missing/empty/corrupt/non-object-root.
 */
JsonParser *cofi_json_load_object_file(const char *path);

/* Writes <path>.tmp in same directory then rename(). Target file is left
 * untouched on any failure. No fsync — no mid-write crash protection.
 */
bool cofi_json_save_root(const char *path, JsonNode *root);

/* Returned pointer is owned by the parser tree — copy if storing past parser
 * lifetime. When `fallback` is NULL and the key is missing or wrong-type,
 * returns NULL (so `if (s)` works as expected).
 */
const char *cofi_json_obj_str_or(JsonObject *obj, const char *key, const char *fallback, gboolean *present);
int cofi_json_obj_int_or(JsonObject *obj, const char *key, int fallback, gboolean *present);
gboolean cofi_json_obj_bool_or(JsonObject *obj, const char *key, gboolean fallback, gboolean *present);

/* Returned pointer is owned by the parser tree — copy if storing past parser
 * lifetime.
 */
JsonArray *cofi_json_obj_array(JsonObject *obj, const char *key);

/* Returned pointer is owned by the parser tree — copy if storing past parser
 * lifetime.
 */
JsonObject *cofi_json_obj_object(JsonObject *obj, const char *key);

#endif
