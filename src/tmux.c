#include "tmux.h"

#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#ifndef COFI_TMUX_PARSER_TEST
#include "app_data.h"
#include "detach_launch.h"
#include "display.h"
#include "log.h"
#include "selection.h"

#include <gtk/gtk.h>
#endif

static gboolean parse_int_field(const char *text, int *out) {
    if (!text || !out || text[0] == '\0') return FALSE;
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (*end != '\0' || value < 0 || value > 9999) return FALSE;
    *out = (int)value;
    return TRUE;
}

static void copy_field(char *dest, size_t dest_size, const char *start, size_t len) {
    if (!dest || dest_size == 0) return;
    if (len >= dest_size) len = dest_size - 1;
    memcpy(dest, start, len);
    dest[len] = '\0';
}

static int parse_tmux_session_list(const char *output,
                                   TmuxSession *out,
                                   int max_out,
                                   char *error_out,
                                   size_t error_size) {
    if (error_out && error_size > 0) error_out[0] = '\0';
    if (!out || max_out <= 0) return 0;
    if (!output || output[0] == '\0') {
        if (error_out && error_size > 0)
            g_strlcpy(error_out, "No tmux sessions", error_size);
        return 0;
    }

    int count = 0;
    const char *line = output;
    while (*line && count < max_out) {
        const char *line_end = strchr(line, '\n');
        if (!line_end) line_end = line + strlen(line);
        if (line_end > line) {
            const char *tab1 = memchr(line, '\t', (size_t)(line_end - line));
            const char *tab2 = tab1 ? memchr(tab1 + 1, '\t', (size_t)(line_end - tab1 - 1)) : NULL;
            if (tab1 && tab2) {
                int windows = 0;
                int attached = 0;
                char windows_buf[32];
                char attached_buf[32];
                copy_field(windows_buf, sizeof(windows_buf), tab1 + 1, (size_t)(tab2 - tab1 - 1));
                copy_field(attached_buf, sizeof(attached_buf), tab2 + 1,
                           (size_t)(line_end - tab2 - 1));
                if (tab1 > line &&
                    parse_int_field(windows_buf, &windows) &&
                    parse_int_field(attached_buf, &attached)) {
                    copy_field(out[count].name, sizeof(out[count].name), line,
                               (size_t)(tab1 - line));
                    out[count].windows = windows;
                    out[count].attached = attached;
                    count++;
                }
            }
        }
        line = (*line_end == '\n') ? line_end + 1 : line_end;
    }

    if (count == 0 && error_out && error_size > 0) {
        g_strlcpy(error_out, "No tmux sessions", error_size);
    }
    return count;
}

static const char *path_basename(const char *path) {
    if (!path || path[0] == '\0') return "";
    const char *end = path + strlen(path);
    while (end > path && *(end - 1) == '/') end--;
    if (end == path) return path;

    const char *slash = end;
    while (slash > path && *(slash - 1) != '/') slash--;
    return slash;
}

static gchar *folder_label_from_path(const char *path) {
    const char *base = path_basename(path);
    size_t len = strlen(base);
    while (len > 0 && base[len - 1] == '/') len--;
    return len > 0 ? g_strndup(base, len) : g_strdup("zoxide");
}

#ifndef COFI_TMUX_PARSER_TEST
static void clear_zoxide_folders(TmuxFolder *folders, int count) {
    if (!folders || count <= 0) return;
    for (int i = 0; i < count; i++) {
        g_clear_pointer(&folders[i].path, g_free);
        g_clear_pointer(&folders[i].label, g_free);
    }
}
#endif

