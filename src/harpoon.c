#include "harpoon.h"

#include <stdio.h>
#include <string.h>

#include "log.h"
#include "utils.h"

const char *harpoon_tab_id(void) {
    return "windows";
}

char *serialize_window_slot_payload(const HarpoonSlot *slot, char *out, size_t out_size) {
    if (!slot || !out || out_size == 0) {
        return NULL;
    }
    snprintf(out, out_size, "%d", slot->match_id);
    return out;
}

bool deserialize_window_slot_payload(const char *payload, HarpoonSlot *slot) {
    if (!payload || !slot) {
        return false;
    }

    memset(slot, 0, sizeof(*slot));
    slot->match_id = (int)strtol(payload, NULL, 10);
    slot->assigned = slot->match_id > 0;
    return slot->assigned;
}

void init_harpoon_manager(HarpoonManager *manager) {
    if (!manager) return;

    slot_store_init(&manager->store);
    manager->matching = NULL;
    manager->windows = NULL;
    manager->window_count = NULL;

    for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
        manager->slots[i].assigned = 0;
        manager->slots[i].match_id = 0;
    }
}

void assign_window_to_slot(HarpoonManager *manager, int slot, const WindowInfo *window) {
    if (!manager || !window || slot < 0 || slot >= MAX_HARPOON_SLOTS) return;
    if (!manager->matching || !manager->windows || !manager->window_count) return;

    int match_id = matching_capture_or_get(manager->matching,
                                           manager->windows,
                                           *manager->window_count,
                                           window);
    if (match_id <= 0) {
        log_warn("Unable to capture matching entry for harpoon slot %d", slot);
        return;
    }

    manager->slots[slot].match_id = match_id;
    manager->slots[slot].assigned = 1;

    char payload[SLOT_STORE_PAYLOAD_LEN];
    serialize_window_slot_payload(&manager->slots[slot], payload, sizeof(payload));
    slot_assign(&manager->store, slot_key_from_index(slot), harpoon_tab_id(), payload);
}

void unassign_slot(HarpoonManager *manager, int slot) {
    if (!manager || slot < 0 || slot >= MAX_HARPOON_SLOTS) return;

    manager->slots[slot].assigned = 0;
    manager->slots[slot].match_id = 0;
    slot_clear(&manager->store, harpoon_tab_id(), slot_key_from_index(slot));
}

int harpoon_gc_unreferenced_match_entries(HarpoonManager *manager) {
    if (!manager || !manager->matching) return 0;

    // Compose consumer references here so more matching consumers can append ids
    // without teaching match_entry.c about harpoon, geom, or any other owner.
    int referenced_ids[MAX_HARPOON_SLOTS + (MAX_WINDOWS * 2)];
    int referenced_count = 0;
    for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
        if (!manager->slots[i].assigned || manager->slots[i].match_id <= 0) {
            continue;
        }
        referenced_ids[referenced_count++] = manager->slots[i].match_id;
    }

    referenced_count += match_entry_collect_labeled_ids(manager->matching,
                                                        referenced_ids + referenced_count,
                                                        (int)(sizeof(referenced_ids) / sizeof(referenced_ids[0])) - referenced_count);

    referenced_count += match_entry_collect_geom_ids(manager->matching,
                                                     referenced_ids + referenced_count,
                                                     (int)(sizeof(referenced_ids) / sizeof(referenced_ids[0])) - referenced_count);

    return match_entry_gc(manager->matching, referenced_ids, referenced_count);
}

static MatchEntry *resolve_slot_entry(const HarpoonManager *manager, int slot) {
    if (!manager || slot < 0 || slot >= MAX_HARPOON_SLOTS) return NULL;
    if (!manager->matching) return NULL;
    const HarpoonSlot *harpoon_slot = &manager->slots[slot];
    if (!harpoon_slot->assigned || harpoon_slot->match_id <= 0) return NULL;
    int idx = match_entry_find_index_by_match_id(manager->matching, harpoon_slot->match_id);
    if (idx < 0 || idx >= manager->matching->count) return NULL;
    return &manager->matching->entries[idx];
}

Window get_slot_window(const HarpoonManager *manager, int slot) {
    MatchEntry *entry = resolve_slot_entry(manager, slot);
    if (!entry) return 0;

    if (entry->assigned && entry->bound_x11_id != 0) {
        return entry->bound_x11_id;
    }

    if (!manager->windows || !manager->window_count || !manager->matching) {
        return 0;
    }

    int changed = match_entry_reassign_live_windows(manager->matching, manager->windows,
                                                    *manager->window_count);
    if (changed) {
        entry = resolve_slot_entry(manager, slot);
    }
    return (entry && entry->assigned) ? entry->bound_x11_id : 0;
}

int get_window_slot(const HarpoonManager *manager, Window id) {
    if (!manager || id == 0 || !manager->matching) return -1;

    for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
        MatchEntry *entry = resolve_slot_entry(manager, i);
        if (!entry) continue;
        if (entry->assigned && entry->bound_x11_id == id) {
            return i;
        }
    }
    return -1;
}

int is_slot_assigned(const HarpoonManager *manager, int slot) {
    if (!manager || slot < 0 || slot >= MAX_HARPOON_SLOTS) return 0;
    return manager->slots[slot].assigned;
}
