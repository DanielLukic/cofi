#include "harpoon/harpoon_config.h"
#include "core/log/log.h"
#include "core/utils/utils.h"
#include <stdio.h>

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
    log_info("No slot store found at %s (legacy harpoon load disabled)", harpoon->store.path);
}