static int parse_zoxide_folder_list(const char *output,
                                    TmuxFolder *out,
                                    int max_out,
                                    char *error_out,
                                    size_t error_size) {
    if (error_out && error_size > 0) error_out[0] = '\0';
    if (!out || max_out <= 0) return 0;
    if (!output || output[0] == '\0') {
        if (error_out && error_size > 0)
            g_strlcpy(error_out, "No zoxide folders", error_size);
        return 0;
    }

    int count = 0;
    const char *line = output;
    while (*line && count < max_out) {
        const char *line_end = strchr(line, '\n');
        if (!line_end) line_end = line + strlen(line);
        if (line_end > line) {
            out[count].path = g_strndup(line, (size_t)(line_end - line));
            out[count].label = folder_label_from_path(out[count].path);
            if (out[count].path[0] != '\0' && out[count].label[0] != '\0') {
                count++;
            } else {
                g_clear_pointer(&out[count].path, g_free);
                g_clear_pointer(&out[count].label, g_free);
            }
        }
        line = (*line_end == '\n') ? line_end + 1 : line_end;
    }

    if (count == 0 && error_out && error_size > 0) {
        g_strlcpy(error_out, "No zoxide folders", error_size);
    }
    return count;
}

static gchar *build_folder_session_name(const char *path) {
    gchar *label = folder_label_from_path(path);

    GString *name = g_string_new(NULL);
    for (const char *p = label; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-') {
            g_string_append_c(name, (char)c);
        } else {
            g_string_append_c(name, '_');
        }
    }
    if (name->len == 0) {
        g_string_append(name, "zoxide");
    }
    g_free(label);
    return g_string_free(name, FALSE);
}

gchar *tmux_build_attach_command(const char *session_name) {
    if (!session_name || session_name[0] == '\0') return NULL;
    gchar *target = g_strconcat("=", session_name, NULL);
    gchar *quoted_target = g_shell_quote(target);
    gchar *command = g_strdup_printf("tmux attach-session -t %s", quoted_target);
    g_free(quoted_target);
    g_free(target);
    return command;
}

gchar *tmux_build_folder_session_command(const char *path) {
    if (!path || path[0] == '\0') return NULL;
    gchar *session_name = build_folder_session_name(path);
    gchar *quoted_session = g_shell_quote(session_name);
    gchar *quoted_path = g_shell_quote(path);
    gchar *command = g_strdup_printf("tmux new-session -A -s %s -c %s",
                                     quoted_session, quoted_path);
    g_free(quoted_path);
    g_free(quoted_session);
    g_free(session_name);
    return command;
}

gchar *tmux_build_kill_command(const char *session_name) {
    if (!session_name || session_name[0] == '\0') return NULL;
    gchar *target = g_strconcat("=", session_name, NULL);
    gchar *quoted_target = g_shell_quote(target);
    gchar *command = g_strdup_printf("tmux kill-session -t %s", quoted_target);
    g_free(quoted_target);
    g_free(target);
    return command;
}

gchar *tmux_build_rename_command(const char *old_name, const char *new_name) {
    if (!old_name || old_name[0] == '\0' || !new_name || new_name[0] == '\0') return NULL;
    gchar *target = g_strconcat("=", old_name, NULL);
    gchar *quoted_target = g_shell_quote(target);
    gchar *quoted_new_name = g_shell_quote(new_name);
    gchar *command = g_strdup_printf("tmux rename-session -t %s %s",
                                     quoted_target, quoted_new_name);
    g_free(quoted_new_name);
    g_free(quoted_target);
    g_free(target);
    return command;
}

gchar *tmux_build_new_session_command(const char *session_name, const char *start_dir) {
    if (!session_name || session_name[0] == '\0' || !start_dir || start_dir[0] == '\0') return NULL;
    gchar *quoted_name = g_shell_quote(session_name);
    gchar *quoted_dir = g_shell_quote(start_dir);
    gchar *command = g_strdup_printf("tmux new-session -A -s %s -c %s",
                                     quoted_name, quoted_dir);
    g_free(quoted_dir);
    g_free(quoted_name);
    return command;
}

#ifndef COFI_TMUX_PARSER_TEST
static gboolean default_launch_in_terminal(const char *command) {
    return detach_launch_in_terminal_cmd(command);
}

static gboolean (*s_launch_in_terminal)(const char *command) = default_launch_in_terminal;

