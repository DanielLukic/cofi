#include "sessions_tmux_windows.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>

#include "app_data.h"
#include "display.h"
#include "log.h"
#include "process_windows.h"
#include "sessions_window_env.h"
#include "window_list.h"

int sessions_parse_tmux_client_pids(const char *output, pid_t *pids, int max_pids) {
    if (!output || !pids || max_pids <= 0) return 0;

    int count = 0;
    gchar **lines = g_strsplit(output, "\n", -1);
    for (int i = 0; lines[i] && count < max_pids; i++) {
        char *line = g_strstrip(lines[i]);
        if (line[0] == '\0') continue;

        errno = 0;
        char *end = NULL;
        long pid = strtol(line, &end, 10);
        if (errno != 0 || end == line || *end != '\0' || pid <= 1) continue;

        pids[count++] = (pid_t)pid;
    }
    g_strfreev(lines);
    return count;
}

static gboolean read_tmux_proc_file(const char *path, gchar **contents, gsize *len) {
    GError *error = NULL;
    gboolean ok = g_file_get_contents(path, contents, len, &error);
    if (!ok) g_clear_error(&error);
    return ok;
}

static gboolean tmux_window_is_valid(Display *display, Window window) {
    if (!display || !window) return FALSE;
    XWindowAttributes attrs;
    return XGetWindowAttributes(display, window, &attrs) != 0;
}

static gboolean window_from_client_environ(AppData *app, pid_t pid, Window *window_out) {
    if (window_out) *window_out = 0;
    if (!app || pid <= 0 || !window_out) return FALSE;

    gchar *path = g_strdup_printf("/proc/%d/environ", (int)pid);
    gchar *environ_data = NULL;
    gsize environ_len = 0;
    gboolean ok = read_tmux_proc_file(path, &environ_data, &environ_len);
    g_free(path);
    if (!ok) {
        g_free(environ_data);
        return FALSE;
    }

    Window window = 0;
    gboolean found = sessions_windowid_from_environ(environ_data, environ_len, &window);
    g_free(environ_data);
    if (!found || !tmux_window_is_valid(app->display, window)) return FALSE;

    *window_out = window;
    return TRUE;
}

gboolean sessions_activate_tmux_window(AppData *app, const char *session_name) {
    if (!app || !session_name || session_name[0] == '\0') return FALSE;

    gchar *tmux = g_find_program_in_path("tmux");
    if (!tmux) return FALSE;

    gchar *target = g_strdup_printf("=%s", session_name);
    gchar *argv[] = {
        tmux,
        "list-clients",
        "-t",
        target,
        "-F",
        "#{client_pid}",
        NULL
    };
    gchar *stdout_str = NULL;
    gint wait_status = 0;
    GError *error = NULL;

    gboolean spawned = g_spawn_sync(NULL, argv, NULL, G_SPAWN_STDERR_TO_DEV_NULL,
                                    NULL, NULL, &stdout_str, NULL, &wait_status, &error);
    gboolean ok = spawned && g_spawn_check_wait_status(wait_status, &error);
    g_clear_error(&error);
    g_free(target);
    g_free(tmux);
    if (!ok) {
        g_free(stdout_str);
        return FALSE;
    }

    pid_t pids[16];
    int count = sessions_parse_tmux_client_pids(stdout_str, pids, 16);
    g_free(stdout_str);
    if (count == 0) return FALSE;

    get_window_list(app);

    for (int i = 0; i < count; i++) {
        Window window = 0;
        if (window_from_client_environ(app, pids[i], &window) ||
            process_find_window_for_pid_ancestry(app, pids[i], 32, &window)) {
            activate_window(app->display, window);
            log_info("USER: tmux: activated existing window for session '%s'", session_name);
            return TRUE;
        }
    }

    return FALSE;
}
