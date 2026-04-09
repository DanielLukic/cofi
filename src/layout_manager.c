#include "layout_manager.h"

#include <string.h>

#include "app_data.h"
#include "layout_patterns.h"
#include "log.h"
#include "monitor_move.h"
#include "types.h"
#include "x11_utils.h"

typedef struct {
    Window id;
    int x;
    int y;
    int width;
    int height;
} LayoutCandidate;

static int get_workspace_layout_index(AppData *app) {
    int desktop = get_current_desktop(app->display);
    if (desktop < 0 || desktop >= MAX_LAYOUT_WORKSPACES) {
        log_warn("Layout unavailable for desktop index %d (supported: 0-%d)",
                 desktop, MAX_LAYOUT_WORKSPACES - 1);
        return -1;
    }

    return desktop;
}

static gboolean id_in_list(const Window *windows, int count, Window id) {
    for (int i = 0; i < count; i++) {
        if (windows[i] == id) {
            return TRUE;
        }
    }

    return FALSE;
}

static gboolean is_normal_window_type(AppData *app, Window window_id) {
    char *window_type = get_window_type(app->display, window_id);
    gboolean is_normal = window_type && strcmp(window_type, WINDOW_TYPE_NORMAL) == 0;
    g_free(window_type);
    return is_normal;
}

static gboolean should_include_window(AppData *app, const WindowInfo *window,
                                      int current_desktop) {
    if (!window || window->id == app->own_window_id) {
        return FALSE;
    }

    if (window->desktop != current_desktop) {
        return FALSE;
    }

    if (strcmp(window->type, WINDOW_TYPE_NORMAL) != 0) {
        return FALSE;
    }

    if (!is_normal_window_type(app, window->id)) {
        return FALSE;
    }

    if (get_window_state(app->display, window->id, "_NET_WM_STATE_HIDDEN")) {
        return FALSE;
    }

    if (get_window_state(app->display, window->id, "_NET_WM_STATE_SHADED")) {
        return FALSE;
    }

    if (get_window_state(app->display, window->id, "_NET_WM_STATE_STICKY")) {
        return FALSE;
    }

    return TRUE;
}

static int collect_layout_candidates(AppData *app, int current_desktop,
                                     LayoutCandidate *candidates, int max_candidates) {
    int count = 0;

    for (int i = 0; i < app->window_count && count < max_candidates; i++) {
        WindowInfo *window = &app->windows[i];
        int x, y, width, height;

        if (!should_include_window(app, window, current_desktop)) {
            continue;
        }

        if (!get_window_geometry(app->display, window->id, &x, &y, &width, &height)) {
            continue;
        }

        if (width <= 0 || height <= 0) {
            continue;
        }

        candidates[count].id = window->id;
        candidates[count].x = x;
        candidates[count].y = y;
        candidates[count].width = width;
        candidates[count].height = height;
        count++;
    }

    return count;
}

static int append_mru_candidates(AppData *app, const LayoutCandidate *candidates, int candidate_count,
                                 Window *ordered, int max_ordered) {
    int ordered_count = 0;

    for (int i = 0; i < app->filtered_count && ordered_count < max_ordered; i++) {
        Window id = app->filtered[i].id;
        for (int j = 0; j < candidate_count; j++) {
            if (candidates[j].id == id && !id_in_list(ordered, ordered_count, id)) {
                ordered[ordered_count++] = id;
                break;
            }
        }
    }

    return ordered_count;
}

static int order_candidates(AppData *app, const LayoutCandidate *candidates, int candidate_count,
                            Window *ordered, int max_ordered) {
    int ordered_count = append_mru_candidates(app, candidates, candidate_count,
                                              ordered, max_ordered);

    for (int i = 0; i < candidate_count && ordered_count < max_ordered; i++) {
        if (!id_in_list(ordered, ordered_count, candidates[i].id)) {
            ordered[ordered_count++] = candidates[i].id;
        }
    }

    return ordered_count;
}

static const LayoutCandidate *find_candidate(const LayoutCandidate *candidates,
                                             int candidate_count, Window id) {
    for (int i = 0; i < candidate_count; i++) {
        if (candidates[i].id == id) {
            return &candidates[i];
        }
    }

    return NULL;
}

static gboolean intersects_work_area(const LayoutCandidate *candidate,
                                     const WorkArea *work_area) {
    int left = candidate->x;
    int top = candidate->y;
    int right = candidate->x + candidate->width;
    int bottom = candidate->y + candidate->height;

    int work_left = work_area->x;
    int work_top = work_area->y;
    int work_right = work_area->x + work_area->width;
    int work_bottom = work_area->y + work_area->height;

    return right > work_left && left < work_right &&
           bottom > work_top && top < work_bottom;
}

