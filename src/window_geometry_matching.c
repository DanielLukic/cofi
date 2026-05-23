#include "window_geometry_matching.h"

#include "frame_extents.h"
#include "geometry_planner.h"
#include "layout_store.h"
#include "log.h"
#include "matching_gc.h"
#include "match_entry_config.h"
#include "monitor_move.h"
#include "x11_utils.h"

static void pending_restore_add(AppData *app, Window w);

gboolean resolve_window_geometry_restore_target(const MatchEntryManager *manager,
                                                const LayoutStore *store,
                                                int match_id,
                                                WindowGeometryRestoreTarget *out) {
    if (!manager || !store || !out || match_id <= 0) return FALSE;

    int idx = match_entry_find_index_by_match_id(manager, match_id);
    if (idx < 0) return FALSE;

    const MatchEntry *entry = &manager->entries[idx];
    const LayoutRecord *record = layout_store_get(store, match_id);
    if (!record || !entry->assigned || entry->bound_x11_id == 0) {
        return FALSE;
    }

    out->window = entry->bound_x11_id;
    out->x = record->x;
    out->y = record->y;
    out->width = record->width;
    out->height = record->height;
    out->desktop = record->desktop;
    out->maximized_vert = record->maximized_vert;
    out->maximized_horz = record->maximized_horz;
    out->fullscreen = record->fullscreen;
    return TRUE;
}

gboolean apply_window_geometry_restore(Display *display,
                                       const WindowGeometryRestoreTarget *target,
                                       gboolean *move_deferred_out) {
    if (!display || !target || target->window == 0) return FALSE;
    if (target->width <= 0 || target->height <= 0) return FALSE;
    if (move_deferred_out) *move_deferred_out = FALSE;

    // Read current window state so we emit only the delta.
    GeometryState current = {0};
    current.fullscreen     = get_window_state(display, target->window,
                                              "_NET_WM_STATE_FULLSCREEN");
    current.maximized_vert = get_window_state(display, target->window,
                                              "_NET_WM_STATE_MAXIMIZED_VERT");
    current.maximized_horz = get_window_state(display, target->window,
                                              "_NET_WM_STATE_MAXIMIZED_HORZ");
    current.desktop        = get_window_desktop(display, target->window);
    if (!get_window_geometry(display, target->window,
                             &current.x, &current.y,
                             &current.width, &current.height)) {
        // Unreadable geometry: assume differs so all geometry ops run.
        current.x = current.y = current.width = current.height = 0;
    }

    GeometryState wanted = {
        .x             = target->x,
        .y             = target->y,
        .width         = target->width,
        .height        = target->height,
        .desktop       = target->desktop,
        .maximized_vert = (bool)target->maximized_vert,
        .maximized_horz = (bool)target->maximized_horz,
        .fullscreen    = (bool)target->fullscreen,
    };

    int active_desktop = get_current_desktop(display);
    GeometryRestorePlan plan = geometry_restore_plan(&current, &wanted, active_desktop);

    if (plan.unset_fullscreen)
        set_window_state(display, target->window, "_NET_WM_STATE_FULLSCREEN",
                         WINDOW_STATE_UNSET);
    if (plan.unset_max_vert)
        set_window_state(display, target->window, "_NET_WM_STATE_MAXIMIZED_VERT",
                         WINDOW_STATE_UNSET);
    if (plan.unset_max_horz)
        set_window_state(display, target->window, "_NET_WM_STATE_MAXIMIZED_HORZ",
                         WINDOW_STATE_UNSET);
    if (plan.do_move) {
        // get_window_geometry returns the frame's root position; XMoveResizeWindow
        // expects the client's root position. Add frame extents so the frame lands
        // back at the stored position — otherwise each restore shifts by (-left,-top).
        int move_x = target->x, move_y = target->y;
        FrameExtents fe = {0};
        int fe_ok = get_frame_extents(display, target->window, &fe);

        if (!fe_ok || !frame_extents_valid(&fe)) {
            // WM has not yet populated _NET_FRAME_EXTENTS (common on first restore
            // for a freshly opened window). Skip move now; caller will enqueue for
            // retry when the property arrives.
            if (move_deferred_out) *move_deferred_out = TRUE;
            log_info("GEOMDBG: 0x%lx extents not ready (fe_ok=%d fe={l=%d t=%d r=%d b=%d})"
                     " saved=(%d,%d) — deferring move",
                     target->window, fe_ok, fe.left, fe.top, fe.right, fe.bottom,
                     target->x, target->y);
        } else {
            frame_pos_to_client_pos(target->x, target->y, &fe, &move_x, &move_y);
            log_info("GEOMDBG: 0x%lx fe={l=%d t=%d r=%d b=%d} saved=(%d,%d) move=(%d,%d)",
                     target->window, fe.left, fe.top, fe.right, fe.bottom,
                     target->x, target->y, move_x, move_y);
            XMoveResizeWindow(display, target->window, move_x, move_y,
                              (unsigned int)target->width, (unsigned int)target->height);
        }
    }
    if (plan.do_desktop)
        move_window_to_desktop(display, target->window, target->desktop);
    if (plan.do_switch_active_desktop)
        switch_to_desktop(display, target->desktop);
    if (plan.set_fullscreen)
        set_window_state(display, target->window, "_NET_WM_STATE_FULLSCREEN",
                         WINDOW_STATE_SET);
    if (plan.set_max_vert)
        set_window_state(display, target->window, "_NET_WM_STATE_MAXIMIZED_VERT",
                         WINDOW_STATE_SET);
    if (plan.set_max_horz)
        set_window_state(display, target->window, "_NET_WM_STATE_MAXIMIZED_HORZ",
                         WINDOW_STATE_SET);

    if (plan.any)
        XFlush(display);

    return TRUE;
}

