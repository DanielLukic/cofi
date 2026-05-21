#include "projects_remote_scope.h"

#include <string.h>

#include "display.h"
#include "log.h"
#include "projects_parse.h"
#include "selection.h"

typedef struct {
    char host[128];
    ProjectSessionEntry sessions[MAX_PROJECTS];
    int session_count;
    ProjectFolder folders[MAX_PROJECT_FOLDERS];
    int folder_count;
    gboolean active;
    gboolean loading;
    char status_message[256];
} ProjectsRemoteScopeState;

typedef struct {
    AppData *app;
    char host[128];
    gboolean ok;
    char error[256];
    ProjectSessionEntry sessions[MAX_PROJECTS];
    int session_count;
    ProjectFolder folders[MAX_PROJECT_FOLDERS];
    int folder_count;
} ProjectsRemoteFetchResult;

static ProjectsRemoteScopeState s_state;

typedef gboolean (*ProjectsRemoteExecFn)(const char *host,
                                         const char *const *remote_argv,
                                         gchar **stdout_out,
                                         gchar **stderr_out);
static gboolean default_exec(const char *host,
                             const char *const *remote_argv,
                             gchar **stdout_out,
                             gchar **stderr_out);
static ProjectsRemoteExecFn s_exec = default_exec;

static int build_ssh_argv(const char *host,
                          const char *const *remote_argv,
                          gchar **argv_out,
                          int argv_cap) {
    if (!host || !host[0] || !remote_argv || !remote_argv[0] ||
        !argv_out || argv_cap < 8) {
        return 0;
    }

    int idx = 0;
    argv_out[idx++] = "ssh";
    argv_out[idx++] = "-o";
    argv_out[idx++] = "BatchMode=yes";
    argv_out[idx++] = "-o";
    argv_out[idx++] = "ConnectTimeout=4";
    argv_out[idx++] = (gchar *)host;

    for (int i = 0; remote_argv[i] && idx < argv_cap - 1; i++) {
        argv_out[idx++] = (gchar *)remote_argv[i];
    }
    argv_out[idx] = NULL;
    return idx;
}

static void clear_remote_data(void) {
    projects_clear_folders(s_state.folders, s_state.folder_count);
    s_state.folder_count = 0;
    s_state.session_count = 0;
}

static void clear_status_message(void) {
    s_state.status_message[0] = '\0';
}

static void set_status_message(const char *message) {
    if (!message) {
        clear_status_message();
        return;
    }
    g_strlcpy(s_state.status_message, message, sizeof(s_state.status_message));
}

void projects_remote_scope_init(void) {
    static gboolean initialized = FALSE;
    if (initialized) return;
    memset(&s_state, 0, sizeof(s_state));
    initialized = TRUE;
}

static void mark_local_scope(void) {
    clear_remote_data();
    s_state.active = FALSE;
    s_state.loading = FALSE;
    s_state.host[0] = '\0';
}

void projects_remote_scope_clear(void) {
    projects_remote_scope_init();
    mark_local_scope();
}

gboolean projects_remote_scope_is_active(void) {
    return s_state.active;
}

gboolean projects_remote_scope_is_loading(void) {
    return s_state.loading;
}

const char *projects_remote_scope_current_host(void) {
    return s_state.host;
}

const char *projects_remote_scope_status_message(void) {
    return s_state.status_message;
}

void projects_remote_scope_clear_status_message(void) {
    projects_remote_scope_init();
    clear_status_message();
}

