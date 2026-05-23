#include "slot_store.h"

#include <errno.h>
#include <glib.h>
#include <json-glib/json-glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "log.h"
#include "cofi_json_io.h"

static const char *default_slot_path(void) {
    static char path[512];
    const char *home = getenv("HOME");
    if (!home) {
        home = ".";
    }

    g_snprintf(path, sizeof(path), "%s/.config", home);
    mkdir(path, 0755);
    g_snprintf(path, sizeof(path), "%s/.config/cofi", home);
    mkdir(path, 0755);
    g_snprintf(path, sizeof(path), "%s/.config/cofi/harpoon.json", home);
    return path;
}

void slot_store_init(SlotStore *store) {
    if (!store) {
        return;
    }

    memset(store, 0, sizeof(*store));
    g_strlcpy(store->path, default_slot_path(), sizeof(store->path));
}

void slot_store_init_with_path(SlotStore *store, const char *path) {
    slot_store_init(store);
    if (store && path && path[0] != '\0') {
        g_strlcpy(store->path, path, sizeof(store->path));
    }
}

void slot_store_free(SlotStore *store) {
    if (!store) {
        return;
    }

    g_free(store->entries);
    store->entries = NULL;
    store->count = 0;
    store->capacity = 0;
}

int slot_index_from_key(char slot_key) {
    if (slot_key >= '0' && slot_key <= '9') {
        return slot_key - '0';
    }
    if (slot_key >= 'a' && slot_key <= 'z') {
        return 10 + (slot_key - 'a');
    }
    if (slot_key >= 'A' && slot_key <= 'Z') {
        return 10 + (slot_key - 'A');
    }
    return -1;
}

char slot_key_from_index(int slot_index) {
    if (slot_index >= 0 && slot_index <= 9) {
        return (char)('0' + slot_index);
    }
    if (slot_index >= 10 && slot_index < MAX_HARPOON_SLOTS) {
        return (char)('a' + (slot_index - 10));
    }
    return '\0';
}

bool slot_parse_at_key_arg(const char *args, char *slot_key) {
    if (!args) return false;
    while (g_ascii_isspace(*args)) args++;
    if (*args != '@') return false;
    args++;
    if (!args[0]) return false;

    char key = args[0];
    args++;
    while (g_ascii_isspace(*args)) args++;
    if (*args != '\0' || slot_index_from_key(key) < 0) return false;

    if (slot_key) *slot_key = key;
    return true;
}

static SlotEntry *find_entry(SlotStore *store,
                             const char *tab_id,
                             char slot_key) {
    if (!store || !tab_id) {
        return NULL;
    }

    for (size_t i = 0; i < store->count; i++) {
        SlotEntry *entry = &store->entries[i];
        if (entry->assigned &&
            entry->slot_key == slot_key &&
            strcmp(entry->tab_id, tab_id) == 0) {
            return entry;
        }
    }
    return NULL;
}

static const SlotEntry *find_const_entry(const SlotStore *store,
                                         const char *tab_id,
                                         char slot_key) {
    return find_entry((SlotStore *)store, tab_id, slot_key);
}

static SlotEntry *append_entry(SlotStore *store) {
    if (store->count == store->capacity) {
        size_t next_capacity = store->capacity == 0 ? MAX_HARPOON_SLOTS : store->capacity * 2;
        SlotEntry *next = g_realloc(store->entries, next_capacity * sizeof(SlotEntry));
        if (!next) {
            return NULL;
        }
        store->entries = next;
        store->capacity = next_capacity;
    }

    SlotEntry *entry = &store->entries[store->count++];
    memset(entry, 0, sizeof(*entry));
    return entry;
}

void slot_assign(SlotStore *store, char slot_key,
                 const char *tab_id, const char *payload) {
    int index = slot_index_from_key(slot_key);
    if (!store || index < 0 || !tab_id || !payload || payload[0] == '\0') {
        return;
    }

    SlotEntry *entry = find_entry(store, tab_id, slot_key);
    if (!entry) {
        entry = append_entry(store);
        if (!entry) {
            log_error("Failed to allocate slot store entry");
            return;
        }
    }
    entry->slot_key = slot_key_from_index(index);
    g_strlcpy(entry->tab_id, tab_id, sizeof(entry->tab_id));
    g_strlcpy(entry->payload, payload, sizeof(entry->payload));
    entry->assigned = 1;
}

