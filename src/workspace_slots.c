#include "workspace_slots.h"
#include "app_data.h"
#include "config.h"
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

// If a window has less than this fraction of its area visible, exclude it
// (configurable via slot_occlusion_threshold, default 0.02 = 2%)
#define DEFAULT_OCCLUSION_THRESHOLD 0.02

// Max occluding rects for visible-area subtraction
#define MAX_OCCLUDERS 32

typedef struct {
    Window id;
    int x;
    int y;
    int w;
    int h;
} WindowPosition;

// A rect for visible-area computation (axis-aligned bounding box)
typedef struct {
    int x1, y1, x2, y2;  // top-left (x1,y1), bottom-right (x2,y2) exclusive
} Rect;

// Forward declaration (compute_visible_fraction uses this helper)
static int get_stack_position(Window id, Window *stack, unsigned long stack_count);

// Subtract rect B from rect A, producing up to 4 remaining rects.
// Returns the number of resulting rects written to `out`.
static int rect_subtract(Rect a, Rect b, Rect *out) {
    // No overlap
    if (b.x2 <= a.x1 || b.x1 >= a.x2 || b.y2 <= a.y1 || b.y1 >= a.y2) {
        out[0] = a;
        return 1;
    }

    int n = 0;

    // Left strip
    if (b.x1 > a.x1) {
        out[n++] = (Rect){a.x1, a.y1, b.x1, a.y2};
    }
    // Right strip
    if (b.x2 < a.x2) {
        out[n++] = (Rect){b.x2, a.y1, a.x2, a.y2};
    }
    // Top strip (clipped to B's x-range)
    if (b.y1 > a.y1) {
        out[n++] = (Rect){b.x1 > a.x1 ? b.x1 : a.x1, a.y1,
                          b.x2 < a.x2 ? b.x2 : a.x2, b.y1};
    }
    // Bottom strip (clipped to B's x-range)
    if (b.y2 < a.y2) {
        out[n++] = (Rect){b.x1 > a.x1 ? b.x1 : a.x1, b.y2,
                          b.x2 < a.x2 ? b.x2 : a.x2, a.y2};
    }

    return n;
}

// Compute the visible fraction of a window by subtracting all occluding rects
// (windows above it in the stacking order) using rectangle subtraction.
// This avoids double-counting overlapping occluders.
static double compute_visible_fraction(const WindowPosition *win, int win_stack_pos,
                                       const WindowPosition *all, int count,
                                       Window *stack, unsigned long stack_count) {
    double total_area = (double)win->w * win->h;
    if (total_area <= 0) return 0.0;

    // Collect occluding rects (windows above us in the stack)
    Rect occluders[MAX_OCCLUDERS];
    int occ_count = 0;

    for (int i = 0; i < count && occ_count < MAX_OCCLUDERS; i++) {
        if (all[i].id == win->id) continue;
        int other_pos = get_stack_position(all[i].id, stack, stack_count);
        if (other_pos <= win_stack_pos) continue;  // Below us in stack

        // Check if this window overlaps at all
        int ix1 = (win->x > all[i].x) ? win->x : all[i].x;
        int iy1 = (win->y > all[i].y) ? win->y : all[i].y;
        int ix2 = (win->x + win->w < all[i].x + all[i].w) ? win->x + win->w : all[i].x + all[i].w;
        int iy2 = (win->y + win->h < all[i].y + all[i].h) ? win->y + win->h : all[i].y + all[i].h;

        if (ix2 > ix1 && iy2 > iy1) {
            occluders[occ_count++] = (Rect){ix1, iy1, ix2, iy2};
        }
    }

    if (occ_count == 0) return 1.0;  // Nothing occluding

    // Start with the full window rect as the visible region
    Rect visible[MAX_OCCLUDERS * 4];  // Each subtraction can quad-section
    int vis_count = 1;
    visible[0] = (Rect){win->x, win->y, win->x + win->w, win->y + win->h};

    // Subtract each occluder from all current visible rects
    for (int o = 0; o < occ_count; o++) {
        Rect new_visible[MAX_OCCLUDERS * 4];
        int new_count = 0;

        for (int v = 0; v < vis_count; v++) {
            Rect split[4];
            int n = rect_subtract(visible[v], occluders[o], split);
            for (int s = 0; s < n && new_count < MAX_OCCLUDERS * 4; s++) {
                new_visible[new_count++] = split[s];
            }
            if (n > 0 && new_count >= MAX_OCCLUDERS * 4) {
                log_warn("Visible rect buffer full (%d rects), occlusion may be inaccurate",
                         MAX_OCCLUDERS * 4);
            }
        }

        vis_count = new_count;
        memcpy(visible, new_visible, vis_count * sizeof(Rect));

        // Early exit: if nothing visible left
        if (vis_count == 0) return 0.0;
    }

    // Sum remaining visible area
    double visible_area = 0.0;
    for (int v = 0; v < vis_count; v++) {
        visible_area += (double)(visible[v].x2 - visible[v].x1) * (visible[v].y2 - visible[v].y1);
    }

    return visible_area / total_area;
}

// Check if a window is occluded (visible area below threshold)
static int is_occluded(const WindowPosition *win, int win_stack_pos,
                       const WindowPosition *all, int count,
                       Window *stack, unsigned long stack_count,
                       double threshold) {
    double visible = compute_visible_fraction(win, win_stack_pos, all, count, stack, stack_count);
    if (visible < threshold) {
        log_debug("Window 0x%lx is %.1f%% visible (threshold %.1f%%), excluding",
                  win->id, visible * 100, threshold * 100);
        return 1;
    }
    return 0;
}

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

