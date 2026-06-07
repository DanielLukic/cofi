#include "geom/window_geometry_matching.h"

#include "geom/geometry_planner.h"
#include "geom/geom_rule_sync.h"
#include "geom/layout_store.h"
#include "core/log/log.h"
#include "matching/match_entry_config.h"
#include "x11/monitor_move.h"
#include "x11/x11_utils.h"

#include <string.h>

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
    out->restore_desktop = record->restore_desktop;
    out->disabled = record->disabled;
    return TRUE;
}

gboolean apply_window_geometry_restore(Display *display,
                                       const WindowGeometryRestoreTarget *target) {
    if (!display || !target || target->window == 0) return FALSE;
    if (target->width <= 0 || target->height <= 0) return FALSE;
    if (target->disabled) return TRUE;

    // Read current window state so we emit only the delta.
    GeometryState current = {0};
    current.fullscreen     = window_is_fullscreen(display, target->window);
    current.maximized_vert = window_is_maximized_vertical(display, target->window);
    current.maximized_horz = window_is_maximized_horizontal(display, target->window);
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
    if (!target->restore_desktop) {
        wanted.desktop = current.desktop;
    }

    int active_desktop = get_current_desktop(display);
    GeometryRestorePlan plan = geometry_restore_plan(&current, &wanted, active_desktop);

    if (plan.unset_fullscreen)
        set_window_fullscreen(display, target->window, WINDOW_STATE_UNSET);
    if (plan.unset_max_vert || plan.unset_max_horz)
        unmaximize_and_settle(display, target->window);
    if (plan.do_move)
        xmove_resize_frame_aware(display, target->window,
                                  target->x, target->y, target->width, target->height);
    if (plan.do_desktop)
        move_window_to_desktop(display, target->window, target->desktop);
    if (plan.do_switch_active_desktop)
        switch_to_desktop(display, target->desktop);
    if (plan.set_fullscreen)
        set_window_fullscreen(display, target->window, WINDOW_STATE_SET);
    if (plan.set_max_vert)
        set_window_maximized_vertical(display, target->window, WINDOW_STATE_SET);
    if (plan.set_max_horz)
        set_window_maximized_horizontal(display, target->window, WINDOW_STATE_SET);

    if (plan.any)
        XFlush(display);

    return TRUE;
}

static void fill_restore_target_from_record(const LayoutRecord *record,
                                            Window window,
                                            WindowGeometryRestoreTarget *out) {
    memset(out, 0, sizeof(*out));
    out->window = window;
    out->x = record->x;
    out->y = record->y;
    out->width = record->width;
    out->height = record->height;
    out->desktop = record->desktop;
    out->maximized_vert = record->maximized_vert;
    out->maximized_horz = record->maximized_horz;
    out->fullscreen = record->fullscreen;
    out->restore_desktop = record->restore_desktop;
    out->disabled = record->disabled;
}

