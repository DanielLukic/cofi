#include "sessions_zellij_windows.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>

#include "app_data.h"
#include "display.h"
#include "log.h"

static const char *next_arg(const char *data, size_t len, size_t *offset) {
    while (*offset < len && data[*offset] == '\0') (*offset)++;
    if (*offset >= len) return NULL;

    const char *arg = data + *offset;
    const char *end = memchr(arg, '\0', len - *offset);
    if (!end) {
        *offset = len;
        return NULL;
    }

    *offset = (size_t)(end - data) + 1;
    return arg;
}

static gboolean basename_is_zellij(const char *arg) {
    if (!arg || arg[0] == '\0') return FALSE;
    const char *slash = strrchr(arg, '/');
    const char *base = slash ? slash + 1 : arg;
    return strcmp(base, "zellij") == 0;
}

static gboolean is_server_process(const char *cmdline, size_t len) {
    size_t offset = 0;
    const char *arg;
    while ((arg = next_arg(cmdline, len, &offset)) != NULL) {
        if (strcmp(arg, "--server") == 0) return TRUE;
    }
    return FALSE;
}

static gboolean session_arg_matches(const char *arg, const char *session_name) {
    return arg && session_name && strcmp(arg, session_name) == 0;
}

static gboolean is_non_client_subcommand(const char *arg) {
    return strcmp(arg, "action") == 0 ||
           strcmp(arg, "delete-all-sessions") == 0 ||
           strcmp(arg, "delete-session") == 0 ||
           strcmp(arg, "kill-all-sessions") == 0 ||
           strcmp(arg, "kill-session") == 0 ||
           strcmp(arg, "list-aliases") == 0 ||
           strcmp(arg, "list-sessions") == 0 ||
           strcmp(arg, "plugin") == 0 ||
           strcmp(arg, "pipe") == 0 ||
           strcmp(arg, "run") == 0 ||
           strcmp(arg, "setup") == 0;
}

static gboolean match_attach_args(const char *cmdline, size_t len,
                                  size_t offset, const char *session_name) {
    const char *arg;
    while ((arg = next_arg(cmdline, len, &offset)) != NULL) {
        if (strcmp(arg, "--create") == 0 || strcmp(arg, "-c") == 0) continue;
        return session_arg_matches(arg, session_name);
    }
    return FALSE;
}

gboolean sessions_zellij_cmdline_matches_session(const char *cmdline,
                                                 size_t len,
                                                 const char *session_name) {
    if (!cmdline || len == 0 || !session_name || session_name[0] == '\0') return FALSE;
    if (is_server_process(cmdline, len)) return FALSE;

    size_t offset = 0;
    const char *arg0 = next_arg(cmdline, len, &offset);
    if (!basename_is_zellij(arg0)) return FALSE;

    gboolean session_option_matches = FALSE;
    const char *arg;
    while ((arg = next_arg(cmdline, len, &offset)) != NULL) {
        if (strcmp(arg, "--session") == 0 || strcmp(arg, "-s") == 0) {
            const char *name = next_arg(cmdline, len, &offset);
            session_option_matches = session_arg_matches(name, session_name);
            continue;
        }

        if (strcmp(arg, "a") == 0 || strcmp(arg, "attach") == 0) {
            return session_option_matches ||
                   match_attach_args(cmdline, len, offset, session_name);
        }

        if (is_non_client_subcommand(arg)) return FALSE;
    }

    return session_option_matches;
}

gboolean sessions_windowid_from_environ(const char *environ_data,
                                        size_t len,
                                        Window *window_out) {
    if (window_out) *window_out = 0;
    if (!environ_data || len == 0) return FALSE;

    size_t offset = 0;
    const char *entry;
    while ((entry = next_arg(environ_data, len, &offset)) != NULL) {
        if (strncmp(entry, "WINDOWID=", 9) != 0) continue;

        errno = 0;
        char *end = NULL;
        unsigned long id = strtoul(entry + 9, &end, 10);
        if (errno != 0 || end == entry + 9 || *end != '\0' || id == 0) return FALSE;

        if (window_out) *window_out = (Window)id;
        return TRUE;
    }

    return FALSE;
}

static gboolean read_proc_file(const char *path, gchar **contents, gsize *len) {
    GError *error = NULL;
    gboolean ok = g_file_get_contents(path, contents, len, &error);
    if (!ok) g_clear_error(&error);
    return ok;
}

static gboolean window_is_valid(Display *display, Window window) {
    if (!display || !window) return FALSE;
    XWindowAttributes attrs;
    return XGetWindowAttributes(display, window, &attrs) != 0;
}

gboolean sessions_activate_zellij_window(AppData *app, const char *session_name) {
    if (!app || !app->display || !session_name || session_name[0] == '\0') return FALSE;

    GDir *dir = g_dir_open("/proc", 0, NULL);
    if (!dir) return FALSE;

    gboolean activated = FALSE;
    const char *entry;
    while ((entry = g_dir_read_name(dir)) != NULL) {
        if (!g_ascii_isdigit(entry[0])) continue;

        gchar *cmdline_path = g_strdup_printf("/proc/%s/cmdline", entry);
        gchar *cmdline = NULL;
        gsize cmdline_len = 0;
        gboolean cmdline_ok = read_proc_file(cmdline_path, &cmdline, &cmdline_len);
        g_free(cmdline_path);
        if (!cmdline_ok) {
            g_free(cmdline);
            continue;
        }

        gboolean matches = sessions_zellij_cmdline_matches_session(cmdline,
                                                                   cmdline_len,
                                                                   session_name);
        g_free(cmdline);
        if (!matches) continue;

        gchar *environ_path = g_strdup_printf("/proc/%s/environ", entry);
        gchar *environ_data = NULL;
        gsize environ_len = 0;
        gboolean environ_ok = read_proc_file(environ_path, &environ_data, &environ_len);
        g_free(environ_path);
        if (!environ_ok) {
            g_free(environ_data);
            continue;
        }

        Window window = 0;
        gboolean has_window = sessions_windowid_from_environ(environ_data,
                                                             environ_len,
                                                             &window);
        g_free(environ_data);
        if (!has_window || !window_is_valid(app->display, window)) continue;

        activate_window(app->display, window);
        log_info("USER: zellij: activated existing window for session '%s'", session_name);
        activated = TRUE;
        break;
    }

    g_dir_close(dir);
    return activated;
}