static gboolean parse_remote_outputs(const char *host,
                                     const char *tmux_out,
                                     const char *zellij_out,
                                     const char *zoxide_out,
                                     ProjectSessionEntry *sessions_out,
                                     int *session_count_out,
                                     ProjectFolder *folders_out,
                                     int *folder_count_out,
                                     char *error_out,
                                     size_t error_size) {
    if (error_out && error_size > 0) error_out[0] = '\0';
    if (!host || !host[0]) {
        g_strlcpy(error_out, "Host is empty", error_size);
        return FALSE;
    }

    int session_count = 0;
    char parse_error[256];
    session_count += projects_parse_tmux_list(tmux_out ? tmux_out : "",
                                              sessions_out + session_count,
                                              MAX_PROJECTS - session_count,
                                              parse_error,
                                              sizeof(parse_error));
    session_count += projects_parse_zellij_list(zellij_out ? zellij_out : "",
                                                sessions_out + session_count,
                                                MAX_PROJECTS - session_count,
                                                parse_error,
                                                sizeof(parse_error));

    int folder_count = projects_parse_zoxide_list(zoxide_out ? zoxide_out : "",
                                                  folders_out,
                                                  MAX_PROJECT_FOLDERS,
                                                  parse_error,
                                                  sizeof(parse_error));

    if (session_count == 0 && folder_count == 0) {
        g_snprintf(error_out, error_size, "Remote host '%s' returned no sessions or folders", host);
        return FALSE;
    }

    for (int i = 0; i < session_count; i++) {
        sessions_out[i].is_saved_remote = TRUE;
        g_strlcpy(sessions_out[i].remote_host, host, sizeof(sessions_out[i].remote_host));
        sessions_out[i].remote_cwd[0] = '\0';
    }

    for (int i = 0; i < folder_count; i++) {
        folders_out[i].is_remote = TRUE;
        g_strlcpy(folders_out[i].remote_host, host, sizeof(folders_out[i].remote_host));
    }

    *session_count_out = session_count;
    *folder_count_out = folder_count;
    return TRUE;
}

static gboolean default_exec(const char *host,
                             const char *const *remote_argv,
                             gchar **stdout_out,
                             gchar **stderr_out) {
    if (!host || !host[0] || !remote_argv || !remote_argv[0]) return FALSE;

    gchar *argv[16];
    int argc = build_ssh_argv(host, remote_argv, argv, (int)(sizeof(argv) / sizeof(argv[0])));
    if (argc <= 0) return FALSE;

    gint status = 0;
    GError *error = NULL;
    gboolean spawned = g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                                    stdout_out, stderr_out, &status, &error);
    if (!spawned) {
        g_clear_error(&error);
        return FALSE;
    }

    gboolean ok = g_spawn_check_wait_status(status, &error);
    g_clear_error(&error);
    return ok;
}

static void fetch_remote_sync(ProjectsRemoteFetchResult *result) {
    gchar *tmux_out = NULL;
    gchar *tmux_err = NULL;
    gchar *zellij_out = NULL;
    gchar *zellij_err = NULL;
    gchar *zoxide_out = NULL;
    gchar *zoxide_err = NULL;

    const char *tmux_argv[] = {
        "tmux", "list-sessions", "-F", "#{session_name}\t#{session_windows}\t#{session_attached}", NULL
    };
    const char *zellij_argv[] = {"zellij", "list-sessions", "--short", NULL};
    const char *zoxide_argv[] = {"zoxide", "query", "-l", NULL};

    gboolean tmux_ok = s_exec(result->host, tmux_argv, &tmux_out, &tmux_err);
    gboolean zellij_ok = s_exec(result->host, zellij_argv, &zellij_out, &zellij_err);
    gboolean zoxide_ok = s_exec(result->host, zoxide_argv, &zoxide_out, &zoxide_err);

    if (!tmux_ok && !zellij_ok && !zoxide_ok) {
        const char *msg = (tmux_err && tmux_err[0]) ? tmux_err :
                          (tmux_out && tmux_out[0]) ? tmux_out :
                          (zellij_err && zellij_err[0]) ? zellij_err :
                          (zellij_out && zellij_out[0]) ? zellij_out :
                          (zoxide_err && zoxide_err[0]) ? zoxide_err :
                          (zoxide_out && zoxide_out[0]) ? zoxide_out :
                          "SSH commands failed";
        char reason[160];
        g_strlcpy(reason, msg, sizeof(reason));
        g_strstrip(reason);
        for (char *p = reason; *p; p++) {
            if (*p == '\n' || *p == '\r' || *p == '\t') *p = ' ';
        }
        g_snprintf(result->error, sizeof(result->error),
                   "Remote fetch failed for '%s': %s",
                   result->host,
                   reason[0] ? reason : "SSH commands failed");
        goto cleanup;
    }

    result->ok = parse_remote_outputs(result->host,
                                      tmux_ok ? tmux_out : "",
                                      zellij_ok ? zellij_out : "",
                                      zoxide_ok ? zoxide_out : "",
                                      result->sessions,
                                      &result->session_count,
                                      result->folders,
                                      &result->folder_count,
                                      result->error,
                                      sizeof(result->error));

cleanup:
    g_free(tmux_out);
    g_free(tmux_err);
    g_free(zellij_out);
    g_free(zellij_err);
    g_free(zoxide_out);
    g_free(zoxide_err);
}

