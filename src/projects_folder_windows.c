#include "projects_folder_windows.h"

#include <string.h>
#include <strings.h>

#include "types.h"

static gboolean is_caja_folder_window(const WindowInfo *win, const char *basename) {
    if (!win || !basename || basename[0] == '\0') return FALSE;
    if (strcasecmp(win->class_name, "Caja") != 0) return FALSE;
    if (strcasecmp(win->instance, "caja") != 0) return FALSE;
    if (win->type[0] != '\0' && strcmp(win->type, WINDOW_TYPE_NORMAL) != 0) return FALSE;
    return strcmp(win->title, basename) == 0;
}

static int stack_position(Window id, const Window *stack, unsigned long stack_count) {
    if (!stack) return -1;
    for (unsigned long i = 0; i < stack_count; i++) {
        if (stack[i] == id) return (int)i;
    }
    return -1;
}

gboolean projects_find_caja_folder_window(const WindowInfo *windows,
                                          int window_count,
                                          const Window *stack,
                                          unsigned long stack_count,
                                          const char *path,
                                          Window *window_out) {
    if (window_out) *window_out = 0;
    if (!windows || window_count <= 0 || !path || path[0] == '\0') return FALSE;

    gchar *basename = g_path_get_basename(path);
    if (!basename || basename[0] == '\0' ||
        strcmp(basename, ".") == 0 || strcmp(basename, "/") == 0) {
        g_free(basename);
        return FALSE;
    }

    Window best = 0;
    int best_stack_pos = -1;
    int best_match_index = -1;

    for (int i = 0; i < window_count; i++) {
        if (!is_caja_folder_window(&windows[i], basename)) continue;

        int pos = stack_position(windows[i].id, stack, stack_count);
        if (pos >= best_stack_pos || (best_stack_pos < 0 && i > best_match_index)) {
            best = windows[i].id;
            best_stack_pos = pos;
            best_match_index = i;
        }
    }

    g_free(basename);
    if (!best) return FALSE;
    if (window_out) *window_out = best;
    return TRUE;
}
