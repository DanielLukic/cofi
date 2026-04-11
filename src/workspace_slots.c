#include "workspace_slots.h"
#include "app_data.h"
#include "config.h"
#include "x11_utils.h"
#include "monitor_move.h"
#include "slot_overlay.h"
#include "frame_extents.h"
#include "log.h"
#include <X11/Xatom.h>

// Some isolated test binaries compile workspace_slots.c without linking
// frame_extents.c; weak-link this symbol and fall back when unavailable.
extern int get_frame_extents(Display *display, Window window, FrameExtents *extents)
    __attribute__((weak));
#include <stdlib.h>
#include <string.h>

// Row grouping threshold: windows within this many pixels of the same Y
// are considered to be in the same row
#define ROW_THRESHOLD 50

// If a window has less than this fraction of its area visible, exclude it
// (configurable via slot_occlusion_threshold, default 5%)
#define DEFAULT_OCCLUSION_THRESHOLD_PCT 5

// The largest visible fragment must be at least this size in both dimensions
// to count as meaningfully visible (filters decoration leaks).
#define MIN_VISIBLE_DIM_PX 8

// Max occluding rects for visible-area subtraction
#define MAX_OCCLUDERS 32

typedef struct {
    Window id;
    int x;
    int y;
    int w;
    int h;
    int overlay_x;  // centroid of largest visible fragment (screen coords)
    int overlay_y;
    FrameExtents frame;  // decoration insets for post-subtraction filtering
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
// Also returns the centroid of the largest remaining visible rect fragment.
// This avoids double-counting overlapping occluders.
static double compute_visible_fraction_and_overlay_center(const WindowPosition *win,
                                                          int win_stack_pos,
                                                          const WindowPosition *all, int count,
                                                          Window *stack, unsigned long stack_count,
                                                          int *overlay_x, int *overlay_y,
                                                          int *largest_w, int *largest_h) {
    // Content rect: outer rect inset by frame extents.
    // Rectangle subtraction uses outer geometry (stacking order handles who
    // occludes whom). Frame extents are only applied when measuring the result:
    // visible fragments are clipped to the content rect so that decoration strips
    // (title bars, borders) that peek out from behind occluders don't count.
    int left = win->frame.left < 0 ? 0 : win->frame.left;
    int right = win->frame.right < 0 ? 0 : win->frame.right;
    int top = win->frame.top < 0 ? 0 : win->frame.top;
    int bottom = win->frame.bottom < 0 ? 0 : win->frame.bottom;

    if (left > win->w) left = win->w;
    if (right > win->w - left) right = win->w - left;
    if (top > win->h) top = win->h;
    if (bottom > win->h - top) bottom = win->h - top;

    Rect content_rect = {
        win->x + left,
        win->y + top,
        win->x + win->w - right,
        win->y + win->h - bottom
    };

    if (content_rect.x2 <= content_rect.x1 || content_rect.y2 <= content_rect.y1) {
        content_rect = (Rect){win->x, win->y, win->x + win->w, win->y + win->h};
    }

    double total_area = (double)(content_rect.x2 - content_rect.x1) *
                        (content_rect.y2 - content_rect.y1);

    if (overlay_x) *overlay_x = 0;
    if (overlay_y) *overlay_y = 0;
    if (largest_w) *largest_w = 0;
    if (largest_h) *largest_h = 0;
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

    if (occ_count == 0) {
        // Nothing occluding: full content area is visible.
        int cw = content_rect.x2 - content_rect.x1;
        int ch = content_rect.y2 - content_rect.y1;
        if (overlay_x) *overlay_x = content_rect.x1 + cw / 2;
        if (overlay_y) *overlay_y = content_rect.y1 + ch / 2;
        if (largest_w) *largest_w = cw;
        if (largest_h) *largest_h = ch;
        return 1.0;
    }

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

    // Sum remaining visible area (clipped to content) and track largest fragment.
    double visible_area = 0.0;
    int largest_area = 0;
    Rect largest = {0};
    for (int v = 0; v < vis_count; v++) {
        // Clip fragment to content rect
        Rect clipped = {
            visible[v].x1 > content_rect.x1 ? visible[v].x1 : content_rect.x1,
            visible[v].y1 > content_rect.y1 ? visible[v].y1 : content_rect.y1,
            visible[v].x2 < content_rect.x2 ? visible[v].x2 : content_rect.x2,
            visible[v].y2 < content_rect.y2 ? visible[v].y2 : content_rect.y2
        };
        int rw = clipped.x2 - clipped.x1;
        int rh = clipped.y2 - clipped.y1;
        if (rw <= 0 || rh <= 0) continue;  // fragment entirely in decoration zone
        int area = rw * rh;
        visible_area += (double)area;

        if (area > largest_area) {
            largest_area = area;
            largest = clipped;
        }
    }

    if (largest_area > 0) {
        if (overlay_x) *overlay_x = (largest.x1 + largest.x2) / 2;
        if (overlay_y) *overlay_y = (largest.y1 + largest.y2) / 2;
        if (largest_w) *largest_w = largest.x2 - largest.x1;
        if (largest_h) *largest_h = largest.y2 - largest.y1;
    }

    return visible_area / total_area;
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
    int occlusion_threshold_pct = app->config.slot_occlusion_threshold_pct;
    double occlusion_threshold = occlusion_threshold_pct / 100.0;

    // Defensive fallback: some test harnesses and early call paths may not run
    // init_config_defaults(), leaving this at 0. Use product default in that case.
    if (occlusion_threshold_pct <= 0 || occlusion_threshold_pct > 100) {
        occlusion_threshold_pct = DEFAULT_OCCLUSION_THRESHOLD_PCT;
    }
    occlusion_threshold = occlusion_threshold_pct / 100.0;

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
        candidates[cand_count].overlay_x = x + w / 2;
        candidates[cand_count].overlay_y = y + h / 2;
        // Store frame extents for post-subtraction decoration filtering
        memset(&candidates[cand_count].frame, 0, sizeof(FrameExtents));
        if (get_frame_extents && get_frame_extents(app->display, win->id, &candidates[cand_count].frame)) {
            // frame stored for later use
        }
        cand_count++;
    }

    // Get stacking order and filter occluded windows
    unsigned long stack_count = 0;
    Window *stack = get_stacking_order(app->display, &stack_count);

    WindowPosition visible[MAX_WINDOWS];
    int vis_count = 0;

    for (int i = 0; i < cand_count; i++) {
        int stack_pos = stack ? get_stack_position(candidates[i].id, stack, stack_count) : -1;
        if (stack && stack_pos >= 0) {
            int overlay_x = 0;
            int overlay_y = 0;
            int largest_w = 0;
            int largest_h = 0;
            double visible_fraction = compute_visible_fraction_and_overlay_center(
                &candidates[i], stack_pos, candidates, cand_count, stack, stack_count,
                &overlay_x, &overlay_y, &largest_w, &largest_h);

            if (visible_fraction < occlusion_threshold) {
                log_debug("Window 0x%lx excluded: %.1f%% visible (threshold %d%%)",
                          candidates[i].id, visible_fraction * 100, occlusion_threshold_pct);
                continue;
            }
            if (largest_w < MIN_VISIBLE_DIM_PX || largest_h < MIN_VISIBLE_DIM_PX) {
                log_debug("Window 0x%lx excluded: largest fragment %dx%d below min %dpx",
                          candidates[i].id, largest_w, largest_h, MIN_VISIBLE_DIM_PX);
                continue;
            }

            // Use centroid of largest visible fragment for overlay placement.
            candidates[i].overlay_x = overlay_x;
            candidates[i].overlay_y = overlay_y;
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
        manager->slots[i].overlay_x = visible[i].overlay_x;
        manager->slots[i].overlay_y = visible[i].overlay_y;
        manager->slots[i].has_overlay_pos = 1;
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
