#ifndef SLOT_STORE_H
#define SLOT_STORE_H

#include <stdbool.h>
#include <stddef.h>

#include "core/utils/types.h"

#define SLOT_STORE_TAB_ID_LEN 32
#define SLOT_STORE_PAYLOAD_LEN 1024

typedef struct {
    char slot_key;
    char tab_id[SLOT_STORE_TAB_ID_LEN];
    char payload[SLOT_STORE_PAYLOAD_LEN];
    int assigned;
} SlotEntry;

typedef struct {
    SlotEntry *entries;
    size_t count;
    size_t capacity;
    char path[512];
} SlotStore;

void slot_store_init(SlotStore *store);
void slot_store_init_with_path(SlotStore *store, const char *path);
void slot_store_free(SlotStore *store);

int slot_index_from_key(char slot_key);
char slot_key_from_index(int slot_index);
bool slot_parse_at_key_arg(const char *args, char *slot_key);

void slot_assign(SlotStore *store, char slot_key,
                 const char *tab_id, const char *payload);
void slot_clear(SlotStore *store, const char *tab_id, char slot_key);
const char *slot_lookup(const SlotStore *store,
                        const char *tab_id, char slot_key);
char slot_for_payload(const SlotStore *store,
                      const char *tab_id, const char *payload);

bool slot_save(const SlotStore *store);
bool slot_load(SlotStore *store);

#endif