static gboolean default_run_tmux_command(const char *command) {
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

static gboolean (*s_run_tmux_command)(const char *command) = default_run_tmux_command;

static TmuxSession *tmux_session_at_visible(AppData *app, int visible_idx) {
    if (!app) return NULL;
    TmuxMode *mode = &app->tmux_mode;
    if (visible_idx < 0 || visible_idx >= mode->filtered_count) return NULL;
    if (mode->filtered_rows[visible_idx].type != TMUX_ROW_SESSION) return NULL;
    int raw = mode->filtered_rows[visible_idx].index;
    if (raw < 0 || raw >= mode->session_count) return NULL;
    return &mode->sessions[raw];
}

static TmuxFolder *tmux_folder_at_visible(AppData *app, int visible_idx) {
    if (!app) return NULL;
    TmuxMode *mode = &app->tmux_mode;
    if (visible_idx < 0 || visible_idx >= mode->filtered_count) return NULL;
    if (mode->filtered_rows[visible_idx].type != TMUX_ROW_FOLDER) return NULL;
    int raw = mode->filtered_rows[visible_idx].index;
    if (raw < 0 || raw >= mode->folder_count) return NULL;
    return &mode->folders[raw];
}

static CofiActionStatus tmux_attach_session(AppData *app, const char *session_name) {
    (void)app;
    gchar *command = tmux_build_attach_command(session_name);
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

static CofiActionStatus tmux_open_folder(AppData *app, const char *path) {
    (void)app;
    gchar *command = tmux_build_folder_session_command(path);
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

static void tmux_add_filtered_row(TmuxMode *mode, TmuxRowType type, int index) {
    int max_rows = MAX_TMUX_SESSIONS + MAX_TMUX_FOLDERS;
    if (!mode || mode->filtered_count >= max_rows) return;
    mode->filtered_rows[mode->filtered_count].type = type;
    mode->filtered_rows[mode->filtered_count].index = index;
    mode->filtered_count++;
}

void tmux_filter(AppData *app, const char *query) {
    if (!app) return;
    TmuxMode *mode = &app->tmux_mode;
    mode->filtered_count = 0;

    if (!query || query[0] == '\0') {
        for (int i = 0; i < mode->session_count && i < MAX_TMUX_SESSIONS; i++) {
            tmux_add_filtered_row(mode, TMUX_ROW_SESSION, i);
        }
        for (int i = 0; i < mode->folder_count && i < MAX_TMUX_FOLDERS; i++) {
            tmux_add_filtered_row(mode, TMUX_ROW_FOLDER, i);
        }
        return;
    }

    gchar *query_lower = g_utf8_strdown(query, -1);
    for (int i = 0; i < mode->session_count && i < MAX_TMUX_SESSIONS; i++) {
        gchar *name_lower = g_utf8_strdown(mode->sessions[i].name, -1);
        if (g_strrstr(name_lower, query_lower)) {
            tmux_add_filtered_row(mode, TMUX_ROW_SESSION, i);
        }
        g_free(name_lower);
    }
    for (int i = 0; i < mode->folder_count && i < MAX_TMUX_FOLDERS; i++) {
        gchar *path_lower = g_utf8_strdown(mode->folders[i].path, -1);
        gchar *label_lower = g_utf8_strdown(mode->folders[i].label, -1);
        if (g_strrstr(path_lower, query_lower) || g_strrstr(label_lower, query_lower)) {
            tmux_add_filtered_row(mode, TMUX_ROW_FOLDER, i);
        }
        g_free(path_lower);
        g_free(label_lower);
    }
    g_free(query_lower);
}

void tmux_refresh(AppData *app) {
    if (!app) return;
    TmuxMode *mode = &app->tmux_mode;
    clear_zoxide_folders(mode->folders, mode->folder_count);
    mode->session_count = 0;
    mode->folder_count = 0;
    mode->filtered_count = 0;
    mode->last_error[0] = '\0';

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
            mode->session_count = parse_tmux_session_list(stdout_str, mode->sessions,
                                                          MAX_TMUX_SESSIONS,
                                                          parse_error, sizeof(parse_error));
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
            mode->folder_count = parse_zoxide_folder_list(stdout_str, mode->folders,
                                                          MAX_TMUX_FOLDERS,
                                                          parse_error, sizeof(parse_error));
        }
        g_clear_error(&error);
        g_free(stdout_str);
        g_free(zoxide);
    }

    const char *query = app->entry ? gtk_entry_get_text(GTK_ENTRY(app->entry)) : "";
    tmux_filter(app, query);
}

int tmux_row_count(AppData *app) {
    if (!app) return 0;
    TmuxMode *mode = &app->tmux_mode;
    if (mode->filtered_count > 0) return mode->filtered_count;
    return 1;
}

void tmux_format_row(AppData *app, int visible_idx, CofiRowCells *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!app) return;

    TmuxMode *mode = &app->tmux_mode;
    TmuxFolder *folder = tmux_folder_at_visible(app, visible_idx);
    static char windows_buf[16];
    static char attached_buf[16];

    if (folder) {
        out->cell_count = 3;
        out->cells[0].text = "[dir]";
        out->cells[0].width_hint = 5;
        out->cells[1].text = folder->label;
        out->cells[1].width_hint = 24;
        out->cells[2].text = folder->path;
        out->row_flags = COFI_ROW_ACTIONABLE;
        return;
    }
    TmuxSession *session = tmux_session_at_visible(app, visible_idx);
    if (!session) {
        out->cell_count = 1;
        if (mode->session_count + mode->folder_count > 0) {
            out->cells[0].text = "No matching tmux entries";
        } else if (mode->last_error[0] != '\0') {
            out->cells[0].text = mode->last_error;
            out->row_flags = COFI_ROW_ERROR;
        } else {
            out->cells[0].text = "No tmux sessions or zoxide folders";
        }
        return;
    }

    g_snprintf(windows_buf, sizeof(windows_buf), "%d %s",
               session->windows, session->windows == 1 ? "win" : "wins");
    g_snprintf(attached_buf, sizeof(attached_buf), "%d %s",
               session->attached, session->attached == 1 ? "client" : "clients");

    out->cell_count = 4;
    out->cells[0].text = "[tmx]";
    out->cells[0].width_hint = 5;
    out->cells[1].text = session->name;
    out->cells[2].text = windows_buf;
    out->cells[2].width_hint = 7;
    out->cells[2].align = 1;
    out->cells[3].text = attached_buf;
    out->cells[3].width_hint = 10;
    out->cells[3].align = 1;
    out->row_flags = COFI_ROW_ACTIONABLE;
}

