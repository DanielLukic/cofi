#ifndef HARPOON_H
#define HARPOON_H

#include <X11/Xlib.h>
#include <stdbool.h>
#include "types.h"

// Forward declare WindowAlignment
struct AppData;

// Structure to store a harpoon assignment
typedef struct HarpoonSlot {
    Window id;
    char title[MAX_TITLE_LEN];
    char class_name[MAX_CLASS_LEN];
    char instance[MAX_CLASS_LEN];
    char type[16];
    int assigned;  // 1 if slot is assigned, 0 otherwise
} HarpoonSlot;

// Structure to manage all harpoon assignments
typedef struct HarpoonManager {
    HarpoonSlot slots[MAX_HARPOON_SLOTS];  // Slots 0-9 and a-z (excluding h,j,k,l,u)
} HarpoonManager;

// Function declarations
void init_harpoon_manager(HarpoonManager *manager);
void assign_window_to_slot(HarpoonManager *manager, int slot, const WindowInfo *window);
void unassign_slot(HarpoonManager *manager, int slot);
int get_window_slot(const HarpoonManager *manager, Window id);
Window get_slot_window(const HarpoonManager *manager, int slot);
int is_slot_assigned(const HarpoonManager *manager, int slot);



// Automatic reassignment functions
// Returns true if any slots were reassigned
bool check_and_reassign_windows(HarpoonManager *manager, WindowInfo *windows, int window_count);

#endif // HARPOON_H