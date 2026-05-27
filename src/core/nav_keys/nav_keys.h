#ifndef NAV_KEYS_H
#define NAV_KEYS_H

#include <gdk/gdk.h>

typedef enum {
    NAV_NONE,
    NAV_UP,
    NAV_DOWN
} NavDirection;

NavDirection nav_direction_from_key(GdkEventKey *event);

#endif
