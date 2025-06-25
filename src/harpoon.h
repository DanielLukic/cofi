#ifndef HARPOON_H
#define HARPOON_H

#include <X11/Xlib.h>
#include "window_info.h"

#define MAX_HARPOON_SLOTS 36

// Structure to store a harpoon assignment
typedef struct {
    Window id;
    char title[MAX_TITLE_LEN];
    char class_name[MAX_CLASS_LEN];
    char instance[MAX_CLASS_LEN];
    char type[16];
    int assigned;  // 1 if slot is assigned, 0 otherwise
} HarpoonSlot;

// Structure to manage all harpoon assignments
typedef struct {
    HarpoonSlot slots[MAX_HARPOON_SLOTS];  // Slots 0-9 and a-z (excluding h,j,k,l,u)
} HarpoonManager;

// Function declarations
void init_harpoon_manager(HarpoonManager *manager);
void assign_window_to_slot(HarpoonManager *manager, int slot, const WindowInfo *window);
void unassign_slot(HarpoonManager *manager, int slot);
int get_window_slot(const HarpoonManager *manager, Window id);
Window get_slot_window(const HarpoonManager *manager, int slot);
int is_slot_assigned(const HarpoonManager *manager, int slot);

// Persistence functions
void save_harpoon_config(const HarpoonManager *manager);
void load_harpoon_config(HarpoonManager *manager);

// Automatic reassignment functions
void check_and_reassign_windows(HarpoonManager *manager, WindowInfo *windows, int window_count);

#endif // HARPOON_H