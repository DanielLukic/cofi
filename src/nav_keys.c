#include "nav_keys.h"

NavDirection nav_direction_from_key(GdkEventKey *event) {
    if (!event) {
        return NAV_NONE;
    }

    switch (event->keyval) {
        case GDK_KEY_Up:
            return NAV_UP;
        case GDK_KEY_Down:
            return NAV_DOWN;
        case GDK_KEY_k:
            if (event->state & GDK_CONTROL_MASK) {
                return NAV_UP;
            }
            return NAV_NONE;
        case GDK_KEY_j:
            if (event->state & GDK_CONTROL_MASK) {
                return NAV_DOWN;
            }
            return NAV_NONE;
        default:
            return NAV_NONE;
    }
}
