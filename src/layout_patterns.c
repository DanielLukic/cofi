#include "layout_patterns.h"

#include <string.h>

#include "size_hints.h"
#include "types.h"

static int find_window_index(const Window *windows, int count, Window window_id) {
    for (int i = 0; i < count; i++) {
        if (windows[i] == window_id) {
            return i;
        }
    }

    return -1;
}

static int copy_primary_target(const Window *windows, int count, int primary_index,
                               const WorkArea *monitor_rect,
                               LayoutTarget *targets, int max_targets) {
    if (!monitor_rect || !targets || max_targets <= 0 || count <= 0) {
        return 0;
    }

    int main_width = monitor_rect->width / 2;
    targets[0].window_id = windows[primary_index];
    targets[0].geometry.x = monitor_rect->x;
    targets[0].geometry.y = monitor_rect->y;
    targets[0].geometry.width = main_width;
    targets[0].geometry.height = monitor_rect->height;
    return 1;
}

static int append_stack_targets(const Window *windows, int count, int primary_index,
                                const WorkArea *monitor_rect,
                                LayoutTarget *targets, int start_index,
                                int max_targets) {
    int stack_count = count - 1;
    if (stack_count <= 0) {
        return start_index;
    }

    int stack_x = monitor_rect->x + (monitor_rect->width / 2);
    int stack_width = monitor_rect->width - (monitor_rect->width / 2);
    int base_height = monitor_rect->height / stack_count;
    int extra_pixels = monitor_rect->height % stack_count;
    int stack_y = monitor_rect->y;

    for (int i = 0; i < count && start_index < max_targets; i++) {
        if (i == primary_index) {
            continue;
        }

        int offset = start_index - 1;
        int height = base_height + (offset < extra_pixels ? 1 : 0);

        targets[start_index].window_id = windows[i];
        targets[start_index].geometry.x = stack_x;
        targets[start_index].geometry.y = stack_y;
        targets[start_index].geometry.width = stack_width;
        targets[start_index].geometry.height = height;

        stack_y += height;
        start_index++;
    }

    return start_index;
}

int calculate_main_stack_targets(const Window *windows, int count, Window primary_id,
                                 const WorkArea *monitor_rect,
                                 LayoutTarget *targets, int max_targets) {
    if (!windows || count <= 0 || !monitor_rect || !targets || max_targets <= 0) {
        return 0;
    }

    int target_count = (count < max_targets) ? count : max_targets;
    int primary_index = find_window_index(windows, count, primary_id);
    if (primary_index < 0) {
        primary_index = 0;
    }

    int next_index = copy_primary_target(windows, count, primary_index,
                                         monitor_rect, targets, target_count);
    return append_stack_targets(windows, count, primary_index,
                                monitor_rect, targets, next_index, target_count);
}

gboolean layout_main_stack(Display *display, const Window *windows, int count,
                           Window primary_id, const WorkArea *monitor_rect) {
    LayoutTarget targets[MAX_WINDOWS];
    int target_count = calculate_main_stack_targets(windows, count, primary_id,
                                                    monitor_rect, targets, MAX_WINDOWS);

    if (!display || target_count <= 0) {
        return FALSE;
    }

    for (int i = 0; i < target_count; i++) {
        WindowSizeHints size_hints;
        memset(&size_hints, 0, sizeof(size_hints));
        get_window_size_hints(display, targets[i].window_id, &size_hints);
        apply_window_position(display, targets[i].window_id, &targets[i].geometry, &size_hints);
    }

    return TRUE;
}