static gboolean fetch_result_apply_idle(gpointer data) {
    ProjectsRemoteFetchResult *result = (ProjectsRemoteFetchResult *)data;
    AppData *app = result->app;

    projects_remote_scope_init();
    clear_remote_data();
    s_state.loading = FALSE;

    if (result->ok) {
        clear_status_message();
        g_strlcpy(s_state.host, result->host, sizeof(s_state.host));
        s_state.active = TRUE;
        s_state.session_count = result->session_count;
        s_state.folder_count = result->folder_count;
        memcpy(s_state.sessions, result->sessions, sizeof(ProjectSessionEntry) * (size_t)result->session_count);
        for (int i = 0; i < result->folder_count; i++) {
            s_state.folders[i].path = g_strdup(result->folders[i].path);
            s_state.folders[i].label = g_strdup(result->folders[i].label);
            s_state.folders[i].is_remote = TRUE;
            g_strlcpy(s_state.folders[i].remote_host, result->host, sizeof(s_state.folders[i].remote_host));
        }
        log_info("projects: remote scope set to host '%s' (%d sessions, %d folders)",
                 s_state.host, s_state.session_count, s_state.folder_count);
    } else {
        mark_local_scope();
        set_status_message(result->error[0] ? result->error : "Remote fetch failed");
        log_warn("%s", s_state.status_message);
    }

    if (app) {
        projects_refresh(app);
        reset_selection(app);
        update_scroll_position(app);
        update_display(app);
    }

    projects_clear_folders(result->folders, result->folder_count);
    g_free(result);
    return G_SOURCE_REMOVE;
}

static void fetch_worker(GTask *task,
                         gpointer source_object,
                         gpointer task_data,
                         GCancellable *cancellable) {
    (void)task;
    (void)source_object;
    (void)cancellable;
    ProjectsRemoteFetchResult *result = (ProjectsRemoteFetchResult *)task_data;
    fetch_remote_sync(result);
    g_idle_add(fetch_result_apply_idle, result);
}

void projects_remote_scope_begin_fetch(AppData *app, const char *host) {
    projects_remote_scope_init();
    if (!host || !host[0]) return;

    clear_remote_data();
    clear_status_message();
    s_state.active = FALSE;
    s_state.loading = TRUE;
    g_strlcpy(s_state.host, host, sizeof(s_state.host));

    ProjectsRemoteFetchResult *result = g_new0(ProjectsRemoteFetchResult, 1);
    result->app = app;
    g_strlcpy(result->host, host, sizeof(result->host));

    GTask *task = g_task_new(NULL, NULL, NULL, NULL);
    g_task_set_task_data(task, result, NULL);
    g_task_run_in_thread(task, fetch_worker);
    g_object_unref(task);
}

