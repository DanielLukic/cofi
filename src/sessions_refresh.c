#include "sessions.h"

#include "app_data.h"
#include "sessions_parse.h"

#include <gtk/gtk.h>

static void refresh_tmux_backend(SessionsMode *mode) {
    gchar *tmux = g_find_program_in_path("tmux");
    if (tmux) {
        gchar *argv[] = {
            tmux,
            "list-sessions",
            "-F",
            "#{session_name}\t#{session_windows}\t#{session_attached}",
            NULL
        };
        gchar *stdout_str = NULL;
        gchar *stderr_str = NULL;
        gint wait_status = 0;
        GError *error = NULL;

        gboolean spawned = g_spawn_sync(NULL, argv, NULL, G_SPAWN_DEFAULT, NULL, NULL,
                                        &stdout_str, &stderr_str, &wait_status, &error);
        if (!spawned) {
            g_snprintf(mode->last_error, sizeof(mode->last_error),
                       "tmux failed: %s", error ? error->message : "unknown error");
            g_clear_error(&error);
        } else if (!g_spawn_check_wait_status(wait_status, &error)) {
            const char *message = stderr_str && stderr_str[0] ? stderr_str : "No tmux server";
            g_strlcpy(mode->last_error, message, sizeof(mode->last_error));
            g_strstrip(mode->last_error);
            g_clear_error(&error);
        } else {
            char parse_error[256];
            mode->session_count = sessions_parse_tmux_list(stdout_str, mode->sessions,
                                                           MAX_SESSIONS,
                                                           parse_error,
                                                           sizeof(parse_error));
            if (mode->session_count == 0) {
                g_strlcpy(mode->last_error, parse_error[0] ? parse_error : "No tmux sessions",
                          sizeof(mode->last_error));
            }
        }

        g_free(stdout_str);
        g_free(stderr_str);
        g_free(tmux);
    } else {
        g_strlcpy(mode->last_error, "tmux not found", sizeof(mode->last_error));
    }
}

static void refresh_zellij_backend(SessionsMode *mode) {
    gchar *zellij = g_find_program_in_path("zellij");
    if (!zellij) return;
    if (mode->session_count >= MAX_SESSIONS) {
        g_free(zellij);
        return;
    }

    gchar *argv[] = {zellij, "list-sessions", "--short", NULL};
    gchar *stdout_str = NULL;
    gint wait_status = 0;
    GError *error = NULL;

    gboolean spawned = g_spawn_sync(NULL, argv, NULL, G_SPAWN_STDERR_TO_DEV_NULL,
                                    NULL, NULL, &stdout_str, NULL, &wait_status, &error);
    if (spawned && g_spawn_check_wait_status(wait_status, &error)) {
        char parse_error[256];
        int added = sessions_parse_zellij_list(stdout_str,
                                               mode->sessions + mode->session_count,
                                               MAX_SESSIONS - mode->session_count,
                                               parse_error, sizeof(parse_error));
        mode->session_count += added;
        if (added > 0) {
            mode->last_error[0] = '\0';
        }
    }
    g_clear_error(&error);
    g_free(stdout_str);
    g_free(zellij);
}

static void refresh_zoxide_backend(SessionsMode *mode) {
    gchar *zoxide = g_find_program_in_path("zoxide");
    if (zoxide) {
        gchar *argv[] = {zoxide, "query", "-l", NULL};
        gchar *stdout_str = NULL;
        gint wait_status = 0;
        GError *error = NULL;

        gboolean spawned = g_spawn_sync(NULL, argv, NULL, G_SPAWN_STDERR_TO_DEV_NULL,
                                        NULL, NULL, &stdout_str, NULL, &wait_status, &error);
        if (spawned && g_spawn_check_wait_status(wait_status, &error)) {
            char parse_error[256];
            mode->folder_count = sessions_parse_zoxide_list(stdout_str, mode->folders,
                                                            MAX_SESSION_FOLDERS,
                                                            parse_error,
                                                            sizeof(parse_error));
        }
        g_clear_error(&error);
        g_free(stdout_str);
        g_free(zoxide);
    }
}

void sessions_refresh(AppData *app) {
    if (!app) return;
    SessionsMode *mode = &app->sessions_mode;
    sessions_clear_folders(mode->folders, mode->folder_count);
    mode->session_count = 0;
    mode->folder_count = 0;
    mode->filtered_count = 0;
    mode->last_error[0] = '\0';

    refresh_tmux_backend(mode);
    refresh_zellij_backend(mode);
    refresh_zoxide_backend(mode);

    const char *query = app->entry ? gtk_entry_get_text(GTK_ENTRY(app->entry)) : "";
    sessions_filter(app, query);
}
