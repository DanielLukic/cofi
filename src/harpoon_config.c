#include "harpoon_config.h"
#include "log.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

// Helper function to parse harpoon slot data from a line
static void parse_harpoon_slot_line(const char *line, HarpoonSlot *temp_slot, int *slot) {
    if (strstr(line, "\"slot\":")) {
        sscanf(line, " \"slot\": %d", slot);
    } else if (strstr(line, "\"window_id\":")) {
        sscanf(line, " \"window_id\": %lu", &temp_slot->id);
    } else if (strstr(line, "\"title\":")) {
        char *colon = strchr(line, ':');
        if (colon) {
            char *start = strchr(colon + 1, '"');
            if (start) {
                start++;  // Move past the opening quote
                char *end = strchr(start, '"');
                if (end) {
                    int len = end - start;
                    if (len >= (int)sizeof(temp_slot->title)) len = sizeof(temp_slot->title) - 1;
                    safe_string_copy(temp_slot->title, start, len + 1);
                }
            }
        }
    } else if (strstr(line, "\"class_name\":")) {
        char *colon = strchr(line, ':');
        if (colon) {
            char *start = strchr(colon + 1, '"');
            if (start) {
                start++;  // Move past the opening quote
                char *end = strchr(start, '"');
                if (end) {
                    int len = end - start;
                    if (len >= (int)sizeof(temp_slot->class_name)) len = sizeof(temp_slot->class_name) - 1;
                    safe_string_copy(temp_slot->class_name, start, len + 1);
                }
            }
        }
    } else if (strstr(line, "\"instance\":")) {
        char *colon = strchr(line, ':');
        if (colon) {
            char *start = strchr(colon + 1, '"');
            if (start) {
                start++;  // Move past the opening quote
                char *end = strchr(start, '"');
                if (end) {
                    int len = end - start;
                    if (len >= (int)sizeof(temp_slot->instance)) len = sizeof(temp_slot->instance) - 1;
                    safe_string_copy(temp_slot->instance, start, len + 1);
                }
            }
        }
    } else if (strstr(line, "\"type\":")) {
        char *colon = strchr(line, ':');
        if (colon) {
            char *start = strchr(colon + 1, '"');
            if (start) {
                start++;  // Move past the opening quote
                char *end = strchr(start, '"');
                if (end) {
                    int len = end - start;
                    if (len >= (int)sizeof(temp_slot->type)) len = sizeof(temp_slot->type) - 1;
                    safe_string_copy(temp_slot->type, start, len + 1);
                }
            }
        }
    }
}

static void load_legacy_harpoon_slots(HarpoonManager *harpoon) {
    FILE *file = fopen(harpoon->store.path, "r");
    if (!file) {
        return;
    }

    char line[1024];
    int in_slots = 0;
    int slot = -1;
    HarpoonSlot temp_slot = {0};

    while (fgets(line, sizeof(line), file)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        if (strstr(p, "\"harpoon_slots\":")) {
            in_slots = 1;
        } else if (strstr(p, "}")) {
            if (in_slots && slot >= 0 && slot < MAX_HARPOON_SLOTS && temp_slot.id != 0) {
                temp_slot.assigned = 1;
                harpoon->slots[slot] = temp_slot;

                char payload[SLOT_STORE_PAYLOAD_LEN];
                serialize_window_slot_payload(&temp_slot, payload, sizeof(payload));
                slot_assign(&harpoon->store, slot_key_from_index(slot),
                            harpoon_tab_id(), payload);
                slot = -1;
                memset(&temp_slot, 0, sizeof(temp_slot));
            }
        }

        if (in_slots) {
            parse_harpoon_slot_line(p, &temp_slot, &slot);
        }
    }

    fclose(file);
}

static void hydrate_window_slots_from_store(HarpoonManager *harpoon) {
    for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
        const char *payload = slot_lookup(&harpoon->store, harpoon_tab_id(),
                                          slot_key_from_index(i));
        if (!payload) {
            continue;
        }

        HarpoonSlot slot;
        if (deserialize_window_slot_payload(payload, &slot)) {
            harpoon->slots[i] = slot;
        }
    }
}

static void sync_window_slots_to_store(const HarpoonManager *harpoon,
                                       SlotStore *store) {
    for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
        if (!harpoon->slots[i].assigned) {
            if (slot_lookup(store, harpoon_tab_id(), slot_key_from_index(i))) {
                slot_clear(store, harpoon_tab_id(), slot_key_from_index(i));
            }
            continue;
        }

        char payload[SLOT_STORE_PAYLOAD_LEN];
        serialize_window_slot_payload(&harpoon->slots[i], payload, sizeof(payload));
        slot_assign(store, slot_key_from_index(i), harpoon_tab_id(), payload);
    }
}

// Save harpoon slots to separate config file
void save_harpoon_slots(const HarpoonManager *harpoon) {
    if (!harpoon) return;

    SlotStore *store = &((HarpoonManager *)harpoon)->store;
    sync_window_slots_to_store(harpoon, store);
    if (slot_save(store)) {
        log_debug("Saved slot store to %s", store->path);
    }
}

// Load harpoon slots from separate config file
void load_harpoon_slots(HarpoonManager *harpoon) {
    if (!harpoon) return;

    gboolean loaded_generic = slot_load(&harpoon->store);
    if (loaded_generic) {
        hydrate_window_slots_from_store(harpoon);
        log_info("Loaded slots from %s", harpoon->store.path);
        return;
    }

    load_legacy_harpoon_slots(harpoon);
    log_info("Loaded legacy harpoon slots from %s", harpoon->store.path);
}