const char *tmux_match_string(AppData *app, int visible_idx) {
    TmuxSession *session = tmux_session_at_visible(app, visible_idx);
    TmuxFolder *folder = tmux_folder_at_visible(app, visible_idx);
    if (folder) return folder->path;
    return session ? session->name : "";
}

const char *tmux_row_identity(AppData *app, int visible_idx) {
    TmuxSession *session = tmux_session_at_visible(app, visible_idx);
    TmuxFolder *folder = tmux_folder_at_visible(app, visible_idx);
    if (folder) return folder->path;
    return session ? session->name : "";
}

void tmux_on_enter(AppData *app) {
    if (!app) return;
    if (app->entry) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "tmux sessions...");
    }
    tmux_refresh(app);
}

void tmux_on_query_changed(AppData *app, const char *query) {
    tmux_filter(app, query);
    reset_selection(app);
}

void tmux_on_tick(AppData *app, int generation) {
    (void)generation;
    if (!app) return;
    TmuxRowType selected_type = TMUX_ROW_SESSION;
    gchar *selected_identity = NULL;
    TmuxSession *selected_session = tmux_session_at_visible(app, app->selection.provider_index);
    TmuxFolder *selected_folder = tmux_folder_at_visible(app, app->selection.provider_index);
    if (selected_session) {
        selected_type = TMUX_ROW_SESSION;
        selected_identity = g_strdup(selected_session->name);
    } else if (selected_folder) {
        selected_type = TMUX_ROW_FOLDER;
        selected_identity = g_strdup(selected_folder->path);
    }

    tmux_refresh(app);
    app->selection.provider_index = 0;
    if (selected_identity && selected_identity[0] != '\0') {
        for (int i = 0; i < app->tmux_mode.filtered_count; i++) {
            TmuxRowRef row = app->tmux_mode.filtered_rows[i];
            const char *identity = row.type == TMUX_ROW_SESSION
                ? app->tmux_mode.sessions[row.index].name
                : app->tmux_mode.folders[row.index].path;
            if (row.type == selected_type && strcmp(identity, selected_identity) == 0) {
                app->selection.provider_index = i;
                break;
            }
        }
    }
    g_free(selected_identity);
    update_scroll_position(app);
    update_display(app);
}

