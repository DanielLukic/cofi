#include "sessions.h"

#include <stdlib.h>
#include <string.h>
#include "app_data.h"
#include "detach_launch.h"
#include "display.h"
#include "fzf_algo.h"
#include "log.h"
#include "selection.h"
#include "sessions_commands.h"
#include "sessions_parse.h"

#include <gtk/gtk.h>
static gboolean default_launch_in_terminal(const char *command) {
    return detach_launch_in_terminal_cmd(command);
}

static gboolean (*s_launch_in_terminal)(const char *command) = default_launch_in_terminal;

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

static SessionEntry *session_at_visible(AppData *app, int visible_idx) {
    if (!app) return NULL;
    SessionsMode *mode = &app->sessions_mode;
    if (visible_idx < 0 || visible_idx >= mode->filtered_count) return NULL;
    if (mode->filtered_rows[visible_idx].type != SESSION_ROW_SESSION) return NULL;
    int raw = mode->filtered_rows[visible_idx].index;
    if (raw < 0 || raw >= mode->session_count) return NULL;
    return &mode->sessions[raw];
}

static SessionFolder *folder_at_visible(AppData *app, int visible_idx) {
    if (!app) return NULL;
    SessionsMode *mode = &app->sessions_mode;
    if (visible_idx < 0 || visible_idx >= mode->filtered_count) return NULL;
    if (mode->filtered_rows[visible_idx].type != SESSION_ROW_FOLDER) return NULL;
    int raw = mode->filtered_rows[visible_idx].index;
    if (raw < 0 || raw >= mode->folder_count) return NULL;
    return &mode->folders[raw];
}

