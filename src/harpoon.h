#ifndef HARPOON_H
#define HARPOON_H

#include <X11/Xlib.h>
#include <stdbool.h>
#include "slot_store.h"
#include "types.h"
#include "match_entry.h"

// Forward declare WindowAlignment
struct AppData;

// Structure to store a harpoon assignment
typedef struct HarpoonSlot {
    int match_id;
    int assigned;  // 1 if slot is assigned, 0 otherwise
} HarpoonSlot;

// Structure to manage all harpoon assignments
typedef struct HarpoonManager {
    SlotStore store;
    MatchEntryManager *matching;
    WindowInfo *windows;
    int *window_count;
    HarpoonSlot slots[MAX_HARPOON_SLOTS];  // Slots 0-9 and a-z (excluding h,j,k,l,u)
} HarpoonManager;

// Function declarations
void init_harpoon_manager(HarpoonManager *manager);
void assign_window_to_slot(HarpoonManager *manager, int slot, const WindowInfo *window);
void unassign_slot(HarpoonManager *manager, int slot);
int get_window_slot(const HarpoonManager *manager, Window id);
Window get_slot_window(const HarpoonManager *manager, int slot);
int is_slot_assigned(const HarpoonManager *manager, int slot);
const char *harpoon_tab_id(void);
char *serialize_window_slot_payload(const HarpoonSlot *slot, char *out, size_t out_size);
bool deserialize_window_slot_payload(const char *payload, HarpoonSlot *slot);

#endif // HARPOON_H
