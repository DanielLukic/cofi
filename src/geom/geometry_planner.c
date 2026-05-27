#include "geom/geometry_planner.h"

GeometryRestorePlan geometry_restore_plan(const GeometryState *current,
                                          const GeometryState *target,
                                          int current_active_desktop) {
    GeometryRestorePlan plan = {0};
    if (!current || !target) return plan;

    // Step 1: UNSET states that are currently SET but not wanted.
    // Must happen before move/resize — a maximized window ignores geometry.
    plan.unset_fullscreen = current->fullscreen  && !target->fullscreen;
    plan.unset_max_vert   = current->maximized_vert && !target->maximized_vert;
    plan.unset_max_horz   = current->maximized_horz && !target->maximized_horz;

    // Step 2: Move/resize only if geometry differs AND the window won't be
    // fully maximized in the target state (WM owns geometry under full-max).
    bool geom_differs = (target->x      != current->x      ||
                         target->y      != current->y      ||
                         target->width  != current->width  ||
                         target->height != current->height);
    bool target_fully_maximized = target->maximized_vert && target->maximized_horz;
    plan.do_move = geom_differs && !target->fullscreen && !target_fully_maximized;

    // Step 3: Desktop move only if target differs and is a valid desktop index.
    plan.do_desktop = (target->desktop >= 0) && (target->desktop != current->desktop);

    // Step 4: Switch the active (viewed) desktop to follow the restored window.
    plan.do_switch_active_desktop = (target->desktop >= 0) &&
                                    (target->desktop != current_active_desktop);

    // Step 5: SET states that are wanted but not currently set.
    plan.set_fullscreen = !current->fullscreen  && target->fullscreen;
    plan.set_max_vert   = !current->maximized_vert && target->maximized_vert;
    plan.set_max_horz   = !current->maximized_horz && target->maximized_horz;

    plan.any = plan.unset_fullscreen || plan.unset_max_vert   || plan.unset_max_horz ||
               plan.do_move          || plan.do_desktop        || plan.do_switch_active_desktop ||
               plan.set_fullscreen   || plan.set_max_vert      || plan.set_max_horz;

    return plan;
}