static CofiActionStatus attach_tmux_session(AppData *app, const char *session_name) {
    (void)app;
    gchar *command = sessions_build_tmux_attach_command(session_name);
    if (!command) return COFI_ACTION_ERROR;

    gboolean ok = s_launch_in_terminal(command);
    if (ok) {
        log_info("USER: tmux: attaching session '%s'", session_name);
    } else {
        log_warn("tmux: failed to launch session '%s'", session_name);
    }
    g_free(command);
    return ok ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

static CofiActionStatus zellij_attach_session(AppData *app, const char *session_name) {
    (void)app;
    gchar *command = sessions_build_zellij_attach_command(session_name);
    if (!command) return COFI_ACTION_ERROR;

    gboolean ok = s_launch_in_terminal(command);
    if (ok) {
        log_info("USER: zellij: attaching session '%s'", session_name);
    } else {
        log_warn("zellij: failed to launch session '%s'", session_name);
    }
    g_free(command);
    return ok ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

static CofiActionStatus open_folder_session(AppData *app, const char *path) {
    (void)app;
    if (!path || path[0] == '\0') return COFI_ACTION_ERROR;
    gchar *session_name = sessions_build_folder_session_name(path);
    gchar *command = sessions_build_tmux_new_command(session_name, path);
    g_free(session_name);
    if (!command) return COFI_ACTION_ERROR;

    gboolean ok = s_launch_in_terminal(command);
    if (ok) {
        log_info("USER: tmux: opening folder '%s'", path);
    } else {
        log_warn("tmux: failed to open folder '%s'", path);
    }
    g_free(command);
    return ok ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

static void add_filtered_row(SessionsMode *mode, SessionRowType type, int index) {
    int max_rows = MAX_SESSIONS + MAX_SESSION_FOLDERS;
    if (!mode || mode->filtered_count >= max_rows) return;
    mode->filtered_rows[mode->filtered_count].type = type;
    mode->filtered_rows[mode->filtered_count].index = index;
    mode->filtered_count++;
}

typedef struct {
    SessionRowType type;
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

void sessions_filter(AppData *app, const char *query) {
    if (!app) return;
    SessionsMode *mode = &app->sessions_mode;
    mode->filtered_count = 0;

    if (!query || query[0] == '\0') {
        for (int i = 0; i < mode->session_count && i < MAX_SESSIONS; i++) {
            add_filtered_row(mode, SESSION_ROW_SESSION, i);
        }
        for (int i = 0; i < mode->folder_count && i < MAX_SESSION_FOLDERS; i++) {
            add_filtered_row(mode, SESSION_ROW_FOLDER, i);
        }
        return;
    }

    SessionFilterHit hits[MAX_SESSIONS + MAX_SESSION_FOLDERS];
    int hit_count = 0;
    int order = 0;
    for (int i = 0; i < mode->session_count && i < MAX_SESSIONS; i++) {
        char match_text[384];
        sessions_format_session_match_text(&mode->sessions[i], match_text, sizeof(match_text));
        if (fzf_has_match(query, match_text)) {
            hits[hit_count++] = (SessionFilterHit){
                .type = SESSION_ROW_SESSION,
                .index = i,
                .order = order,
                .score = fzf_fuzzy_match(query, match_text),
            };
        }
        order++;
    }
    for (int i = 0; i < mode->folder_count && i < MAX_SESSION_FOLDERS; i++) {
        char match_text[512];
        sessions_format_folder_match_text(&mode->folders[i], match_text, sizeof(match_text));
        if (fzf_has_match(query, match_text)) {
            hits[hit_count++] = (SessionFilterHit){
                .type = SESSION_ROW_FOLDER,
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

int sessions_row_count(AppData *app) {
    if (!app) return 0;
    SessionsMode *mode = &app->sessions_mode;
    if (mode->filtered_count > 0) return mode->filtered_count;
    return 1;
}

void sessions_format_row(AppData *app, int visible_idx, CofiRowCells *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!app) return;

    SessionsMode *mode = &app->sessions_mode;
    SessionFolder *folder = folder_at_visible(app, visible_idx);
    static char windows_buf[16];
    static char attached_buf[16];

    if (folder) {
        out->cell_count = 3;
        out->cells[0].text = sessions_folder_marker();
        out->cells[0].width_hint = 3;
        out->cells[1].text = folder->label;
        out->cells[1].width_hint = 24;
        out->cells[2].text = folder->path;
        out->row_flags = COFI_ROW_ACTIONABLE;
        return;
    }
    SessionEntry *session = session_at_visible(app, visible_idx);
    if (!session) {
        out->cell_count = 1;
        if (mode->session_count + mode->folder_count > 0) {
            out->cells[0].text = "No matching sessions";
        } else if (mode->last_error[0] != '\0') {
            out->cells[0].text = mode->last_error;
            out->row_flags = COFI_ROW_ERROR;
        } else {
            out->cells[0].text = "No tmux/zellij sessions or zoxide folders";
        }
        return;
    }

    g_snprintf(windows_buf, sizeof(windows_buf), "%d %s",
               session->windows, session->windows == 1 ? "win" : "wins");
    g_snprintf(attached_buf, sizeof(attached_buf), "%d %s",
               session->attached, session->attached == 1 ? "client" : "clients");

    out->cell_count = 4;
    out->cells[0].text = sessions_session_marker(session->backend);
    out->cells[0].width_hint = 3;
    out->cells[1].text = session->name;
    if (session->backend == SESSION_BACKEND_ZELLIJ) {
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
    out->row_flags = COFI_ROW_ACTIONABLE;
}

const char *sessions_match_string(AppData *app, int visible_idx) {
    SessionEntry *session = session_at_visible(app, visible_idx);
    SessionFolder *folder = folder_at_visible(app, visible_idx);
    static char match_text[512];
    if (folder) {
        sessions_format_folder_match_text(folder, match_text, sizeof(match_text));
        return match_text;
    }
    if (session) {
        sessions_format_session_match_text(session, match_text, sizeof(match_text));
        return match_text;
    }
    return "";
}

const char *sessions_row_identity(AppData *app, int visible_idx) {
    SessionEntry *session = session_at_visible(app, visible_idx);
    SessionFolder *folder = folder_at_visible(app, visible_idx);
    if (folder) return folder->path;
    return session ? session->name : "";
}

void sessions_on_enter(AppData *app) {
    if (!app) return;
    if (app->entry) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "sessions...");
    }
    sessions_refresh(app);
}

void sessions_on_query_changed(AppData *app, const char *query) {
    sessions_filter(app, query);
    reset_selection(app);
}

void sessions_on_tick(AppData *app, int generation) {
    (void)generation;
    if (!app) return;
    SessionRowType selected_type = SESSION_ROW_SESSION;
    SessionBackend selected_backend = SESSION_BACKEND_TMUX;
    gchar *selected_identity = NULL;
    SessionEntry *selected_session = session_at_visible(app, app->selection.provider_index);
    SessionFolder *selected_folder = folder_at_visible(app, app->selection.provider_index);
    if (selected_session) {
        selected_type = SESSION_ROW_SESSION;
        selected_backend = selected_session->backend;
        selected_identity = g_strdup(selected_session->name);
    } else if (selected_folder) {
        selected_type = SESSION_ROW_FOLDER;
        selected_identity = g_strdup(selected_folder->path);
    }

    sessions_refresh(app);
    app->selection.provider_index = 0;
    if (selected_identity && selected_identity[0] != '\0') {
        for (int i = 0; i < app->sessions_mode.filtered_count; i++) {
            SessionRowRef row = app->sessions_mode.filtered_rows[i];
            const char *identity = row.type == SESSION_ROW_SESSION
                ? app->sessions_mode.sessions[row.index].name
                : app->sessions_mode.folders[row.index].path;
            gboolean same_backend = row.type != SESSION_ROW_SESSION ||
                app->sessions_mode.sessions[row.index].backend == selected_backend;
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

CofiActionStatus sessions_attach_visible(AppData *app, int visible_idx) {
    SessionEntry *session = session_at_visible(app, visible_idx);
    if (session) {
        return session->backend == SESSION_BACKEND_ZELLIJ
            ? zellij_attach_session(app, session->name)
            : attach_tmux_session(app, session->name);
    }
    SessionFolder *folder = folder_at_visible(app, visible_idx);
    return folder ? open_folder_session(app, folder->path) : COFI_ACTION_ERROR;
}

static CofiActionStatus run_session_admin_command(const char *command) {
    if (!command) return COFI_ACTION_ERROR;
    gboolean ok = s_run_session_command(command);
    return ok ? COFI_HANDLED_REFRESH : COFI_ACTION_ERROR;
}

CofiActionStatus sessions_kill_session(AppData *app,
                                   const char *session_name,
                                   SessionBackend backend) {
    (void)app;
    if (!session_name || session_name[0] == '\0') return COFI_ACTION_ERROR;

    gchar *command = backend == SESSION_BACKEND_ZELLIJ
        ? sessions_build_zellij_kill_command(session_name)
        : sessions_build_tmux_kill_command(session_name);
    CofiActionStatus status = run_session_admin_command(command);
    if (status == COFI_HANDLED_REFRESH) {
        log_info("USER: %s: killed session '%s'",
                 backend == SESSION_BACKEND_ZELLIJ ? "zellij" : "tmux",
                 session_name);
    }
    g_free(command);
    return status;
}

CofiActionStatus sessions_rename_tmux_session(AppData *app, const char *old_name, const char *new_name) {
    (void)app;
    gchar *command = sessions_build_tmux_rename_command(old_name, new_name);
    CofiActionStatus status = run_session_admin_command(command);
    if (status == COFI_HANDLED_REFRESH) {
        log_info("USER: tmux: renamed session '%s' to '%s'", old_name, new_name);
    }
    g_free(command);
    return status;
}

CofiActionStatus sessions_new_tmux_session(AppData *app, const char *session_name) {
    (void)app;
    const char *home = g_get_home_dir();
    gchar *command = sessions_build_tmux_new_command(session_name, home ? home : "/");
    if (!command) return COFI_ACTION_ERROR;

    gboolean ok = s_launch_in_terminal(command);
    if (ok) {
        log_info("USER: tmux: created/attached session '%s'", session_name);
    } else {
        log_warn("tmux: failed to create/attach session '%s'", session_name);
    }
    g_free(command);
    return ok ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

SessionEntry *sessions_selected_session(AppData *app) {
    if (!app) return NULL;
    return session_at_visible(app, app->selection.provider_index);
}

CofiActionStatus sessions_attach_named(AppData *app, const char *name) {
    if (!app || !name || name[0] == '\0') return COFI_NO_OP;
    for (int i = 0; i < app->sessions_mode.session_count; i++) {
        if (strcmp(app->sessions_mode.sessions[i].name, name) == 0) {
            return app->sessions_mode.sessions[i].backend == SESSION_BACKEND_ZELLIJ
                ? zellij_attach_session(app, name)
                : attach_tmux_session(app, name);
        }
    }
    return COFI_ACTION_ERROR;
}

#ifdef COFI_TESTING
void sessions_set_launch_impl_test_hook(gboolean (*impl)(const char *command)) {
    s_launch_in_terminal = impl ? impl : default_launch_in_terminal;
}

void sessions_set_command_impl_test_hook(gboolean (*impl)(const char *command)) {
    s_run_session_command = impl ? impl : default_run_session_command;
}
#endif
