#include "names/names_store.h"

#include <json-glib/json-glib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "core/app/app_data.h"
#include "core/json/cofi_json_io.h"
#include "core/log/log.h"
#include "core/utils/utils.h"
#include "matching/match_entry_config.h"

static void build_default_path(char *out, size_t out_size) {
    const char *home = getenv("HOME");
    if (!home) home = ".";

    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.config", home);
    mkdir(dir, 0755);
    snprintf(dir, sizeof(dir), "%s/.config/cofi", home);
    mkdir(dir, 0755);
    snprintf(out, out_size, "%s/.config/cofi/names.json", home);
}

void names_store_init(NamesStore *store) {
    if (!store) return;
    memset(store, 0, sizeof(*store));
    build_default_path(store->path, sizeof(store->path));
}

void names_store_init_with_path(NamesStore *store, const char *path) {
    if (!store) return;
    names_store_init(store);
    if (path && path[0] != '\0') {
        safe_string_copy(store->path, path, sizeof(store->path));
    }
}

int names_store_find_index_by_match_id(const NamesStore *store, int match_id) {
    if (!store || match_id <= 0) return -1;
    for (int i = 0; i < store->count; i++) {
        if (store->records[i].match_id == match_id) return i;
    }
    return -1;
}

const char *names_store_get_by_match_id(const NamesStore *store, int match_id) {
    int idx = names_store_find_index_by_match_id(store, match_id);
    if (idx < 0) return NULL;
    return store->records[idx].custom_name[0] ? store->records[idx].custom_name : NULL;
}

NameRecord *names_store_find_by_custom_name(NamesStore *store, const char *name) {
    if (!store || !name) return NULL;
    for (int i = 0; i < store->count; i++) {
        if (strcmp(store->records[i].custom_name, name) == 0) {
            return &store->records[i];
        }
    }
    return NULL;
}

bool names_store_set(NamesStore *store, int match_id, const char *name) {
    if (!store || match_id <= 0 || !name || name[0] == '\0') return false;

    int idx = names_store_find_index_by_match_id(store, match_id);
    if (idx < 0) {
        if (store->count >= MAX_WINDOWS) return false;
        idx = store->count++;
    }

    store->records[idx].match_id = match_id;
    safe_string_copy(store->records[idx].custom_name, name,
                     sizeof(store->records[idx].custom_name));
    return true;
}

bool names_store_remove_by_match_id(NamesStore *store, int match_id) {
    int idx = names_store_find_index_by_match_id(store, match_id);
    if (idx < 0) return false;

    for (int i = idx; i < store->count - 1; i++) {
        store->records[i] = store->records[i + 1];
    }
    store->count--;
    memset(&store->records[store->count], 0, sizeof(store->records[store->count]));
    return true;
}

int names_store_collect_ids(const NamesStore *store, int *out, int max) {
    if (!store || !out || max <= 0) return 0;
    int count = 0;
    for (int i = 0; i < store->count && count < max; i++) {
        if (store->records[i].match_id > 0) {
            out[count++] = store->records[i].match_id;
        }
    }
    return count;
}

bool names_store_save(const NamesStore *store) {
    if (!store || store->path[0] == '\0') return false;

    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "names");
    json_builder_begin_array(builder);
    for (int i = 0; i < store->count; i++) {
        const NameRecord *record = &store->records[i];
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "match_id");
        json_builder_add_int_value(builder, record->match_id);
        json_builder_set_member_name(builder, "custom_name");
        json_builder_add_string_value(builder, record->custom_name);
        json_builder_end_object(builder);
    }
    json_builder_end_array(builder);
    json_builder_end_object(builder);

    JsonNode *root = json_builder_get_root(builder);
    bool ok = cofi_json_save_root(store->path, root);
    json_node_unref(root);
    g_object_unref(builder);
    return ok;
}

bool names_store_load(NamesStore *store) {
    if (!store) return false;

    char path[sizeof(store->path)];
    safe_string_copy(path, store->path, sizeof(path));
    store->count = 0;
    memset(store->records, 0, sizeof(store->records));
    safe_string_copy(store->path, path, sizeof(store->path));

    JsonParser *parser = cofi_json_load_object_file(store->path);
    if (!parser) return true;

    JsonObject *root = json_node_get_object(json_parser_get_root(parser));
    JsonArray *names = cofi_json_obj_array(root, "names");
    if (names) {
        guint n = json_array_get_length(names);
        for (guint i = 0; i < n && store->count < MAX_WINDOWS; i++) {
            JsonNode *node = json_array_get_element(names, i);
            if (!node || !JSON_NODE_HOLDS_OBJECT(node)) continue;

            JsonObject *obj = json_node_get_object(node);
            int match_id = cofi_json_obj_int_or(obj, "match_id", 0, NULL);
            const char *name = cofi_json_obj_str_or(obj, "custom_name", "", NULL);
            if (match_id <= 0 || !name || name[0] == '\0') continue;
            names_store_set(store, match_id, name);
        }
    }

    g_object_unref(parser);
    return true;
}

const char *names_get_for_window(const NamesStore *store,
                                 const MatchEntryManager *manager,
                                 const WindowInfo *window) {
    if (!store || !manager || !window) return NULL;

    for (int i = 0; i < store->count; i++) {
        int entry_idx = match_entry_find_index_by_match_id(manager, store->records[i].match_id);
        if (entry_idx < 0) continue;
        const MatchEntry *entry = &manager->entries[entry_idx];
        if (match_entry_matches_window(entry, window)) {
            return store->records[i].custom_name;
        }
    }
    return NULL;
}

bool names_assign_window(AppData *app, WindowInfo *window, const char *name) {
    if (!app || !window || !name || name[0] == '\0') return false;

    for (int i = 0; i < app->names.count; i++) {
        int entry_idx = match_entry_find_index_by_match_id(&app->matching,
                                                           app->names.records[i].match_id);
        if (entry_idx < 0) continue;
        MatchEntry *entry = &app->matching.entries[entry_idx];
        if (!match_entry_matches_window(entry, window)) continue;

        entry->bound_x11_id = window->id;
        entry->assigned = 1;
        if (!names_store_set(&app->names, entry->match_id, name)) return false;
        save_match_entries(&app->matching);
        names_store_save(&app->names);
        return true;
    }

    int match_id = matching_create_entry(&app->matching, window);
    if (match_id <= 0) return false;
    if (!names_store_set(&app->names, match_id, name)) return false;

    save_match_entries(&app->matching);
    names_store_save(&app->names);
    return true;
}