CofiActionStatus tmux_attach_visible(AppData *app, int visible_idx) {
    TmuxSession *session = tmux_session_at_visible(app, visible_idx);
    if (session) return tmux_attach_session(app, session->name);
    TmuxFolder *folder = tmux_folder_at_visible(app, visible_idx);
    return folder ? tmux_open_folder(app, folder->path) : COFI_ACTION_ERROR;
}

static CofiActionStatus run_tmux_admin_command(const char *command) {
    if (!command) return COFI_ACTION_ERROR;
    gboolean ok = s_run_tmux_command(command);
    return ok ? COFI_HANDLED_REFRESH : COFI_ACTION_ERROR;
}

CofiActionStatus tmux_kill_session(AppData *app, const char *session_name) {
    (void)app;
    gchar *command = tmux_build_kill_command(session_name);
    CofiActionStatus status = run_tmux_admin_command(command);
    if (status == COFI_HANDLED_REFRESH) {
        log_info("USER: tmux: killed session '%s'", session_name);
    }
    g_free(command);
    return status;
}

CofiActionStatus tmux_rename_session(AppData *app, const char *old_name, const char *new_name) {
    (void)app;
    gchar *command = tmux_build_rename_command(old_name, new_name);
    CofiActionStatus status = run_tmux_admin_command(command);
    if (status == COFI_HANDLED_REFRESH) {
        log_info("USER: tmux: renamed session '%s' to '%s'", old_name, new_name);
    }
    g_free(command);
    return status;
}

CofiActionStatus tmux_new_session(AppData *app, const char *session_name) {
    (void)app;
    const char *home = g_get_home_dir();
    gchar *command = tmux_build_new_session_command(session_name, home ? home : "/");
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

const char *tmux_selected_session_name(AppData *app) {
    if (!app) return NULL;
    TmuxSession *session = tmux_session_at_visible(app, app->selection.provider_index);
    return session ? session->name : NULL;
}

CofiActionStatus tmux_attach_named(AppData *app, const char *name) {
    if (!app || !name || name[0] == '\0') return COFI_NO_OP;
    for (int i = 0; i < app->tmux_mode.session_count; i++) {
        if (strcmp(app->tmux_mode.sessions[i].name, name) == 0) {
            return tmux_attach_session(app, name);
        }
    }
    return COFI_ACTION_ERROR;
}

#ifdef COFI_TESTING
int tmux_parse_session_list_test_hook(const char *output,
                                      TmuxSession *out,
                                      int max_out,
                                      char *error_out,
                                      size_t error_size) {
    return parse_tmux_session_list(output, out, max_out, error_out, error_size);
}

int tmux_parse_zoxide_list_test_hook(const char *output,
                                     TmuxFolder *out,
                                     int max_out,
                                     char *error_out,
                                     size_t error_size) {
    return parse_zoxide_folder_list(output, out, max_out, error_out, error_size);
}

void tmux_set_launch_impl_test_hook(gboolean (*impl)(const char *command)) {
    s_launch_in_terminal = impl ? impl : default_launch_in_terminal;
}

void tmux_set_command_impl_test_hook(gboolean (*impl)(const char *command)) {
    s_run_tmux_command = impl ? impl : default_run_tmux_command;
}
#endif
#endif /* COFI_TMUX_PARSER_TEST */

#ifdef COFI_TMUX_PARSER_TEST
int tmux_parse_session_list_test_hook(const char *output,
                                      TmuxSession *out,
                                      int max_out,
                                      char *error_out,
                                      size_t error_size) {
    return parse_tmux_session_list(output, out, max_out, error_out, error_size);
}

int tmux_parse_zoxide_list_test_hook(const char *output,
                                     TmuxFolder *out,
                                     int max_out,
                                     char *error_out,
                                     size_t error_size) {
    return parse_zoxide_folder_list(output, out, max_out, error_out, error_size);
}

#endif
