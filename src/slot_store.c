#include "slot_store.h"

#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "log.h"

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

static char *json_escape(const char *text) {
    GString *out = g_string_new(NULL);
    for (const char *p = text ? text : ""; *p; p++) {
        switch (*p) {
            case '\\': g_string_append(out, "\\\\"); break;
            case '"': g_string_append(out, "\\\""); break;
            case '\n': g_string_append(out, "\\n"); break;
            case '\t': g_string_append(out, "\\t"); break;
            default: g_string_append_c(out, *p); break;
        }
    }
    return g_string_free(out, FALSE);
}

bool slot_save(const SlotStore *store) {
    if (!store || store->path[0] == '\0') {
        return false;
    }

    FILE *file = fopen(store->path, "w");
    if (!file) {
        log_error("Failed to open slot store for writing: %s", store->path);
        return false;
    }

    fprintf(file, "{\n  \"slots\": [\n");
    int first = 1;
    for (size_t i = 0; i < store->count; i++) {
        const SlotEntry *entry = &store->entries[i];
        if (!entry->assigned) {
            continue;
        }

        char *tab = json_escape(entry->tab_id);
        char *payload = json_escape(entry->payload);
        if (!first) {
            fprintf(file, ",\n");
        }
        first = 0;
        fprintf(file,
                "    { \"slot\": \"%c\", \"tab\": \"%s\", \"payload\": \"%s\" }",
                entry->slot_key, tab, payload);
        g_free(tab);
        g_free(payload);
    }
    fprintf(file, "\n  ]\n}\n");
    fclose(file);
    return true;
}

static gboolean extract_json_string(const char *line,
                                    const char *field,
                                    char *out,
                                    size_t out_size) {
    if (!line || !field || !out || out_size == 0) {
        return FALSE;
    }

    char pattern[64];
    g_snprintf(pattern, sizeof(pattern), "\"%s\"", field);
    char *p = strstr((char *)line, pattern);
    if (!p) {
        return FALSE;
    }
    p = strchr(p + strlen(pattern), ':');
    if (!p) {
        return FALSE;
    }
    p = strchr(p, '"');
    if (!p) {
        return FALSE;
    }
    p++;

    size_t pos = 0;
    while (*p && pos + 1 < out_size) {
        if (*p == '"') {
            out[pos] = '\0';
            return TRUE;
        }
        if (*p == '\\' && p[1]) {
            p++;
            if (*p == 'n') {
                out[pos++] = '\n';
            } else if (*p == 't') {
                out[pos++] = '\t';
            } else {
                out[pos++] = *p;
            }
            p++;
            continue;
        }
        out[pos++] = *p++;
    }
    out[pos] = '\0';
    return TRUE;
}

bool slot_load(SlotStore *store) {
    if (!store || store->path[0] == '\0') {
        return false;
    }

    FILE *file = fopen(store->path, "r");
    if (!file) {
        if (errno != ENOENT) {
            log_error("Failed to open slot store for reading: %s", store->path);
        }
        return false;
    }

    slot_store_free(store);

    char line[2048];
    bool loaded = false;
    while (fgets(line, sizeof(line), file)) {
        char slot_text[8] = {0};
        char tab[SLOT_STORE_TAB_ID_LEN] = {0};
        char payload[SLOT_STORE_PAYLOAD_LEN] = {0};

        if (!strstr(line, "\"slot\"") || !strstr(line, "\"tab\"") ||
            !strstr(line, "\"payload\"")) {
            continue;
        }
        if (!extract_json_string(line, "slot", slot_text, sizeof(slot_text)) ||
            !extract_json_string(line, "tab", tab, sizeof(tab)) ||
            !extract_json_string(line, "payload", payload, sizeof(payload))) {
            continue;
        }

        slot_assign(store, slot_text[0], tab, payload);
        loaded = true;
    }

    fclose(file);
    return loaded;
}
