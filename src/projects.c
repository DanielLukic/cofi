#include "projects.h"

#include <stdlib.h>
#include <string.h>
#include "app_data.h"
#include "detach_launch.h"
#include "display.h"
#include "fzf_algo.h"
#include "log.h"
#include "selection.h"
#include "projects_commands.h"
#include "projects_exec.h"
#include "projects_folder_windows.h"
#include "projects_parse.h"
#include "projects_remote_windows.h"
#include "projects_tmux_windows.h"
#include "projects_zellij_windows.h"
#include "projects_remote_store.h"
#include "projects_remote_scope.h"
#include "window_list.h"

#include <gtk/gtk.h>
#include <X11/Xatom.h>

static gboolean default_launch_in_terminal(const char *command) {
    return detach_launch_in_terminal_cmd(command);
}

static gboolean (*s_launch_in_terminal)(const char *command) = default_launch_in_terminal;

static gboolean default_launch_argv(const char *const *argv) {
    return detach_launch_argv_array(argv);
}

static gboolean (*s_launch_argv)(const char *const *argv) = default_launch_argv;

static gboolean default_run_session_command(const char *command) {
    if (!command || command[0] == '\0') return FALSE;

    gchar *stderr_str = NULL;
    gint wait_status = 0;
    GError *error = NULL;
    gboolean spawned = g_spawn_command_line_sync(command, NULL, &stderr_str,
                                                 &wait_status, &error);
    if (!spawned) {
        log_warn("tmux command spawn failed: %s", error ? error->message : "unknown error");
        g_clear_error(&error);
        g_free(stderr_str);
        return FALSE;
    }

    gboolean ok = g_spawn_check_wait_status(wait_status, &error);
    if (!ok) {
        if (stderr_str && stderr_str[0]) {
            g_strstrip(stderr_str);
            log_warn("tmux command failed: %s", stderr_str);
        } else {
            log_warn("tmux command failed: %s", error ? error->message : "unknown error");
        }
    }
    g_clear_error(&error);
    g_free(stderr_str);
    return ok;
}

static gboolean (*s_run_session_command)(const char *command) = default_run_session_command;

static gboolean default_exec_command(const char *command) {
    if (!command || command[0] == '\0') return FALSE;
    gint wait_status = 0;
    GError *error = NULL;
    gboolean spawned = g_spawn_command_line_sync(command, NULL, NULL,
                                                 &wait_status, &error);
    if (!spawned) {
        g_clear_error(&error);
        return FALSE;
    }
    gboolean ok = g_spawn_check_wait_status(wait_status, &error);
    g_clear_error(&error);
    return ok;
}

static gboolean (*s_exec_command)(const char *command) = default_exec_command;

static gchar *build_remote_terminal_title(const char *host, const char *name) {
    if (!host || !host[0] || !name || !name[0]) return NULL;
    return g_strdup_printf("%s:%s", host, name);
}

static CofiActionStatus launch_remote_session(AppData *app,
                                              const char *host,
                                              ProjectBackend backend,
                                              const char *session_name,
                                              const char *cwd,
                                              gboolean is_new) {
    const char *remote_tool = backend == PROJECT_BACKEND_ZELLIJ ? "zellij" : "tmux";
    if (!is_new &&
        projects_activate_remote_attach_window(app, host, remote_tool, session_name)) {
        return COFI_HANDLED_HIDE;
    }

    gchar *command = is_new
        ? projects_build_remote_new_command(backend, host, remote_tool, session_name, cwd && cwd[0] ? cwd : g_get_home_dir())
        : projects_build_remote_attach_command(backend, host, remote_tool, session_name);
    if (!command) return COFI_ACTION_ERROR;

    gchar *title = build_remote_terminal_title(host, session_name);
    gchar *launch_command = projects_with_terminal_title(command, title ? title : session_name);
    g_free(title);
    g_free(command);
    if (!launch_command) return COFI_ACTION_ERROR;

    gboolean ok = s_launch_in_terminal(launch_command);
    g_free(launch_command);
    if (!ok) return COFI_ACTION_ERROR;

    projects_remote_store_save_intent(host, backend, session_name, cwd ? cwd : "");
    return COFI_HANDLED_HIDE;
}

