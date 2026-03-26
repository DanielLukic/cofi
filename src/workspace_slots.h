#ifndef WORKSPACE_SLOTS_H
#define WORKSPACE_SLOTS_H

#include <X11/Xlib.h>
#include "window_info.h"

#define MAX_WORKSPACE_SLOTS 9

typedef struct {
    Window id;
} WorkspaceSlot;

typedef struct {
    WorkspaceSlot slots[MAX_WORKSPACE_SLOTS];
    int count;
    int workspace;  // workspace these were assigned for
} WorkspaceSlotManager;

// Forward declaration
typedef struct AppData AppData;

// Assign slots to visible windows on the current workspace, sorted by position
void assign_workspace_slots(AppData *app);

// Get window ID for a given slot (1-9), returns 0 if unassigned
Window get_workspace_slot_window(const WorkspaceSlotManager *manager, int slot);

// Initialize the slot manager
void init_workspace_slots(WorkspaceSlotManager *manager);

#endif // WORKSPACE_SLOTS_H
