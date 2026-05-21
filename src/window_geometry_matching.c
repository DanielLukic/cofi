#include "window_geometry_matching.h"

#include <X11/Xatom.h>
#include <string.h>

#include "log.h"
#include "match_entry_config.h"
#include "monitor_move.h"
#include "selection.h"
#include "x11_utils.h"

static gboolean is_geometry_save_shortcut(const GdkEventKey *event) {
    return event &&
           (event->state & GDK_CONTROL_MASK) &&
           !(event->state & (GDK_SHIFT_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK)) &&
           event->keyval == GDK_KEY_semicolon;
}

static gboolean is_geometry_restore_shortcut(const GdkEventKey *event) {
    return event &&
           (event->state & GDK_CONTROL_MASK) &&
           !(event->state & (GDK_SHIFT_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK)) &&
           event->keyval == GDK_KEY_apostrophe;
}

gboolean resolve_window_geometry_restore_target(const MatchEntryManager *manager,
                                                int match_id,
                                                WindowGeometryRestoreTarget *out) {
    if (!manager || !out || match_id <= 0) return FALSE;

    int idx = match_entry_find_index_by_match_id(manager, match_id);
    if (idx < 0) return FALSE;

    const MatchEntry *entry = &manager->entries[idx];
    if (!entry->has_geom || !entry->assigned || entry->bound_x11_id == 0) {
        return FALSE;
    }

    out->window = entry->bound_x11_id;
    out->x = entry->geom_x;
    out->y = entry->geom_y;
    out->width = entry->geom_w;
    out->height = entry->geom_h;
    out->desktop = entry->geom_desktop;
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

gboolean handle_window_geometry_save(GdkEventKey *event, AppData *app) {
    if (!is_geometry_save_shortcut(event)) {
        return FALSE;
    }

    if (!app || app->current_tab != TAB_WINDOWS) {
        return FALSE;
    }

    WindowInfo *selected_window = get_selected_window(app);
    if (!selected_window) {
        return FALSE;
    }

    int x = 0, y = 0, width = 0, height = 0;
    if (!get_window_geometry(app->display, selected_window->id, &x, &y, &width, &height)) {
        log_warn("Failed to capture geometry for window 0x%lx", selected_window->id);
        return TRUE;
    }

    int desktop = get_window_desktop(app->display, selected_window->id);
    int match_id = matching_capture_or_get(&app->matching, app->windows,
                                           app->window_count, selected_window);
    if (match_id <= 0) {
        log_warn("Failed to capture matching entry for geometry save on window 0x%lx",
                 selected_window->id);
        return TRUE;
    }

    int idx = match_entry_find_index_by_match_id(&app->matching, match_id);
    if (idx < 0) {
        log_warn("Captured match_id %d but could not resolve geometry entry", match_id);
        return TRUE;
    }

    MatchEntry *entry = &app->matching.entries[idx];
    entry->geom_x = x;
    entry->geom_y = y;
    entry->geom_w = width;
    entry->geom_h = height;
    entry->geom_desktop = desktop;
    entry->has_geom = 1;
    save_match_entries(&app->matching);

    log_info("Saved geometry for window 0x%lx (match_id=%d): %d,%d %dx%d desktop=%d",
             selected_window->id, match_id, x, y, width, height, desktop);
    return TRUE;
}

gboolean handle_window_geometry_restore(GdkEventKey *event, AppData *app) {
    if (!is_geometry_restore_shortcut(event)) {
        return FALSE;
    }

    if (!app || app->current_tab != TAB_WINDOWS) {
        return FALSE;
    }

    WindowInfo *selected_window = get_selected_window(app);
    if (!selected_window) {
        return FALSE;
    }

    int match_id = matching_capture_or_get(&app->matching, app->windows,
                                           app->window_count, selected_window);
    if (match_id <= 0) {
        log_warn("Failed to capture matching entry for geometry restore on window 0x%lx",
                 selected_window->id);
        return TRUE;
    }

    WindowGeometryRestoreTarget target = {0};
    if (!resolve_window_geometry_restore_target(&app->matching, match_id, &target)) {
        log_info("No saved geometry for window 0x%lx (match_id=%d)",
                 selected_window->id, match_id);
        return TRUE;
    }

    if (!apply_window_geometry_restore(app->display, &target)) {
        log_warn("Failed to apply saved geometry for window 0x%lx (match_id=%d)",
                 selected_window->id, match_id);
        return TRUE;
    }

    log_info("Restored geometry for window 0x%lx (match_id=%d): %d,%d %dx%d desktop=%d",
             target.window, match_id, target.x, target.y,
             target.width, target.height, target.desktop);
    return TRUE;
}