static ProjectSessionEntry *session_at_visible(AppData *app, int visible_idx) {
    if (!app) return NULL;
    ProjectsMode *mode = &app->projects_mode;
    if (visible_idx < 0 || visible_idx >= mode->filtered_count) return NULL;
    if (mode->filtered_rows[visible_idx].type != PROJECT_ROW_SESSION) return NULL;
    int raw = mode->filtered_rows[visible_idx].index;
    if (raw < 0 || raw >= mode->session_count) return NULL;
    return &mode->projects[raw];
}

static ProjectFolder *folder_at_visible(AppData *app, int visible_idx) {
    if (!app) return NULL;
    ProjectsMode *mode = &app->projects_mode;
    if (visible_idx < 0 || visible_idx >= mode->filtered_count) return NULL;
    if (mode->filtered_rows[visible_idx].type != PROJECT_ROW_FOLDER) return NULL;
    int raw = mode->filtered_rows[visible_idx].index;
    if (raw < 0 || raw >= mode->folder_count) return NULL;
    return &mode->folders[raw];
}

static CofiActionStatus attach_tmux_session(AppData *app, const char *session_name) {
    char err[128] = {0};
    gchar *tmux = projects_resolve_tool(&app->config, PROJECT_TOOL_TMUX,
                                        err, sizeof(err));
    if (!tmux) return COFI_ACTION_ERROR;

    if (projects_activate_tmux_window(app, tmux, session_name)) {
        g_free(tmux);
        return COFI_HANDLED_HIDE;
    }

    gchar *command = projects_build_tmux_attach_command(tmux, session_name);
    g_free(tmux);
    if (!command) return COFI_ACTION_ERROR;

    gchar *launch_command = projects_with_terminal_title(command, session_name);
    g_free(command);
    if (!launch_command) return COFI_ACTION_ERROR;

    gboolean ok = s_launch_in_terminal(launch_command);
    if (ok) {
        log_info("USER: tmux: attaching session '%s'", session_name);
    } else {
        log_warn("tmux: failed to launch session '%s'", session_name);
    }
    g_free(launch_command);
    return ok ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

static CofiActionStatus zellij_attach_session(AppData *app, const char *session_name) {
    char err[128] = {0};
    gchar *zellij = projects_resolve_tool(&app->config, PROJECT_TOOL_ZELLIJ,
                                          err, sizeof(err));
    if (!zellij) return COFI_ACTION_ERROR;

    if (projects_activate_zellij_window(app, zellij, session_name)) {
        g_free(zellij);
        return COFI_HANDLED_HIDE;
    }

    gchar *command = projects_build_zellij_attach_command(zellij, session_name);
    g_free(zellij);
    if (!command) return COFI_ACTION_ERROR;

    gchar *launch_command = projects_with_terminal_title(command, session_name);
    g_free(command);
    if (!launch_command) return COFI_ACTION_ERROR;

    gboolean ok = s_launch_in_terminal(launch_command);
    if (ok) {
        log_info("USER: zellij: attaching session '%s'", session_name);
    } else {
        log_warn("zellij: failed to launch session '%s'", session_name);
    }
    g_free(launch_command);
    return ok ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

static gboolean launch_folder_opener(const char *program, const char *arg, const char *path) {
    gchar *resolved = g_find_program_in_path(program);
    if (!resolved) return FALSE;
    gboolean ok = FALSE;
    if (arg) {
        const char *argv[] = {resolved, arg, path, NULL};
        ok = s_launch_argv(argv);
    } else {
        const char *argv[] = {resolved, path, NULL};
        ok = s_launch_argv(argv);
    }
    if (ok) log_info("USER: opened folder via %s: %s", program, path);
    else log_warn("Failed to open folder via %s: %s", program, path);
    g_free(resolved);
    return ok;
}

static gboolean launch_configured_folder_opener(AppData *app, const char *path) {
    const char *configured = app->config.projects_file_explorer_path;
    if (!configured || configured[0] == '\0') return FALSE;
    if (!g_file_test(configured, G_FILE_TEST_IS_REGULAR) ||
        !g_file_test(configured, G_FILE_TEST_IS_EXECUTABLE)) {
        log_warn("projects: configured file explorer is not executable: %s", configured);
        return FALSE;
    }
    const char *argv[] = {configured, path, NULL};
    gboolean ok = s_launch_argv(argv);
    if (ok) log_info("USER: opened folder via configured explorer: %s", path);
    else log_warn("Failed to open folder via configured explorer: %s", path);
    return ok;
}

static Window *get_stacking_order(Display *display, unsigned long *count) {
    if (count) *count = 0;
    if (!display) return NULL;

    Atom atom = XInternAtom(display, "_NET_CLIENT_LIST_STACKING", False);
    Atom actual_type;
    int actual_format;
    unsigned long n_items;
    unsigned long bytes_after;
    unsigned char *prop = NULL;

    if (XGetWindowProperty(display, DefaultRootWindow(display), atom,
                           0, 4096, False, XA_WINDOW,
                           &actual_type, &actual_format, &n_items, &bytes_after,
                           &prop) != Success || !prop) {
        return NULL;
    }

    if (count) *count = n_items;
    return (Window *)prop;
}

static gboolean activate_existing_caja_folder(AppData *app, const char *path) {
    if (!app || !app->display || !path || path[0] == '\0') return FALSE;

    get_window_list(app);

    unsigned long stack_count = 0;
    Window *stack = get_stacking_order(app->display, &stack_count);
    Window window = 0;
    gboolean found = projects_find_caja_folder_window(app->windows,
                                                      app->window_count,
                                                      stack,
                                                      stack_count,
                                                      path,
                                                      &window);
    if (stack) XFree(stack);
    if (!found) return FALSE;

    activate_window(app->display, window);
    log_info("USER: activated Caja folder window for %s", path);
    return TRUE;
}

CofiActionStatus projects_open_folder(AppData *app, const char *path) {
    projects_remote_scope_clear_status_message();
    if (!path || path[0] == '\0') return COFI_ACTION_ERROR;

    if (activate_existing_caja_folder(app, path)) {
        return COFI_HANDLED_HIDE;
    }

    if (launch_configured_folder_opener(app, path)) {
        return COFI_HANDLED_HIDE;
    }

    if (app->config.projects_file_explorer_path[0] == '\0' &&
        (launch_folder_opener("caja", NULL, path) ||
        launch_folder_opener("xdg-open", NULL, path) ||
        launch_folder_opener("gio", "open", path))) {
        return COFI_HANDLED_HIDE;
    }

    log_warn("No file manager opener found for folder: %s", path);
    return COFI_ACTION_ERROR;
}

CofiActionStatus projects_open_folder_terminal(AppData *app, const ProjectFolder *folder) {
    if (!app || !folder || !folder->path || folder->path[0] == '\0') return COFI_ACTION_ERROR;
    gchar *command = NULL;
    if (folder->is_remote) {
        command = projects_build_remote_folder_terminal_command(folder->remote_host, folder->path);
    } else {
        command = projects_build_folder_terminal_command(folder->path);
    }
    if (!command) return COFI_ACTION_ERROR;

    gchar *base = g_path_get_basename(folder->path);
    gchar *title = folder->is_remote
        ? g_strdup_printf("%s:%s", folder->remote_host[0] ? folder->remote_host : "?",
                          base && base[0] ? base : folder->path)
        : g_strdup(base && base[0] ? base : folder->path);
    gchar *launch_command = projects_with_terminal_title(command, title && title[0] ? title : folder->path);
    g_free(title);
    g_free(base);
    g_free(command);
    if (!launch_command) return COFI_ACTION_ERROR;

    gboolean ok = s_launch_in_terminal(launch_command);
    g_free(launch_command);
    return ok ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

static CofiActionStatus projects_open_remote_folder(AppData *app,
                                                    const char *host,
                                                    const char *path) {
    if (!host || !host[0] || !path || path[0] == '\0') return COFI_ACTION_ERROR;
    gchar *uri = g_strdup_printf("sftp://%s%s", host, path);
    CofiActionStatus status = projects_open_folder(app, uri);
    g_free(uri);
    return status;
}

CofiActionStatus projects_remove_folder_entry(AppData *app,
                                              const char *path,
                                              gboolean is_remote,
                                              const char *remote_host) {
    if (!app || !path || path[0] == '\0') return COFI_ACTION_ERROR;

    gchar *command = NULL;
    if (is_remote) {
        if (!remote_host || remote_host[0] == '\0') return COFI_ACTION_ERROR;
        gchar *quoted_host = g_shell_quote(remote_host);
        gchar *quoted_path = g_shell_quote(path);
        command = g_strdup_printf("ssh -o BatchMode=yes -o ConnectTimeout=4 %s zoxide remove %s",
                                  quoted_host, quoted_path);
        g_free(quoted_path);
        g_free(quoted_host);
    } else {
        char err[128] = {0};
        gchar *zoxide = projects_resolve_tool(&app->config, PROJECT_TOOL_ZOXIDE, err, sizeof(err));
        if (!zoxide) return COFI_ACTION_ERROR;
        gchar *quoted_tool = g_shell_quote(zoxide);
        gchar *quoted_path = g_shell_quote(path);
        command = g_strdup_printf("%s remove %s", quoted_tool, quoted_path);
        g_free(quoted_path);
        g_free(quoted_tool);
        g_free(zoxide);
    }

    gboolean ok = s_exec_command(command);
    g_free(command);
    return ok ? COFI_HANDLED_REFRESH : COFI_ACTION_ERROR;
}

static void add_filtered_row(ProjectsMode *mode, ProjectRowType type, int index) {
    int max_rows = MAX_PROJECTS + MAX_PROJECT_FOLDERS;
    if (!mode || mode->filtered_count >= max_rows) return;
    mode->filtered_rows[mode->filtered_count].type = type;
    mode->filtered_rows[mode->filtered_count].index = index;
    mode->filtered_count++;
}

typedef struct {
    ProjectRowType type;
    int index;
    int order;
    score_t score;
} SessionFilterHit;

static int compare_session_filter_hits(const void *a, const void *b) {
    const SessionFilterHit *ha = a;
    const SessionFilterHit *hb = b;
    if (ha->score != hb->score) {
        return hb->score - ha->score;
    }
    return ha->order - hb->order;
}

void projects_filter(AppData *app, const char *query) {
    if (!app) return;
    ProjectsMode *mode = &app->projects_mode;
    mode->filtered_count = 0;

    if (!query || query[0] == '\0') {
        for (int i = 0; i < mode->session_count && i < MAX_PROJECTS; i++) {
            add_filtered_row(mode, PROJECT_ROW_SESSION, i);
        }
        for (int i = 0; i < mode->folder_count && i < MAX_PROJECT_FOLDERS; i++) {
            add_filtered_row(mode, PROJECT_ROW_FOLDER, i);
        }
        return;
    }

    SessionFilterHit hits[MAX_PROJECTS + MAX_PROJECT_FOLDERS];
    int hit_count = 0;
    int order = 0;
    for (int i = 0; i < mode->session_count && i < MAX_PROJECTS; i++) {
        char match_text[384];
        projects_format_session_match_text(&mode->projects[i], match_text, sizeof(match_text));
        if (fzf_has_match(query, match_text)) {
            hits[hit_count++] = (SessionFilterHit){
                .type = PROJECT_ROW_SESSION,
                .index = i,
                .order = order,
                .score = fzf_fuzzy_match(query, match_text),
            };
        }
        order++;
    }
    for (int i = 0; i < mode->folder_count && i < MAX_PROJECT_FOLDERS; i++) {
        char match_text[512];
        projects_format_folder_match_text(&mode->folders[i], match_text, sizeof(match_text));
        if (fzf_has_match(query, match_text)) {
            hits[hit_count++] = (SessionFilterHit){
                .type = PROJECT_ROW_FOLDER,
                .index = i,
                .order = order,
                .score = fzf_fuzzy_match(query, match_text),
            };
        }
        order++;
    }

    if (hit_count > 1) {
        qsort(hits, (size_t)hit_count, sizeof(hits[0]), compare_session_filter_hits);
    }
    for (int i = 0; i < hit_count; i++) {
        add_filtered_row(mode, hits[i].type, hits[i].index);
    }
}

int projects_row_count(AppData *app) {
    if (!app) return 0;
    ProjectsMode *mode = &app->projects_mode;
    if (mode->filtered_count > 0) return mode->filtered_count;
    return 1;
}

void projects_format_row(AppData *app, int visible_idx, CofiRowCells *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!app) return;

    ProjectsMode *mode = &app->projects_mode;
    ProjectFolder *folder = folder_at_visible(app, visible_idx);
    static char windows_buf[16];
    static char attached_buf[16];
    static char remote_name_buf[640];
    static char remote_folder_label_buf[640];

    if (folder) {
        out->cell_count = 3;
        out->cells[0].text = projects_folder_marker();
        out->cells[0].width_hint = 3;
        if (folder->is_remote) {
            g_snprintf(remote_folder_label_buf, sizeof(remote_folder_label_buf),
                       "[REMOTE:%s] %s",
                       folder->remote_host[0] ? folder->remote_host : "?",
                       folder->label ? folder->label : "");
            out->cells[1].text = remote_folder_label_buf;
        } else {
            out->cells[1].text = folder->label;
        }
        out->cells[1].width_hint = 24;
        out->cells[2].text = folder->path;
        out->row_flags = COFI_ROW_ACTIONABLE | COFI_ROW_SLOTTABLE;
        return;
    }
    ProjectSessionEntry *session = session_at_visible(app, visible_idx);
    if (!session) {
        out->cell_count = 1;
        if (mode->session_count + mode->folder_count > 0) {
            out->cells[0].text = "No matching projects";
        } else if (mode->last_error[0] != '\0') {
            out->cells[0].text = mode->last_error;
            out->row_flags = COFI_ROW_ERROR;
        } else {
            out->cells[0].text = "No tmux/zellij sessions or zoxide folders";
        }
        return;
    }

    if (session->is_saved_remote) {
        g_snprintf(remote_name_buf, sizeof(remote_name_buf), "[REMOTE:%s] %s",
                   session->remote_host[0] ? session->remote_host : "?",
                   session->name);
        out->cell_count = 4;
        out->cells[0].text = projects_session_marker(session->backend);
        out->cells[0].width_hint = 3;
        out->cells[1].text = remote_name_buf;
        out->cells[2].text = "";
        out->cells[2].width_hint = 7;
        out->cells[3].text = "";
        out->cells[3].width_hint = 10;
        out->row_flags = COFI_ROW_ACTIONABLE | COFI_ROW_SLOTTABLE;
        return;
    }

    g_snprintf(windows_buf, sizeof(windows_buf), "%d %s",
               session->windows, session->windows == 1 ? "win" : "wins");
    g_snprintf(attached_buf, sizeof(attached_buf), "%d %s",
               session->attached, session->attached == 1 ? "client" : "clients");

    out->cell_count = 4;
    out->cells[0].text = projects_session_marker(session->backend);
    out->cells[0].width_hint = 3;
    out->cells[1].text = session->name;
    if (session->backend == PROJECT_BACKEND_ZELLIJ) {
        out->cells[2].text = "";
        out->cells[2].width_hint = 7;
        out->cells[3].text = "";
        out->cells[3].width_hint = 10;
    } else {
        out->cells[2].text = windows_buf;
        out->cells[2].width_hint = 7;
        out->cells[2].align = 1;
        out->cells[3].text = attached_buf;
        out->cells[3].width_hint = 10;
        out->cells[3].align = 1;
    }
    out->row_flags = COFI_ROW_ACTIONABLE | COFI_ROW_SLOTTABLE;
}

const char *projects_match_string(AppData *app, int visible_idx) {
    ProjectSessionEntry *session = session_at_visible(app, visible_idx);
    ProjectFolder *folder = folder_at_visible(app, visible_idx);
    static char match_text[512];
    if (folder) {
        projects_format_folder_match_text(folder, match_text, sizeof(match_text));
        return match_text;
    }
    if (session) {
        projects_format_session_match_text(session, match_text, sizeof(match_text));
        return match_text;
    }
    return "";
}

const char *projects_row_identity(AppData *app, int visible_idx) {
    ProjectSessionEntry *session = session_at_visible(app, visible_idx);
    ProjectFolder *folder = folder_at_visible(app, visible_idx);
    if (folder) return folder->path;
    return session ? session->name : "";
}

void projects_on_enter(AppData *app) {
    if (!app) return;
    if (app->entry) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "projects...");
    }
    projects_refresh(app);
}