static int compare_by_x(const void *a, const void *b) {
    const WindowPosition *wa = (const WindowPosition *)a;
    const WindowPosition *wb = (const WindowPosition *)b;
    return wa->x - wb->x;
}

// Assign column indices by grouping windows whose left-edge X is within ROW_THRESHOLD
// of each other (gap-based, avoids bucket-boundary sensitivity).
// windows[] must be pre-sorted by X ascending.
static void assign_column_indices(WindowPosition *windows, int *col_indices, int count) {
    if (count == 0) return;
    int col = 0;
    int col_start_x = windows[0].x;
    col_indices[0] = 0;
    for (int i = 1; i < count; i++) {
        if (windows[i].x - col_start_x > ROW_THRESHOLD) {
            col++;
            col_start_x = windows[i].x;
        }
        col_indices[i] = col;
    }
}

typedef struct {
    WindowPosition pos;
    int col;
} WindowWithCol;

static int compare_by_col_then_y(const void *a, const void *b) {
    const WindowWithCol *wa = (const WindowWithCol *)a;
    const WindowWithCol *wb = (const WindowWithCol *)b;
    if (wa->col != wb->col) return wa->col - wb->col;
    return wa->pos.y - wb->pos.y;
}

static void sort_by_column(WindowPosition *windows, int count) {
    // Phase 1: sort by X to establish left-to-right order
    qsort(windows, count, sizeof(WindowPosition), compare_by_x);

    // Log positions after X-sort for diagnosis
    for (int i = 0; i < count; i++) {
        log_debug("  [col-sort] pre-group[%d]: id=0x%lx x=%d y=%d",
                  i, windows[i].id, windows[i].x, windows[i].y);
    }

    // Phase 2: assign column indices based on X gaps
    int col_indices[MAX_WINDOWS];
    assign_column_indices(windows, col_indices, count);

    for (int i = 0; i < count; i++) {
        log_debug("  [col-sort] col_assign[%d]: id=0x%lx x=%d -> col=%d",
                  i, windows[i].id, windows[i].x, col_indices[i]);
    }

    // Phase 3: sort by (col, y)
    WindowWithCol tmp[MAX_WINDOWS];
    for (int i = 0; i < count; i++) {
        tmp[i].pos = windows[i];
        tmp[i].col = col_indices[i];
    }
    qsort(tmp, count, sizeof(WindowWithCol), compare_by_col_then_y);
    for (int i = 0; i < count; i++) windows[i] = tmp[i].pos;
}

void init_workspace_slots(WorkspaceSlotManager *manager) {
    memset(manager, 0, sizeof(WorkspaceSlotManager));
    manager->workspace = -1;
}

void assign_workspace_slots(AppData *app) {
    WorkspaceSlotManager *manager = &app->workspace_slots;
    int current_desktop = get_current_desktop(app->display);
    double occlusion_threshold = app->config.slot_occlusion_threshold;

    // Defensive fallback: some test harnesses and early call paths may not run
    // init_config_defaults(), leaving this at 0. Use product default in that case.
    if (occlusion_threshold <= 0.0 || occlusion_threshold > 1.0) {
        occlusion_threshold = DEFAULT_OCCLUSION_THRESHOLD;
    }

    manager->count = 0;
    manager->workspace = current_desktop;

    // Collect all candidate windows with geometry
    WindowPosition candidates[MAX_WINDOWS];
    int cand_count = 0;

    for (int i = 0; i < app->window_count; i++) {
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

    WindowPosition visible[MAX_WINDOWS];
    int vis_count = 0;

    for (int i = 0; i < cand_count; i++) {
        int stack_pos = stack ? get_stack_position(candidates[i].id, stack, stack_count) : -1;
        if (stack && stack_pos >= 0 &&
            is_occluded(&candidates[i], stack_pos, candidates, cand_count, stack, stack_count,
                        occlusion_threshold)) {
            continue;
        }
        visible[vis_count++] = candidates[i];
    }

    if (stack) XFree(stack);

    // Sort visible windows by position
    log_debug("Sorting %d visible windows (mode=%s)", vis_count,
              app->config.slot_sort_order == SLOT_SORT_COLUMN_FIRST ? "column" : "row");
    if (app->config.slot_sort_order == SLOT_SORT_COLUMN_FIRST) {
        sort_by_column(visible, vis_count);
    } else {
        qsort(visible, vis_count, sizeof(WindowPosition), compare_by_position);
    }

    // Assign slots densely (capped to available workspace slots)
    int assigned_count = (vis_count < MAX_WORKSPACE_SLOTS) ? vis_count : MAX_WORKSPACE_SLOTS;
    for (int i = 0; i < assigned_count; i++) {
        manager->slots[i].id = visible[i].id;
        log_info("Workspace slot %d -> window 0x%lx (x=%d, y=%d)",
                 i + 1, visible[i].id, visible[i].x, visible[i].y);
    }
    manager->count = assigned_count;

    // Auto-switch to per-workspace mode when slots are assigned
    if (app->config.digit_slot_mode != DIGIT_MODE_PER_WORKSPACE) {
        app->config.digit_slot_mode = DIGIT_MODE_PER_WORKSPACE;
        save_config(&app->config);
        log_info("Auto-switched digit_slot_mode to per-workspace");
    }

    log_info("Assigned %d workspace slots on desktop %d (%d qualifying, %d visible after occlusion)",
             assigned_count, current_desktop, cand_count, vis_count);

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
