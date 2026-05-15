#ifndef SESSIONS_H
#define SESSIONS_H

#include <glib.h>
#include <stddef.h>
#include <string.h>

#include "cofi_tab_provider.h"

#define MAX_SESSIONS 128
#define MAX_SESSION_FOLDERS 128
#define MAX_SESSION_NAME_LEN 256

typedef struct AppData AppData;

typedef enum {
    SESSION_ROW_SESSION,
    SESSION_ROW_FOLDER,
} SessionRowType;

typedef enum {
    SESSION_BACKEND_TMUX,
    SESSION_BACKEND_ZELLIJ,
} SessionBackend;

typedef struct {
    SessionBackend backend;
    char name[MAX_SESSION_NAME_LEN];
    int windows;
    int attached;
} SessionEntry;

typedef struct {
    char *path;
    char *label;
} SessionFolder;

typedef struct {
    SessionRowType type;
    int index;
} SessionRowRef;

typedef struct {
    SessionEntry sessions[MAX_SESSIONS];
    SessionFolder folders[MAX_SESSION_FOLDERS];
    SessionRowRef filtered_rows[MAX_SESSIONS + MAX_SESSION_FOLDERS];
    int session_count;
    int folder_count;
    int filtered_count;
    char last_error[256];
} SessionsMode;

static inline void init_sessions_mode(SessionsMode *mode) {
    if (mode) {
        memset(mode, 0, sizeof(*mode));
    }
}

void sessions_refresh(AppData *app);
void sessions_filter(AppData *app, const char *query);
int sessions_row_count(AppData *app);
void sessions_format_row(AppData *app, int visible_idx, CofiRowCells *out);
const char *sessions_match_string(AppData *app, int visible_idx);
const char *sessions_row_identity(AppData *app, int visible_idx);
void sessions_on_enter(AppData *app);
void sessions_on_query_changed(AppData *app, const char *query);
void sessions_on_tick(AppData *app, int generation);
CofiActionStatus sessions_attach_visible(AppData *app, int visible_idx);
CofiActionStatus sessions_attach_named(AppData *app, const char *name);
CofiActionStatus sessions_kill_session(AppData *app,
                                   const char *session_name,
                                   SessionBackend backend);
CofiActionStatus sessions_rename_tmux_session(AppData *app, const char *old_name, const char *new_name);
CofiActionStatus sessions_new_session(AppData *app,
                                       const char *session_name,
                                       SessionBackend backend,
                                       const char *start_dir);
SessionEntry *sessions_selected_session(AppData *app);
SessionFolder *sessions_selected_folder(AppData *app);
SessionFolder *sessions_folder_at_visible(AppData *app, int visible_idx);
const char *sessions_get_shortcut_hint(AppData *app);

#ifdef COFI_TESTING
void sessions_set_launch_impl_test_hook(gboolean (*impl)(const char *command));
void sessions_set_command_impl_test_hook(gboolean (*impl)(const char *command));
#endif

#endif /* SESSIONS_H */