gboolean projects_remote_scope_apply(ProjectsMode *mode) {
    projects_remote_scope_init();
    if (!mode) return FALSE;

    if (s_state.loading) {
        mode->session_count = 0;
        mode->folder_count = 0;
        mode->filtered_count = 0;
        g_snprintf(mode->last_error, sizeof(mode->last_error),
                   "Loading remote projects for %s...", s_state.host);
        return TRUE;
    }

    if (!s_state.active) return FALSE;

    int previous_folder_count = mode->folder_count;
    mode->session_count = s_state.session_count;
    mode->folder_count = s_state.folder_count;
    mode->filtered_count = 0;
    mode->last_error[0] = '\0';
    memcpy(mode->projects, s_state.sessions, sizeof(ProjectSessionEntry) * (size_t)mode->session_count);
    projects_clear_folders(mode->folders, previous_folder_count);
    for (int i = 0; i < s_state.folder_count; i++) {
        mode->folders[i].path = g_strdup(s_state.folders[i].path);
        mode->folders[i].label = g_strdup(s_state.folders[i].label);
        mode->folders[i].is_remote = s_state.folders[i].is_remote;
        g_strlcpy(mode->folders[i].remote_host, s_state.folders[i].remote_host,
                  sizeof(mode->folders[i].remote_host));
    }
    return TRUE;
}

#ifdef COFI_TESTING
void projects_remote_scope_reset_for_test(void) {
    projects_remote_scope_clear();
    clear_status_message();
    s_exec = default_exec;
}

void projects_remote_scope_set_exec_for_test(ProjectsRemoteExecFn exec_fn) {
    s_exec = exec_fn ? exec_fn : default_exec;
}

gboolean projects_remote_scope_load_from_outputs_for_test(const char *host,
                                                          const char *tmux_out,
                                                          const char *zellij_out,
                                                          const char *zoxide_out,
                                                          char *error_out,
                                                          size_t error_size) {
    projects_remote_scope_init();
    clear_remote_data();
    s_state.active = FALSE;
    s_state.loading = FALSE;
    g_strlcpy(s_state.host, host ? host : "", sizeof(s_state.host));

    gboolean ok = parse_remote_outputs(host,
                                       tmux_out,
                                       zellij_out,
                                       zoxide_out,
                                       s_state.sessions,
                                       &s_state.session_count,
                                       s_state.folders,
                                       &s_state.folder_count,
                                       error_out,
                                       error_size);
    s_state.active = ok;
    if (ok) {
        /* Folder entries already own parsed allocations. */
    }
    return ok;
}

void projects_remote_scope_set_loading_for_test(const char *host, gboolean loading) {
    projects_remote_scope_init();
    s_state.loading = loading;
    if (host) g_strlcpy(s_state.host, host, sizeof(s_state.host));
}

void projects_remote_scope_set_active_for_test(gboolean active) {
    projects_remote_scope_init();
    s_state.active = active;
}

gboolean projects_remote_scope_fetch_sync_for_test(const char *host, char *error_out, size_t error_size) {
    ProjectsRemoteFetchResult result;
    memset(&result, 0, sizeof(result));
    g_strlcpy(result.host, host ? host : "", sizeof(result.host));
    fetch_remote_sync(&result);
    if (result.ok) {
        clear_status_message();
    } else {
        set_status_message(result.error[0] ? result.error : "Remote fetch failed");
    }
    if (error_out && error_size > 0) {
        g_strlcpy(error_out, result.error, error_size);
    }
    return result.ok;
}

void projects_remote_scope_set_status_for_test(const char *status) {
    projects_remote_scope_init();
    set_status_message(status);
}

int projects_remote_scope_build_ssh_argv_for_test(const char *host,
                                                  const char *const *remote_argv,
                                                  gchar **argv_out,
                                                  int argv_cap) {
    return build_ssh_argv(host, remote_argv, argv_out, argv_cap);
}
#endif