void projects_on_query_changed(AppData *app, const char *query) {
    if (query && query[0] != '\0') {
        projects_remote_scope_clear_status_message();
    }
    projects_filter(app, query);
    reset_selection(app);
}

void projects_on_leave(AppData *app) {
    (void)app;
    projects_remote_scope_clear_status_message();
    projects_remote_scope_clear();
}

void projects_on_tick(AppData *app, int generation) {
    (void)generation;
    if (!app) return;
    ProjectRowType selected_type = PROJECT_ROW_SESSION;
    ProjectBackend selected_backend = PROJECT_BACKEND_TMUX;
    gchar *selected_identity = NULL;
    ProjectSessionEntry *selected_session = session_at_visible(app, app->selection.provider_index);
    ProjectFolder *selected_folder = folder_at_visible(app, app->selection.provider_index);
    if (selected_session) {
        selected_type = PROJECT_ROW_SESSION;
        selected_backend = selected_session->backend;
        selected_identity = g_strdup(selected_session->name);
    } else if (selected_folder) {
        selected_type = PROJECT_ROW_FOLDER;
        selected_identity = g_strdup(selected_folder->path);
    }

    projects_refresh(app);
    app->selection.provider_index = 0;
    if (selected_identity && selected_identity[0] != '\0') {
        for (int i = 0; i < app->projects_mode.filtered_count; i++) {
            ProjectRowRef row = app->projects_mode.filtered_rows[i];
            const char *identity = row.type == PROJECT_ROW_SESSION
                ? app->projects_mode.projects[row.index].name
                : app->projects_mode.folders[row.index].path;
            gboolean same_backend = row.type != PROJECT_ROW_SESSION ||
                app->projects_mode.projects[row.index].backend == selected_backend;
            if (row.type == selected_type && same_backend &&
                strcmp(identity, selected_identity) == 0) {
                app->selection.provider_index = i;
                break;
            }
        }
    }
    g_free(selected_identity);
    update_scroll_position(app);
    update_display(app);
}

