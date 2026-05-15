#ifndef TMUX_H
#define TMUX_H

#include <glib.h>
#include <stddef.h>
#include <string.h>

#include "cofi_tab_provider.h"

#define MAX_TMUX_SESSIONS 128
#define MAX_TMUX_FOLDERS 128
#define MAX_TMUX_SESSION_NAME_LEN 256

typedef struct AppData AppData;

typedef enum {
    TMUX_ROW_SESSION,
    TMUX_ROW_FOLDER,
} TmuxRowType;

typedef struct {
    char name[MAX_TMUX_SESSION_NAME_LEN];
    int windows;
    int attached;
} TmuxSession;

typedef struct {
    char *path;
    char *label;
} TmuxFolder;

typedef struct {
    TmuxRowType type;
    int index;
} TmuxRowRef;

typedef struct {
    TmuxSession sessions[MAX_TMUX_SESSIONS];
    TmuxFolder folders[MAX_TMUX_FOLDERS];
    TmuxRowRef filtered_rows[MAX_TMUX_SESSIONS + MAX_TMUX_FOLDERS];
    int session_count;
    int folder_count;
    int filtered_count;
    char last_error[256];
} TmuxMode;

static inline void init_tmux_mode(TmuxMode *mode) {
    if (mode) {
        memset(mode, 0, sizeof(*mode));
    }
}

void tmux_refresh(AppData *app);
void tmux_filter(AppData *app, const char *query);
int tmux_row_count(AppData *app);
void tmux_format_row(AppData *app, int visible_idx, CofiRowCells *out);
const char *tmux_match_string(AppData *app, int visible_idx);
const char *tmux_row_identity(AppData *app, int visible_idx);
void tmux_on_enter(AppData *app);
void tmux_on_query_changed(AppData *app, const char *query);
void tmux_on_tick(AppData *app, int generation);
CofiActionStatus tmux_attach_visible(AppData *app, int visible_idx);
CofiActionStatus tmux_attach_named(AppData *app, const char *name);
CofiActionStatus tmux_kill_session(AppData *app, const char *session_name);
CofiActionStatus tmux_rename_session(AppData *app, const char *old_name, const char *new_name);
CofiActionStatus tmux_new_session(AppData *app, const char *session_name);
const char *tmux_selected_session_name(AppData *app);
gchar *tmux_build_attach_command(const char *session_name);
gchar *tmux_build_folder_session_command(const char *path);
gchar *tmux_build_kill_command(const char *session_name);
gchar *tmux_build_rename_command(const char *old_name, const char *new_name);
gchar *tmux_build_new_session_command(const char *session_name, const char *start_dir);

#ifdef COFI_TESTING
int tmux_parse_session_list_test_hook(const char *output,
                                      TmuxSession *out,
                                      int max_out,
                                      char *error_out,
                                      size_t error_size);
int tmux_parse_zoxide_list_test_hook(const char *output,
                                     TmuxFolder *out,
                                     int max_out,
                                     char *error_out,
                                     size_t error_size);
void tmux_set_launch_impl_test_hook(gboolean (*impl)(const char *command));
void tmux_set_command_impl_test_hook(gboolean (*impl)(const char *command));
#endif

#endif /* TMUX_H */
