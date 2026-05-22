#include "window_appearance.h"

static int contains_window_id(const Window *ids, int count, Window id) {
    if (!ids || count <= 0 || id == 0) {
        return 0;
    }

    for (int i = 0; i < count; i++) {
        if (ids[i] == id) {
            return 1;
        }
    }

    return 0;
}

int collect_new_window_ids(const Window *old_ids, int old_count,
                           const WindowInfo *windows, int window_count,
                           Window *out_new_ids, int max_new_ids) {
    if (!windows || window_count <= 0 || !out_new_ids || max_new_ids <= 0) {
        return 0;
    }

    int new_count = 0;
    for (int i = 0; i < window_count && new_count < max_new_ids; i++) {
        Window id = windows[i].id;
        if (id == 0 || contains_window_id(old_ids, old_count, id)) {
            continue;
        }
        out_new_ids[new_count++] = id;
    }

    return new_count;
}
