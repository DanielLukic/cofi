#ifndef SLOT_OVERLAY_H
#define SLOT_OVERLAY_H

#include <X11/Xlib.h>
#include <glib.h>

#define MAX_SLOT_OVERLAYS 9

typedef struct {
    Window windows[MAX_SLOT_OVERLAYS];
    int count;
    guint timeout_id;  // g_timeout_add ID for auto-destroy
} SlotOverlayState;

// Forward declaration
typedef struct AppData AppData;

// Show number overlays centered on each assigned workspace slot window
void show_slot_overlays(AppData *app);

// Destroy any active slot overlays immediately
void destroy_slot_overlays(AppData *app);

// Initialize overlay state
void init_slot_overlay_state(SlotOverlayState *state);

#endif // SLOT_OVERLAY_H