CofiActionStatus projects_attach_visible(AppData *app, int visible_idx) {
    ProjectSessionEntry *session = session_at_visible(app, visible_idx);
    if (session) {
        if (session->is_saved_remote) {
            return launch_remote_session(app,
                                         session->remote_host,
                                         session->backend,
                                         session->name,
                                         session->remote_cwd,
                                         FALSE);
        }
        return session->backend == PROJECT_BACKEND_ZELLIJ
            ? zellij_attach_session(app, session->name)
            : attach_tmux_session(app, session->name);
    }
    ProjectFolder *folder = folder_at_visible(app, visible_idx);
    if (folder && folder->is_remote) {
        return projects_open_remote_folder(app, folder->remote_host, folder->path);
    }
    return folder ? projects_open_folder(app, folder->path) : COFI_ACTION_ERROR;
}

static CofiActionStatus run_session_admin_command(const char *command) {
    if (!command) return COFI_ACTION_ERROR;
    gboolean ok = s_run_session_command(command);
    return ok ? COFI_HANDLED_REFRESH : COFI_ACTION_ERROR;
}

CofiActionStatus projects_kill_session(AppData *app,
                                   const char *session_name,
                                   ProjectBackend backend) {
    if (!session_name || session_name[0] == '\0') return COFI_ACTION_ERROR;

    ProjectTool tool = backend == PROJECT_BACKEND_ZELLIJ ? PROJECT_TOOL_ZELLIJ : PROJECT_TOOL_TMUX;
    gchar *program = projects_resolve_tool(&app->config, tool, NULL, 0);
    if (!program) return COFI_ACTION_ERROR;
    gchar *command = backend == PROJECT_BACKEND_ZELLIJ
        ? projects_build_zellij_kill_command(program, session_name)
        : projects_build_tmux_kill_command(program, session_name);
    g_free(program);
    CofiActionStatus status = run_session_admin_command(command);
    if (status == COFI_HANDLED_REFRESH) {
        log_info("USER: %s: killed session '%s'",
                 backend == PROJECT_BACKEND_ZELLIJ ? "zellij" : "tmux",
                 session_name);
    }
    g_free(command);
    return status;
}