void slot_clear(SlotStore *store, const char *tab_id, char slot_key) {
    int index = slot_index_from_key(slot_key);
    if (!store || !tab_id || index < 0) {
        return;
    }

    for (size_t i = 0; i < store->count; i++) {
        SlotEntry *entry = &store->entries[i];
        if (entry->assigned &&
            entry->slot_key == slot_key_from_index(index) &&
            strcmp(entry->tab_id, tab_id) == 0) {
            if (i + 1 < store->count) {
                memmove(entry, entry + 1,
                        (store->count - i - 1) * sizeof(SlotEntry));
            }
            store->count--;
            return;
        }
    }
}

const char *slot_lookup(const SlotStore *store,
                        const char *tab_id, char slot_key) {
    int index = slot_index_from_key(slot_key);
    if (!store || !tab_id || index < 0) {
        return NULL;
    }

    const SlotEntry *entry = find_const_entry(store, tab_id, slot_key_from_index(index));
    if (!entry) {
        return NULL;
    }
    return entry->payload;
}

char slot_for_payload(const SlotStore *store,
                      const char *tab_id, const char *payload) {
    if (!store || !tab_id || !payload) {
        return '\0';
    }

    for (size_t i = 0; i < store->count; i++) {
        const SlotEntry *entry = &store->entries[i];
        if (entry->assigned &&
            strcmp(entry->tab_id, tab_id) == 0 &&
            strcmp(entry->payload, payload) == 0) {
            return entry->slot_key;
        }
    }
    return '\0';
}

bool slot_save(const SlotStore *store) {
    if (!store || store->path[0] == '\0') {
        return false;
    }

    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "slots");
    json_builder_begin_array(builder);

    for (size_t i = 0; i < store->count; i++) {
        const SlotEntry *entry = &store->entries[i];
        if (!entry->assigned) {
            continue;
        }

        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "slot");
        char slot[2] = {entry->slot_key, '\0'};
        json_builder_add_string_value(builder, slot);
        json_builder_set_member_name(builder, "tab");
        json_builder_add_string_value(builder, entry->tab_id);
        json_builder_set_member_name(builder, "payload");
        json_builder_add_string_value(builder, entry->payload);
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

bool slot_load(SlotStore *store) {
    if (!store || store->path[0] == '\0') {
        return false;
    }

    char path[512];
    g_strlcpy(path, store->path, sizeof(path));
    slot_store_free(store);
    g_strlcpy(store->path, path, sizeof(store->path));

    JsonParser *parser = cofi_json_load_object_file(store->path);
    if (!parser) {
        return false;
    }

    JsonObject *root = json_node_get_object(json_parser_get_root(parser));
    JsonArray *slots = cofi_json_obj_array(root, "slots");
    if (!slots) {
        g_object_unref(parser);
        return false;
    }

    bool loaded = false;
    guint n = json_array_get_length(slots);
    for (guint i = 0; i < n; i++) {
        JsonNode *node = json_array_get_element(slots, i);
        if (!node || !JSON_NODE_HOLDS_OBJECT(node)) {
            continue;
        }

        JsonObject *entry = json_node_get_object(node);
        const char *slot = cofi_json_obj_str_or(entry, "slot", "", NULL);
        const char *tab = cofi_json_obj_str_or(entry, "tab", "", NULL);
        const char *payload = cofi_json_obj_str_or(entry, "payload", "", NULL);
        if (!slot || slot[0] == '\0' || slot[1] != '\0' ||
            !tab || tab[0] == '\0' ||
            !payload || payload[0] == '\0') {
            continue;
        }

        slot_assign(store, slot[0], tab, payload);
        if (slot_lookup(store, tab, slot[0])) {
            loaded = true;
        }
    }

    g_object_unref(parser);
    return loaded;
}
