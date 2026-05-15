#include "process_windows.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "app_data.h"
#include "x11_utils.h"

static int default_find_window_for_pid(AppData *app, pid_t pid, Window *window_out) {
    if (!app || pid <= 0 || !window_out) return 0;
    for (int i = 0; i < app->window_count; i++) {
        if (get_window_pid(app->display, app->windows[i].id) == (int)pid) {
            *window_out = app->windows[i].id;
            return 1;
        }
    }
    return 0;
}

static int default_read_parent_pid(pid_t pid) {
    char path[PATH_MAX];
    char line[256];
    int ppid = 0;

    g_snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    while (fgets(line, sizeof(line), f)) {
        if (g_str_has_prefix(line, "PPid:")) {
            char *ptr = line + 5;
            while (*ptr == ' ' || *ptr == '\t') ptr++;
            ppid = (int)strtol(ptr, NULL, 10);
            break;
        }
    }

    fclose(f);
    return ppid;
}

static int (*find_window_for_pid_impl)(AppData *, pid_t, Window *) = default_find_window_for_pid;
static int (*read_parent_pid_impl)(pid_t) = default_read_parent_pid;

gboolean process_find_window_for_pid(AppData *app, pid_t pid, Window *window_out) {
    return find_window_for_pid_impl(app, pid, window_out) ? TRUE : FALSE;
}

int process_read_parent_pid(pid_t pid) {
    return read_parent_pid_impl(pid);
}

gboolean process_find_window_for_pid_ancestry(AppData *app,
                                              pid_t pid,
                                              int max_depth,
                                              Window *window_out) {
    if (window_out) *window_out = 0;
    if (!app || pid <= 0 || !window_out) return FALSE;
    if (max_depth <= 0) max_depth = 32;

    pid_t target_pid = pid;
    for (int depth = 0; depth < max_depth; depth++) {
        if (process_find_window_for_pid(app, target_pid, window_out)) {
            return TRUE;
        }

        int ppid = process_read_parent_pid(target_pid);
        if (ppid <= 1) return FALSE;
        target_pid = (pid_t)ppid;
    }

    return FALSE;
}

#ifdef COFI_TESTING
void process_set_window_resolvers_test_hook(int (*window_for_pid)(AppData *, pid_t, Window *),
                                            int (*parent_pid)(pid_t)) {
    find_window_for_pid_impl = window_for_pid ? window_for_pid : default_find_window_for_pid;
    read_parent_pid_impl = parent_pid ? parent_pid : default_read_parent_pid;
}
#endif