static gboolean resolve_existing_match_id_for_window(AppData *app,
                                                     const WindowInfo *window,
                                                     int *match_id_out) {
    if (!app || !window || !match_id_out) {
        return FALSE;
    }

    int idx = match_entry_find_index_by_window(&app->matching, window->id);
    if (idx >= 0) {
        *match_id_out = app->matching.entries[idx].match_id;
        return *match_id_out > 0;
    }

    if (match_entry_reassign_live_windows(&app->matching, app->windows, app->window_count)) {
        save_match_entries(&app->matching);
    }

    idx = match_entry_find_index_by_window(&app->matching, window->id);
    if (idx < 0) {
        return FALSE;
    }

    *match_id_out = app->matching.entries[idx].match_id;
    return *match_id_out > 0;
}

gboolean save_window_geometry_for_window(AppData *app, const WindowInfo *window) {
    if (!app || !window) return FALSE;
    int x = 0, y = 0, width = 0, height = 0;
    if (!get_window_geometry(app->display, window->id, &x, &y, &width, &height)) {
        log_warn("Failed to capture geometry for window 0x%lx", window->id);
        return FALSE;
    }

    int desktop = get_window_desktop(app->display, window->id);
    gboolean maximized_vert = get_window_state(app->display, window->id,
                                               "_NET_WM_STATE_MAXIMIZED_VERT");
    gboolean maximized_horz = get_window_state(app->display, window->id,
                                               "_NET_WM_STATE_MAXIMIZED_HORZ");
    gboolean fullscreen = get_window_state(app->display, window->id,
                                           "_NET_WM_STATE_FULLSCREEN");
    int match_id = matching_capture_or_get(&app->matching, app->windows,
                                           app->window_count, window);
    if (match_id <= 0) {
        log_warn("Failed to capture matching entry for geometry save on window 0x%lx",
                 window->id);
        return FALSE;
    }

    if (!layout_store_set(&app->layouts, match_id, x, y, width, height, desktop,
                          maximized_vert, maximized_horz, fullscreen)) {
        log_warn("Failed to store layout for window 0x%lx (match_id=%d)",
                 window->id, match_id);
        return FALSE;
    }

    save_match_entries(&app->matching);
    layout_store_save(&app->layouts);

    log_info("Saved layout for window 0x%lx (match_id=%d): %d,%d %dx%d desktop=%d state[v=%d h=%d fs=%d]",
             window->id, match_id, x, y, width, height, desktop,
             maximized_vert, maximized_horz, fullscreen);
    return TRUE;
}

