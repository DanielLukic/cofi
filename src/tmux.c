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

gchar *tmux_build_attach_command(const char *session_name) {
    if (!session_name || session_name[0] == '\0') return NULL;
    gchar *target = g_strconcat("=", session_name, NULL);
    gchar *quoted_target = g_shell_quote(target);
    gchar *command = g_strdup_printf("tmux attach-session -t %s", quoted_target);
    g_free(quoted_target);
    g_free(target);
    return command;
}

#ifndef COFI_TMUX_PARSER_TEST
static gboolean default_launch_in_terminal(const char *command) {
    return detach_launch_in_terminal_cmd(command);
}

static gboolean (*s_launch_in_terminal)(const char *command) = default_launch_in_terminal;

static TmuxSession *tmux_session_at_visible(AppData *app, int visible_idx) {
    if (!app) return NULL;
    TmuxMode *mode = &app->tmux_mode;
    if (visible_idx < 0 || visible_idx >= mode->filtered_count) return NULL;
    int raw = mode->filtered_indices[visible_idx];
    if (raw < 0 || raw >= mode->session_count) return NULL;
    return &mode->sessions[raw];
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

void tmux_filter(AppData *app, const char *query) {
    if (!app) return;
    TmuxMode *mode = &app->tmux_mode;
    mode->filtered_count = 0;

    if (!query || query[0] == '\0') {
        for (int i = 0; i < mode->session_count && i < MAX_TMUX_SESSIONS; i++) {
            mode->filtered_indices[mode->filtered_count++] = i;
        }
        return;
    }

    gchar *query_lower = g_utf8_strdown(query, -1);
    for (int i = 0; i < mode->session_count && i < MAX_TMUX_SESSIONS; i++) {
        gchar *name_lower = g_utf8_strdown(mode->sessions[i].name, -1);
        if (g_strrstr(name_lower, query_lower)) {
            mode->filtered_indices[mode->filtered_count++] = i;
        }
        g_free(name_lower);
    }
    g_free(query_lower);
}

void tmux_refresh(AppData *app) {
    if (!app) return;
    TmuxMode *mode = &app->tmux_mode;
    mode->session_count = 0;
    mode->filtered_count = 0;
    mode->last_error[0] = '\0';

    gchar *tmux = g_find_program_in_path("tmux");
    if (!tmux) {
        g_strlcpy(mode->last_error, "tmux not found", sizeof(mode->last_error));
        tmux_filter(app, "");
        return;
    }

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

    const char *query = app->entry ? gtk_entry_get_text(GTK_ENTRY(app->entry)) : "";
    tmux_filter(app, query);
}

int tmux_row_count(AppData *app) {
    if (!app) return 0;
    TmuxMode *mode = &app->tmux_mode;
    if (mode->last_error[0] != '\0') return 1;
    if (mode->session_count == 0) return 1;
    if (mode->filtered_count == 0) return 1;
    return mode->filtered_count;
}

void tmux_format_row(AppData *app, int visible_idx, CofiRowCells *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!app) return;

    TmuxMode *mode = &app->tmux_mode;
    static char windows_buf[16];
    static char attached_buf[16];

    if (mode->last_error[0] != '\0') {
        out->cell_count = 1;
        out->cells[0].text = mode->last_error;
        out->row_flags = COFI_ROW_ERROR;
        return;
    }
    if (mode->session_count == 0) {
        out->cell_count = 1;
        out->cells[0].text = "No tmux sessions";
        return;
    }
    TmuxSession *session = tmux_session_at_visible(app, visible_idx);
    if (!session) {
        out->cell_count = 1;
        out->cells[0].text = "No matching tmux sessions";
        return;
    }

    g_snprintf(windows_buf, sizeof(windows_buf), "%d %s",
               session->windows, session->windows == 1 ? "win" : "wins");
    g_snprintf(attached_buf, sizeof(attached_buf), "%d %s",
               session->attached, session->attached == 1 ? "client" : "clients");

    out->cell_count = 3;
    out->cells[0].text = session->name;
    out->cells[1].text = windows_buf;
    out->cells[1].width_hint = 7;
    out->cells[1].align = 1;
    out->cells[2].text = attached_buf;
    out->cells[2].width_hint = 10;
    out->cells[2].align = 1;
    out->row_flags = COFI_ROW_ACTIONABLE;
}

const char *tmux_match_string(AppData *app, int visible_idx) {
    TmuxSession *session = tmux_session_at_visible(app, visible_idx);
    return session ? session->name : "";
}

const char *tmux_row_identity(AppData *app, int visible_idx) {
    TmuxSession *session = tmux_session_at_visible(app, visible_idx);
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
    char selected_name[MAX_TMUX_SESSION_NAME_LEN] = {0};
    TmuxSession *selected = tmux_session_at_visible(app, app->selection.provider_index);
    if (selected) {
        g_strlcpy(selected_name, selected->name, sizeof(selected_name));
    }

    tmux_refresh(app);
    app->selection.provider_index = 0;
    if (selected_name[0] != '\0') {
        for (int i = 0; i < app->tmux_mode.filtered_count; i++) {
            int raw = app->tmux_mode.filtered_indices[i];
            if (strcmp(app->tmux_mode.sessions[raw].name, selected_name) == 0) {
                app->selection.provider_index = i;
                break;
            }
        }
    }
    update_scroll_position(app);
    update_display(app);
}

CofiActionStatus tmux_attach_visible(AppData *app, int visible_idx) {
    TmuxSession *session = tmux_session_at_visible(app, visible_idx);
    return session ? tmux_attach_session(app, session->name) : COFI_ACTION_ERROR;
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

void tmux_set_launch_impl_test_hook(gboolean (*impl)(const char *command)) {
    s_launch_in_terminal = impl ? impl : default_launch_in_terminal;
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

#endif