static gboolean resolve_current_title_layout_for_window(AppData *app,
                                                        const WindowInfo *window,
                                                        int *match_id_out,
                                                        int *entry_idx_out,
                                                        WindowGeometryRestoreTarget *target_out) {
    if (!app || !window || !match_id_out || !entry_idx_out || !target_out) {
        return FALSE;
    }

    for (int i = 0; i < app->layouts.count; i++) {
        const LayoutRecord *record = &app->layouts.records[i];
        if (record->match_id <= 0 || record->disabled) continue;

        int idx = match_entry_find_index_by_match_id(&app->matching, record->match_id);
        if (idx < 0) continue;

        MatchEntry *entry = &app->matching.entries[idx];
        if (!match_entry_matches_window(entry, window)) continue;

        *match_id_out = record->match_id;
        *entry_idx_out = idx;
        fill_restore_target_from_record(record, window->id, target_out);
        return TRUE;
    }

    return FALSE;
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
    gboolean maximized_vert = window_is_maximized_vertical(app->display, window->id);
    gboolean maximized_horz = window_is_maximized_horizontal(app->display, window->id);
    gboolean fullscreen = window_is_fullscreen(app->display, window->id);
    int match_id = 0;
    for (int i = 0; i < app->layouts.count; i++) {
        int idx = match_entry_find_index_by_match_id(&app->matching, app->layouts.records[i].match_id);
        if (idx < 0) continue;
        if (!match_entry_matches_window(&app->matching.entries[idx], window)) continue;
        match_id = app->layouts.records[i].match_id;
        app->matching.entries[idx].bound_x11_id = window->id;
        app->matching.entries[idx].assigned = 1;
        break;
    }

    if (match_id <= 0) {
        for (int i = 0; i < app->matching.count; i++) {
            if (!match_entry_matches_window(&app->matching.entries[i], window)) continue;
            match_id = app->matching.entries[i].match_id;
            app->matching.entries[i].bound_x11_id = window->id;
            app->matching.entries[i].assigned = 1;
            break;
        }
    }

    if (match_id <= 0) {
        match_id = matching_create_entry(&app->matching, window);
    }
    if (match_id <= 0) {
        log_warn("Failed to capture matching entry for geometry save on window 0x%lx",
                 window->id);
        return FALSE;
    }

    if (!layout_store_set(&app->layouts, match_id, x, y, width, height, desktop,
                          maximized_vert, maximized_horz, fullscreen, TRUE, FALSE)) {
        log_warn("Failed to store layout for window 0x%lx (match_id=%d)",
                 window->id, match_id);
        return FALSE;
    }

    save_match_entries(&app->matching);
    if (!layout_store_save(&app->layouts)) {
        log_warn("Failed to persist layout for window 0x%lx (match_id=%d); skipping rule sync",
                 window->id, match_id);
        return FALSE;
    }
    geom_rule_sync_for_layout(app, match_id);

    log_info("Saved layout for window 0x%lx (match_id=%d): %d,%d %dx%d desktop=%d state[v=%d h=%d fs=%d lock=%d disabled=%d]",
             window->id, match_id, x, y, width, height, desktop,
             maximized_vert, maximized_horz, fullscreen, 1, 0);
    return TRUE;
}

gboolean restore_window_geometry_for_window(AppData *app, const WindowInfo *window) {
    if (!app || !window) return FALSE;
    int match_id = 0;
    int layout_entry_idx = -1;
    WindowGeometryRestoreTarget target = {0};
    if (resolve_current_title_layout_for_window(app, window, &match_id,
                                                &layout_entry_idx, &target)) {
        if (!apply_window_geometry_restore(app->display, &target)) {
            log_warn("Failed to apply saved layout for window 0x%lx (match_id=%d)",
                     window->id, match_id);
            return FALSE;
        }

        app->matching.entries[layout_entry_idx].bound_x11_id = window->id;
        app->matching.entries[layout_entry_idx].assigned = 1;
        save_match_entries(&app->matching);

        log_info("Restored layout for window 0x%lx (match_id=%d): %d,%d %dx%d desktop=%d state[v=%d h=%d fs=%d lock=%d disabled=%d]",
                 target.window, match_id, target.x, target.y,
                 target.width, target.height, target.desktop,
                 target.maximized_vert, target.maximized_horz, target.fullscreen,
                 target.restore_desktop, target.disabled);
        return TRUE;
    }

    if (!resolve_existing_match_id_for_window(app, window, &match_id)) {
        log_info("No saved layout binding for window 0x%lx", window->id);
        return TRUE;
    }

    memset(&target, 0, sizeof(target));
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

    log_info("Restored layout for window 0x%lx (match_id=%d): %d,%d %dx%d desktop=%d state[v=%d h=%d fs=%d lock=%d disabled=%d]",
             target.window, match_id, target.x, target.y,
             target.width, target.height, target.desktop,
             target.maximized_vert, target.maximized_horz, target.fullscreen,
             target.restore_desktop, target.disabled);
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

    if (!layout_store_save(&app->layouts)) {
        log_warn("geom: layout save failed after clear; skipping rule sync");
        return FALSE;
    }
    geom_rule_remove_for_match_id(app, match_id);
    match_entry_delete_by_match_id(&app->matching, match_id);
    save_match_entries(&app->matching);
    log_info("Cleared saved layout for window 0x%lx (match_id=%d)", window->id, match_id);
    return TRUE;
}