gboolean restore_window_geometry_for_window(AppData *app, const WindowInfo *window) {
    if (!app || !window) return FALSE;
    int match_id = 0;
    if (!resolve_existing_match_id_for_window(app, window, &match_id)) {
        log_info("No saved layout binding for window 0x%lx", window->id);
        return TRUE;
    }

    WindowGeometryRestoreTarget target = {0};
    if (!resolve_window_geometry_restore_target(&app->matching, &app->layouts,
                                                match_id, &target)) {
        log_info("No saved layout for window 0x%lx (match_id=%d)",
                 window->id, match_id);
        return TRUE;
    }

    gboolean move_deferred = FALSE;
    if (!apply_window_geometry_restore(app->display, &target, &move_deferred)) {
        log_warn("Failed to apply saved layout for window 0x%lx (match_id=%d)",
                 window->id, match_id);
        return FALSE;
    }

    if (move_deferred) {
        // _NET_FRAME_EXTENTS not ready. Enqueue the window; PropertyNotify will
        // trigger a retry via process_pending_geometry_restore.
        pending_restore_add(app, window->id);
        request_frame_extents(app->display, window->id);
        log_info("Deferred move for window 0x%lx (match_id=%d) — awaiting _NET_FRAME_EXTENTS",
                 window->id, match_id);
        return TRUE;
    }

    log_info("Restored layout for window 0x%lx (match_id=%d): %d,%d %dx%d desktop=%d state[v=%d h=%d fs=%d]",
             target.window, match_id, target.x, target.y,
             target.width, target.height, target.desktop,
             target.maximized_vert, target.maximized_horz, target.fullscreen);
    return TRUE;
}

gboolean clear_window_geometry_for_window(AppData *app, const WindowInfo *window) {
    if (!app || !window) return FALSE;

    int match_id = 0;
    if (!resolve_existing_match_id_for_window(app, window, &match_id)) {
        log_info("No saved layout binding to clear for window 0x%lx", window->id);
        return TRUE;
    }

    if (!layout_store_clear(&app->layouts, match_id)) {
        log_info("No saved layout to clear for window 0x%lx (match_id=%d)",
                 window->id, match_id);
        return TRUE;
    }

    layout_store_save(&app->layouts);
    matching_run_gc(app);
    log_info("Cleared saved layout for window 0x%lx (match_id=%d)", window->id, match_id);
    return TRUE;
}

static void pending_restore_add(AppData *app, Window w) {
    for (int i = 0; i < app->pending_restore_count; i++) {
        if (app->pending_restores[i] == w) return; // already queued
    }
    if (app->pending_restore_count >= PENDING_RESTORE_MAX) {
        log_warn("Pending restore queue full, dropping 0x%lx", w);
        return;
    }
    app->pending_restores[app->pending_restore_count++] = w;
}

void process_pending_geometry_restore(AppData *app, Window window_id) {
    int found = 0;
    for (int i = 0; i < app->pending_restore_count; i++) {
        if (app->pending_restores[i] == window_id) {
            app->pending_restores[i] = app->pending_restores[--app->pending_restore_count];
            found = 1;
            break;
        }
    }
    if (!found) return;

    FrameExtents fe = {0};
    int fe_ok = get_frame_extents(app->display, window_id, &fe);
    if (!fe_ok || !frame_extents_valid(&fe)) {
        log_warn("GEOMDBG: 0x%lx: _NET_FRAME_EXTENTS fired but still zero — giving up",
                 window_id);
        return;
    }

    for (int i = 0; i < app->window_count; i++) {
        if (app->windows[i].id == window_id) {
            log_info("GEOMDBG: 0x%lx: retrying geometry restore after extents populated",
                     window_id);
            restore_window_geometry_for_window(app, &app->windows[i]);
            return;
        }
    }
    log_debug("Window 0x%lx no longer in list, dropping pending restore", window_id);
}