CofiActionStatus projects_rename_tmux_session(AppData *app, const char *old_name, const char *new_name) {
    gchar *tmux = projects_resolve_tool(&app->config, PROJECT_TOOL_TMUX, NULL, 0);
    if (!tmux) return COFI_ACTION_ERROR;
    gchar *command = projects_build_tmux_rename_command(tmux, old_name, new_name);
    g_free(tmux);
    CofiActionStatus status = run_session_admin_command(command);
    if (status == COFI_HANDLED_REFRESH) {
        log_info("USER: tmux: renamed session '%s' to '%s'", old_name, new_name);
    }
    g_free(command);
    return status;
}

CofiActionStatus projects_new_session(AppData *app,
                                       const char *session_name,
                                       ProjectBackend backend,
                                       const char *start_dir) {
    if (projects_remote_scope_is_active()) {
        const char *host = projects_remote_scope_current_host();
        if (host && host[0]) {
            CofiActionStatus remote_status = launch_remote_session(app, host, backend, session_name, start_dir, TRUE);
            return remote_status;
        }
    }

    const char *home = g_get_home_dir();
    const char *dir = (start_dir && start_dir[0] != '\0') ? start_dir : (home ? home : "/");
    ProjectTool tool = backend == PROJECT_BACKEND_ZELLIJ ? PROJECT_TOOL_ZELLIJ : PROJECT_TOOL_TMUX;
    gchar *program = projects_resolve_tool(&app->config, tool, NULL, 0);
    if (!program) return COFI_ACTION_ERROR;
    gchar *command = backend == PROJECT_BACKEND_ZELLIJ
        ? projects_build_zellij_new_command(program, session_name, dir)
        : projects_build_tmux_new_command(program, session_name, dir);
    g_free(program);
    if (!command) return COFI_ACTION_ERROR;

    gchar *launch_command = projects_with_terminal_title(command, session_name);
    g_free(command);
    if (!launch_command) return COFI_ACTION_ERROR;

    gboolean ok = s_launch_in_terminal(launch_command);
    if (ok) {
        log_info("USER: %s: created/attached session '%s'",
                 backend == PROJECT_BACKEND_ZELLIJ ? "zellij" : "tmux",
                 session_name);
    } else {
        log_warn("%s: failed to create/attach session '%s'",
                 backend == PROJECT_BACKEND_ZELLIJ ? "zellij" : "tmux",
                 session_name);
    }
    g_free(launch_command);
    return ok ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

gboolean projects_forget_selected_remote(AppData *app) {
    if (!app) return FALSE;
    ProjectSessionEntry *session = projects_selected_session(app);
    if (!session || !session->is_saved_remote) return FALSE;

    return projects_forget_remote_entry(session->remote_host,
                                        session->backend,
                                        session->name,
                                        session->remote_cwd);
}

gboolean projects_forget_remote_entry(const char *host,
                                      ProjectBackend backend,
                                      const char *name,
                                      const char *cwd) {
    if (!host || !host[0] || !name || name[0] == '\0') return FALSE;
    return projects_remote_store_forget(host, backend, name, cwd);
}

ProjectSessionEntry *projects_selected_session(AppData *app) {
    if (!app) return NULL;
    return session_at_visible(app, app->selection.provider_index);
}

ProjectFolder *projects_selected_folder(AppData *app) {
    if (!app) return NULL;
    return folder_at_visible(app, app->selection.provider_index);
}

ProjectFolder *projects_folder_at_visible(AppData *app, int visible_idx) {
    return folder_at_visible(app, visible_idx);
}

const char *projects_get_shortcut_hint(AppData *app) {
    /* Ctrl+T (terminal here) applies to folder rows only. */
    if (projects_selected_folder(app)) {
        return "Shortcuts: Enter=Open  Ctrl+N=New  Ctrl+S=Remote  Ctrl+T=Terminal  Ctrl+D/Delete=Delete";
    }
    /* Ctrl+R rename applies only to tmux session rows. */
    ProjectSessionEntry *session = projects_selected_session(app);
    if (session && session->backend == PROJECT_BACKEND_TMUX) {
        return "Shortcuts: Enter=Open  Ctrl+N=New  Ctrl+S=Remote  Ctrl+R=Rename  Ctrl+D/Delete=Delete";
    }
    return "Shortcuts: Enter=Open  Ctrl+N=New  Ctrl+S=Remote  Ctrl+D/Delete=Delete";
}

const char *projects_slot_payload_for(AppData *app, int visible_idx) {
    static char payload_buf[1024];
    payload_buf[0] = '\0';
    ProjectSessionEntry *session = session_at_visible(app, visible_idx);
    ProjectFolder *folder = folder_at_visible(app, visible_idx);
    gchar *payload = NULL;

    if (session) {
        payload = projects_build_session_slot_payload(session->backend, session->name);
    } else if (folder) {
        payload = projects_build_folder_slot_payload(folder->path);
    }
    if (!payload) return NULL;

    g_strlcpy(payload_buf, payload, sizeof(payload_buf));
    g_free(payload);
    return payload_buf;
}

CofiActionStatus projects_slot_recall(AppData *app, const char *payload) {
    ProjectSlotTarget target;
    if (!projects_parse_slot_payload(payload, &target)) {
        log_warn("Projects slot has invalid payload: %s", payload ? payload : "(null)");
        return COFI_ACTION_ERROR;
    }

    if (target.kind == PROJECT_SLOT_FOLDER) {
        return projects_open_folder(app, target.value);
    }
    if (target.kind == PROJECT_SLOT_SESSION) {
        return target.backend == PROJECT_BACKEND_ZELLIJ
            ? zellij_attach_session(app, target.value)
            : attach_tmux_session(app, target.value);
    }
    return COFI_ACTION_ERROR;
}

CofiActionStatus projects_attach_named(AppData *app, const char *name) {
    if (!app || !name || name[0] == '\0') return COFI_NO_OP;
    for (int i = 0; i < app->projects_mode.session_count; i++) {
        if (strcmp(app->projects_mode.projects[i].name, name) == 0) {
            return app->projects_mode.projects[i].backend == PROJECT_BACKEND_ZELLIJ
                ? zellij_attach_session(app, name)
                : attach_tmux_session(app, name);
        }
    }
    return COFI_ACTION_ERROR;
}

gboolean projects_has_named(AppData *app, const char *name) {
    if (!app || !name || name[0] == '\0') return FALSE;
    for (int i = 0; i < app->projects_mode.session_count; i++) {
        if (strcmp(app->projects_mode.projects[i].name, name) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

#ifdef COFI_TESTING
void projects_set_launch_impl_test_hook(gboolean (*impl)(const char *command)) {
    s_launch_in_terminal = impl ? impl : default_launch_in_terminal;
}

void projects_set_command_impl_test_hook(gboolean (*impl)(const char *command)) {
    s_run_session_command = impl ? impl : default_run_session_command;
}

void projects_set_argv_launch_impl_test_hook(gboolean (*impl)(const char *const *argv)) {
    s_launch_argv = impl ? impl : default_launch_argv;
}

void projects_set_exec_impl_test_hook(gboolean (*impl)(const char *command)) {
    s_exec_command = impl ? impl : default_exec_command;
}

#endif
