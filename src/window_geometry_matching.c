#include "window_geometry_matching.h"

#include "layout_store.h"
#include "log.h"
#include "matching_gc.h"
#include "match_entry_config.h"
#include "monitor_move.h"
#include "x11_utils.h"

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
    return TRUE;
}

gboolean apply_window_geometry_restore(Display *display,
                                       const WindowGeometryRestoreTarget *target) {
    if (!display || !target || target->window == 0) return FALSE;
    if (target->width <= 0 || target->height <= 0) return FALSE;

    XMoveResizeWindow(display, target->window, target->x, target->y,
                      (unsigned int)target->width, (unsigned int)target->height);

    // A per-restore workspace toggle is deferred; restoring desktop is part of
    // the current explicit geometry restore prototype.
    move_window_to_desktop(display, target->window, target->desktop);
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
    int match_id = matching_capture_or_get(&app->matching, app->windows,
                                           app->window_count, window);
    if (match_id <= 0) {
        log_warn("Failed to capture matching entry for geometry save on window 0x%lx",
                 window->id);
        return FALSE;
    }

    if (!layout_store_set(&app->layouts, match_id, x, y, width, height, desktop)) {
        log_warn("Failed to store layout for window 0x%lx (match_id=%d)",
                 window->id, match_id);
        return FALSE;
    }

    save_match_entries(&app->matching);
    layout_store_save(&app->layouts);

    log_info("Saved layout for window 0x%lx (match_id=%d): %d,%d %dx%d desktop=%d",
             window->id, match_id, x, y, width, height, desktop);
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

    if (!apply_window_geometry_restore(app->display, &target)) {
        log_warn("Failed to apply saved layout for window 0x%lx (match_id=%d)",
                 window->id, match_id);
        return FALSE;
    }

    log_info("Restored layout for window 0x%lx (match_id=%d): %d,%d %dx%d desktop=%d",
             target.window, match_id, target.x, target.y,
             target.width, target.height, target.desktop);
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
