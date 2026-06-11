#include "projects/projects.h"

#include "core/app/app_data.h"
#include "projects/projects_parse.h"
#include "projects/projects_exec.h"
#include "projects/projects_remote_store.h"
#include "projects/projects_remote_scope.h"

#include <gtk/gtk.h>

static void refresh_tmux_backend(AppData *app, ProjectsMode *mode) {
    char err[128] = {0};
    gchar *tmux = projects_resolve_tool(&app->config, PROJECT_TOOL_TMUX,
                                        err, sizeof(err));
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
            mode->session_count = projects_parse_tmux_list(stdout_str, mode->projects,
                                                           MAX_PROJECTS,
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
        g_strlcpy(mode->last_error, err[0] ? err : "tmux not found",
                  sizeof(mode->last_error));
    }
}

static void refresh_zellij_backend(AppData *app, ProjectsMode *mode) {
    char err[128] = {0};
    gchar *zellij = projects_resolve_tool(&app->config, PROJECT_TOOL_ZELLIJ,
                                          err, sizeof(err));
    if (!zellij) return;
    if (mode->session_count >= MAX_PROJECTS) {
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
        int added = projects_parse_zellij_list(stdout_str,
                                               mode->projects + mode->session_count,
                                               MAX_PROJECTS - mode->session_count,
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

static void refresh_zoxide_backend(AppData *app, ProjectsMode *mode) {
    char err[128] = {0};
    gchar *zoxide = projects_resolve_tool(&app->config, PROJECT_TOOL_ZOXIDE,
                                          err, sizeof(err));
    if (zoxide) {
        gchar *argv[] = {zoxide, "query", "-l", NULL};
        gchar *stdout_str = NULL;
        gint wait_status = 0;
        GError *error = NULL;

        gboolean spawned = g_spawn_sync(NULL, argv, NULL, G_SPAWN_STDERR_TO_DEV_NULL,
                                        NULL, NULL, &stdout_str, NULL, &wait_status, &error);
        if (spawned && g_spawn_check_wait_status(wait_status, &error)) {
            char parse_error[256];
            mode->folder_count = projects_parse_zoxide_list(stdout_str, mode->folders,
                                                            MAX_PROJECT_FOLDERS,
                                                            parse_error,
                                                            sizeof(parse_error));
        }
        g_clear_error(&error);
        g_free(stdout_str);
        g_free(zoxide);
    } else if (err[0]) {
        /* zoxide is optional; log via resolver but don't replace session errors. */
    }
}

void projects_refresh(AppData *app) {
    if (!app) return;
    ProjectsMode *mode = &app->projects_mode;
    projects_clear_folders(mode->folders, mode->folder_count);
    mode->session_count = 0;
    mode->folder_count = 0;
    mode->primary_folder_count = 0;
    mode->locate_folder_count = 0;
    mode->filtered_count = 0;
    mode->last_error[0] = '\0';

    if (projects_remote_scope_apply(mode)) {
        mode->primary_folder_count = mode->folder_count;
        const char *query = app->entry ? gtk_entry_get_text(GTK_ENTRY(app->entry)) : "";
        projects_filter(app, query);
        return;
    }

    refresh_tmux_backend(app, mode);
    refresh_zellij_backend(app, mode);
    projects_remote_store_reload();
    mode->session_count = projects_remote_store_append_sessions(mode->projects,
                                                                mode->session_count,
                                                                MAX_PROJECTS);
    refresh_zoxide_backend(app, mode);
    mode->primary_folder_count = mode->folder_count;

    const char *query = app->entry ? gtk_entry_get_text(GTK_ENTRY(app->entry)) : "";
    projects_filter(app, query);
}
