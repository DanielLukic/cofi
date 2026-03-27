#include "workspace_slots.h"
#include "app_data.h"
#include "x11_utils.h"
#include "monitor_move.h"
#include "slot_overlay.h"
#include "log.h"
#include <X11/Xatom.h>
#include <stdlib.h>
#include <string.h>

// Row grouping threshold: windows within this many pixels of the same Y
// are considered to be in the same row
#define ROW_THRESHOLD 50

// If a window is covered by more than this fraction, exclude it
#define OCCLUSION_THRESHOLD 0.8

typedef struct {
    Window id;
    int x;
    int y;
    int w;
    int h;
} WindowPosition;

// Get stacking order from _NET_CLIENT_LIST_STACKING
// Returns array of Window IDs (bottom to top), caller must XFree
static Window *get_stacking_order(Display *display, unsigned long *count) {
    Atom atom = XInternAtom(display, "_NET_CLIENT_LIST_STACKING", False);
    Atom actual_type;
    int actual_format;
    unsigned long n_items, bytes_after;
    unsigned char *prop = NULL;

    if (XGetWindowProperty(display, DefaultRootWindow(display), atom,
                           0, 4096, False, XA_WINDOW,
                           &actual_type, &actual_format, &n_items, &bytes_after,
                           &prop) == Success && prop) {
        *count = n_items;
        return (Window *)prop;  // Caller must XFree
    }
    *count = 0;
    return NULL;
}

// Get stacking position of a window (higher = more on top)
static int get_stack_position(Window id, Window *stack, unsigned long stack_count) {
    for (unsigned long i = 0; i < stack_count; i++) {
        if (stack[i] == id) return (int)i;
    }
    return -1;
}

// Compute what fraction of window A is covered by window B
static double compute_overlap_fraction(const WindowPosition *a, const WindowPosition *b) {
    int ix1 = (a->x > b->x) ? a->x : b->x;
    int iy1 = (a->y > b->y) ? a->y : b->y;
    int ix2 = (a->x + a->w < b->x + b->w) ? a->x + a->w : b->x + b->w;
    int iy2 = (a->y + a->h < b->y + b->h) ? a->y + a->h : b->y + b->h;

    if (ix2 <= ix1 || iy2 <= iy1) return 0.0;

    double overlap_area = (double)(ix2 - ix1) * (iy2 - iy1);
    double a_area = (double)a->w * a->h;
    if (a_area <= 0) return 0.0;
    return overlap_area / a_area;
}

// Check if a window is mostly occluded by any single window above it in the stack
static int is_occluded(const WindowPosition *win, int win_stack_pos,
                       const WindowPosition *all, int count,
                       Window *stack, unsigned long stack_count) {
    double total_overlap = 0.0;
    for (int i = 0; i < count; i++) {
        if (all[i].id == win->id) continue;
        int other_pos = get_stack_position(all[i].id, stack, stack_count);
        if (other_pos <= win_stack_pos) continue;  // Below us in stack

        total_overlap += compute_overlap_fraction(win, &all[i]);
        if (total_overlap >= OCCLUSION_THRESHOLD) {
            log_debug("Window 0x%lx is %.0f%% cumulatively occluded, excluding",
                      win->id, total_overlap * 100);
            return 1;
        }
    }
    return 0;
}

static int compare_by_position(const void *a, const void *b) {
    const WindowPosition *wa = (const WindowPosition *)a;
    const WindowPosition *wb = (const WindowPosition *)b;

    // Group into rows: if Y difference is within threshold, same row
    int row_a = wa->y / ROW_THRESHOLD;
    int row_b = wb->y / ROW_THRESHOLD;

    if (row_a != row_b) {
        return row_a - row_b;  // Top rows first
    }
    return wa->x - wb->x;  // Left to right within row
}

void init_workspace_slots(WorkspaceSlotManager *manager) {
    memset(manager, 0, sizeof(WorkspaceSlotManager));
    manager->workspace = -1;
}

void assign_workspace_slots(AppData *app) {
    WorkspaceSlotManager *manager = &app->workspace_slots;
    int current_desktop = get_current_desktop(app->display);

    manager->count = 0;
    manager->workspace = current_desktop;

    // Collect all candidate windows with geometry
    WindowPosition candidates[MAX_WORKSPACE_SLOTS];
    int cand_count = 0;

    for (int i = 0; i < app->window_count && cand_count < MAX_WORKSPACE_SLOTS; i++) {
        WindowInfo *win = &app->windows[i];

        if (win->desktop == -1 && strcmp(win->type, "Normal") != 0) continue;
        if (win->desktop != current_desktop && win->desktop != -1) continue;
        if (win->id == app->own_window_id) continue;
        if (get_window_state(app->display, win->id, "_NET_WM_STATE_HIDDEN")) continue;
        if (get_window_state(app->display, win->id, "_NET_WM_STATE_SHADED")) continue;

        int x, y, w, h;
        if (!get_window_geometry(app->display, win->id, &x, &y, &w, &h)) continue;

        candidates[cand_count].id = win->id;
        candidates[cand_count].x = x;
        candidates[cand_count].y = y;
        candidates[cand_count].w = w;
        candidates[cand_count].h = h;
        cand_count++;
    }

    // Get stacking order and filter occluded windows
    unsigned long stack_count = 0;
    Window *stack = get_stacking_order(app->display, &stack_count);

    WindowPosition visible[MAX_WORKSPACE_SLOTS];
    int vis_count = 0;

    for (int i = 0; i < cand_count; i++) {
        int stack_pos = stack ? get_stack_position(candidates[i].id, stack, stack_count) : -1;
        if (stack && stack_pos >= 0 &&
            is_occluded(&candidates[i], stack_pos, candidates, cand_count, stack, stack_count)) {
            continue;
        }
        visible[vis_count++] = candidates[i];
    }

    if (stack) XFree(stack);

    // Sort visible windows by position: top-to-bottom, left-to-right
    qsort(visible, vis_count, sizeof(WindowPosition), compare_by_position);

    // Assign slots densely
    for (int i = 0; i < vis_count; i++) {
        manager->slots[i].id = visible[i].id;
        log_info("Workspace slot %d -> window 0x%lx (x=%d, y=%d)",
                 i + 1, visible[i].id, visible[i].x, visible[i].y);
    }
    manager->count = vis_count;

    // Auto-switch to per-workspace mode when slots are assigned
    if (app->config.digit_slot_mode != DIGIT_MODE_PER_WORKSPACE) {
        app->config.digit_slot_mode = DIGIT_MODE_PER_WORKSPACE;
        save_config(&app->config);
        log_info("Auto-switched digit_slot_mode to per-workspace");
    }

    log_info("Assigned %d workspace slots on desktop %d (%d candidates, %d visible)",
             vis_count, current_desktop, cand_count, vis_count);

    // Show overlay indicators
    show_slot_overlays(app);
}

Window get_workspace_slot_window(const WorkspaceSlotManager *manager, int slot) {
    if (slot < 1 || slot > MAX_WORKSPACE_SLOTS) {
        return 0;
    }

    int index = slot - 1;
    if (index >= manager->count) {
        return 0;
    }

    return manager->slots[index].id;
}
