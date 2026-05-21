#include "projects_remote_windows.h"

#include <string.h>

#include <X11/Xlib.h>

#include "app_data.h"
#include "display.h"
#include "log.h"
#include "projects_window_env.h"

static const char *remote_next_arg(const char *data, size_t len, size_t *offset) {
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

static int split_cmdline(const char *cmdline, size_t len, const char **argv, int max_argv) {
    if (!cmdline || len == 0 || !argv || max_argv <= 0) return 0;
    size_t offset = 0;
    int count = 0;
    const char *arg = NULL;
    while ((arg = remote_next_arg(cmdline, len, &offset)) != NULL && count < max_argv) {
        argv[count++] = arg;
    }
    return count;
}

static gboolean basename_is_ssh(const char *arg) {
    if (!arg || arg[0] == '\0') return FALSE;
    const char *slash = strrchr(arg, '/');
    const char *base = slash ? slash + 1 : arg;
    return strcmp(base, "ssh") == 0;
}

static gboolean match_tmux_attach_tokens(const char **argv, int argc, int start, const char *session_name) {
    if (start >= argc) return FALSE;
    if (strcmp(argv[start], "attach-session") != 0 && strcmp(argv[start], "attach") != 0) return FALSE;

    for (int i = start + 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            return strcmp(argv[i + 1], session_name) == 0;
        }
    }
    return FALSE;
}

static gboolean match_zellij_attach_tokens(const char **argv, int argc, int start, const char *session_name) {
    if (start >= argc) return FALSE;
    if (strcmp(argv[start], "attach") != 0 && strcmp(argv[start], "a") != 0) return FALSE;

    for (int i = start + 1; i < argc; i++) {
        if (strcmp(argv[i], "--create") == 0 || strcmp(argv[i], "-c") == 0) continue;
        return strcmp(argv[i], session_name) == 0;
    }
    return FALSE;
}

gboolean projects_remote_cmdline_matches_attach(const char *cmdline,
                                                size_t len,
                                                const char *host,
                                                const char *tool,
                                                const char *session_name) {
    if (!cmdline || len == 0 || !host || host[0] == '\0' ||
        !tool || tool[0] == '\0' || !session_name || session_name[0] == '\0') {
        return FALSE;
    }

    const char *argv[64];
    int argc = split_cmdline(cmdline, len, argv, (int)(sizeof(argv) / sizeof(argv[0])));
    if (argc < 5) return FALSE;
    if (!basename_is_ssh(argv[0])) return FALSE;

    for (int i = 1; i < argc - 2; i++) {
        if (strcmp(argv[i], host) != 0) continue;
        if (strcmp(argv[i + 1], tool) != 0) return FALSE;

        if (strcmp(tool, "tmux") == 0) {
            return match_tmux_attach_tokens(argv, argc, i + 2, session_name);
        }
        if (strcmp(tool, "zellij") == 0) {
            return match_zellij_attach_tokens(argv, argc, i + 2, session_name);
        }
        return FALSE;
    }

    return FALSE;
}

static gboolean remote_read_proc_file(const char *path, gchar **contents, gsize *len) {
    GError *error = NULL;
    gboolean ok = g_file_get_contents(path, contents, len, &error);
    if (!ok) g_clear_error(&error);
    return ok;
}

static gboolean remote_window_is_valid(Display *display, Window window) {
    if (!display || !window) return FALSE;
    XWindowAttributes attrs;
    return XGetWindowAttributes(display, window, &attrs) != 0;
}

gboolean projects_activate_remote_attach_window(AppData *app,
                                                const char *host,
                                                const char *tool,
                                                const char *session_name) {
    if (!app || !app->display) return FALSE;
    if (!host || host[0] == '\0' || !tool || tool[0] == '\0' ||
        !session_name || session_name[0] == '\0') return FALSE;

    GDir *dir = g_dir_open("/proc", 0, NULL);
    if (!dir) return FALSE;

    gboolean activated = FALSE;
    const char *entry = NULL;
    while ((entry = g_dir_read_name(dir)) != NULL) {
        if (!g_ascii_isdigit(entry[0])) continue;

        gchar *cmdline_path = g_strdup_printf("/proc/%s/cmdline", entry);
        gchar *cmdline_data = NULL;
        gsize cmdline_len = 0;
        gboolean cmdline_ok = remote_read_proc_file(cmdline_path, &cmdline_data, &cmdline_len);
        g_free(cmdline_path);
        if (!cmdline_ok) {
            g_free(cmdline_data);
            continue;
        }

        gboolean matches = projects_remote_cmdline_matches_attach(cmdline_data,
                                                                  cmdline_len,
                                                                  host,
                                                                  tool,
                                                                  session_name);
        g_free(cmdline_data);
        if (!matches) continue;

        gchar *environ_path = g_strdup_printf("/proc/%s/environ", entry);
        gchar *environ_data = NULL;
        gsize environ_len = 0;
        gboolean environ_ok = remote_read_proc_file(environ_path, &environ_data, &environ_len);
        g_free(environ_path);
        if (!environ_ok) {
            g_free(environ_data);
            continue;
        }

        Window window = 0;
        gboolean has_window = projects_windowid_from_environ(environ_data, environ_len, &window);
        g_free(environ_data);
        if (!has_window || !remote_window_is_valid(app->display, window)) continue;

        activate_window(app->display, window);
        log_info("USER: remote %s: activated existing window for %s:%s",
                 tool, host, session_name);
        activated = TRUE;
        break;
    }

    g_dir_close(dir);
    return activated;
}