static int filter_to_monitor_windows(const LayoutCandidate *candidates, int candidate_count,
                                     const Window *ordered, int ordered_count,
                                     const WorkArea *work_area,
                                     Window *monitor_windows, int max_windows) {
    int count = 0;

    for (int i = 0; i < ordered_count && count < max_windows; i++) {
        const LayoutCandidate *candidate = find_candidate(candidates, candidate_count, ordered[i]);
        if (!candidate) {
            continue;
        }

        if (intersects_work_area(candidate, work_area)) {
            monitor_windows[count++] = ordered[i];
        }
    }

    return count;
}

static Window select_primary_from_mru(AppData *app, const Window *windows, int count) {
    for (int i = 0; i < app->filtered_count; i++) {
        Window id = app->filtered[i].id;
        if (id_in_list(windows, count, id)) {
            return id;
        }
    }

    return count > 0 ? windows[0] : 0;
}

static Window choose_primary_window(AppData *app, const WorkspaceLayoutState *state,
                                    const Window *windows, int count, gboolean use_focused) {
    if (count <= 0) {
        return 0;
    }

    if (!use_focused && state->primary_window_id != 0 &&
        id_in_list(windows, count, state->primary_window_id)) {
        return state->primary_window_id;
    }

    if (use_focused) {
        Window focused = (Window)get_active_window_id(app->display);
        if (focused != 0 && id_in_list(windows, count, focused)) {
            return focused;
        }
    }

    return select_primary_from_mru(app, windows, count);
}

static gboolean apply_main_stack_for_workspace(AppData *app, WorkspaceLayoutState *state,
                                               gboolean use_focused_primary) {
    int current_desktop = get_current_desktop(app->display);
    LayoutCandidate candidates[MAX_WINDOWS];
    Window ordered_windows[MAX_WINDOWS];
    Window monitor_windows[MAX_WINDOWS];

    int candidate_count = collect_layout_candidates(app, current_desktop,
                                                    candidates, MAX_WINDOWS);
    if (candidate_count <= 0) {
        log_warn("No eligible windows for layout on desktop %d", current_desktop);
        return FALSE;
    }

    int ordered_count = order_candidates(app, candidates, candidate_count,
                                         ordered_windows, MAX_WINDOWS);
    Window primary = choose_primary_window(app, state, ordered_windows, ordered_count,
                                           use_focused_primary);
    if (primary == 0) {
        log_warn("Could not resolve primary window for layout");
        return FALSE;
    }

    WorkArea monitor_area;
    get_target_work_area(app->display, primary, &monitor_area);

    int monitor_count = filter_to_monitor_windows(candidates, candidate_count,
                                                  ordered_windows, ordered_count,
                                                  &monitor_area,
                                                  monitor_windows, MAX_WINDOWS);
    if (monitor_count <= 0) {
        log_warn("No eligible windows on primary monitor for layout");
        return FALSE;
    }

    if (!id_in_list(monitor_windows, monitor_count, primary)) {
        primary = select_primary_from_mru(app, monitor_windows, monitor_count);
    }

    if (primary == 0) {
        log_warn("Could not resolve primary window after monitor filter");
        return FALSE;
    }

    if (!layout_main_stack(app->display, monitor_windows, monitor_count, primary, &monitor_area)) {
        return FALSE;
    }

    state->active = TRUE;
    state->pattern = LAYOUT_PATTERN_MAIN_STACK;
    state->primary_window_id = primary;

    log_info("Applied main+stack layout on desktop %d with primary 0x%lx (%d windows)",
             current_desktop, primary, monitor_count);
    return TRUE;
}

void init_layout_states(WorkspaceLayoutState *states, int count) {
    for (int i = 0; i < count; i++) {
        states[i].active = FALSE;
        states[i].pattern = LAYOUT_PATTERN_NONE;
        states[i].primary_window_id = 0;
    }
}

gboolean apply_layout(AppData *app, LayoutPattern pattern) {
    int index = get_workspace_layout_index(app);
    if (index < 0) {
        return FALSE;
    }

    if (pattern != LAYOUT_PATTERN_MAIN_STACK) {
        log_warn("Unsupported layout pattern");
        return FALSE;
    }

    WorkspaceLayoutState *state = &app->layout_states[index];
    return apply_main_stack_for_workspace(app, state, TRUE);
}

gboolean refresh_layout(AppData *app) {
    int index = get_workspace_layout_index(app);
    if (index < 0) {
        return FALSE;
    }

    WorkspaceLayoutState *state = &app->layout_states[index];
    if (!state->active || state->pattern == LAYOUT_PATTERN_NONE) {
        log_warn("No active layout on current workspace to refresh");
        return FALSE;
    }

    if (state->pattern == LAYOUT_PATTERN_MAIN_STACK) {
        return apply_main_stack_for_workspace(app, state, FALSE);
    }

    log_warn("Unsupported layout pattern for refresh");
    return FALSE;
}

void layout_off(AppData *app) {
    int index = get_workspace_layout_index(app);
    if (index < 0) {
        return;
    }

    app->layout_states[index].active = FALSE;
    app->layout_states[index].pattern = LAYOUT_PATTERN_NONE;
    app->layout_states[index].primary_window_id = 0;

    log_info("Disabled layout on desktop %d", index);
}
