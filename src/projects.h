#ifndef PROJECTS_H
#define PROJECTS_H

#include <glib.h>
#include <stddef.h>
#include <string.h>

#include "cofi_tab_provider.h"

#define MAX_PROJECTS 128
#define MAX_PROJECT_FOLDERS 128
#define MAX_PROJECT_SESSION_NAME_LEN 256

typedef struct AppData AppData;

typedef enum {
    PROJECT_ROW_SESSION,
    PROJECT_ROW_FOLDER,
} ProjectRowType;

typedef enum {
    PROJECT_BACKEND_TMUX,
    PROJECT_BACKEND_ZELLIJ,
} ProjectBackend;

typedef struct {
    ProjectBackend backend;
    char name[MAX_PROJECT_SESSION_NAME_LEN];
    int windows;
    int attached;
    gboolean is_saved_remote;
    char remote_host[128];
    char remote_cwd[512];
} ProjectSessionEntry;

typedef struct {
    char *path;
    char *label;
    gboolean is_remote;
    char remote_host[128];
} ProjectFolder;

typedef struct {
    ProjectRowType type;
    int index;
} ProjectRowRef;

typedef struct {
    ProjectSessionEntry projects[MAX_PROJECTS];
    ProjectFolder folders[MAX_PROJECT_FOLDERS];
    ProjectRowRef filtered_rows[MAX_PROJECTS + MAX_PROJECT_FOLDERS];
    int session_count;
    int folder_count;
    int filtered_count;
    char last_error[256];
} ProjectsMode;

static inline void init_projects_mode(ProjectsMode *mode) {
    if (mode) {
        memset(mode, 0, sizeof(*mode));
    }
}

void projects_refresh(AppData *app);
void projects_filter(AppData *app, const char *query);
int projects_row_count(AppData *app);
void projects_format_row(AppData *app, int visible_idx, CofiRowCells *out);
const char *projects_match_string(AppData *app, int visible_idx);
const char *projects_row_identity(AppData *app, int visible_idx);
void projects_on_enter(AppData *app);
void projects_on_query_changed(AppData *app, const char *query);
void projects_on_leave(AppData *app);
void projects_on_tick(AppData *app, int generation);
CofiActionStatus projects_attach_visible(AppData *app, int visible_idx);
CofiActionStatus projects_attach_named(AppData *app, const char *name);
gboolean projects_has_named(AppData *app, const char *name);
CofiActionStatus projects_open_folder(AppData *app, const char *path);
CofiActionStatus projects_open_folder_terminal(AppData *app, const ProjectFolder *folder);
CofiActionStatus projects_kill_session(AppData *app,
                                   const char *session_name,
                                   ProjectBackend backend);
CofiActionStatus projects_rename_tmux_session(AppData *app, const char *old_name, const char *new_name);
CofiActionStatus projects_new_session(AppData *app,
                                       const char *session_name,
                                       ProjectBackend backend,
                                       const char *start_dir);
gboolean projects_forget_selected_remote(AppData *app);
gboolean projects_forget_remote_entry(const char *host,
                                      ProjectBackend backend,
                                      const char *name,
                                      const char *cwd);
CofiActionStatus projects_remove_folder_entry(AppData *app,
                                              const char *path,
                                              gboolean is_remote,
                                              const char *remote_host);
ProjectSessionEntry *projects_selected_session(AppData *app);
ProjectFolder *projects_selected_folder(AppData *app);
ProjectFolder *projects_folder_at_visible(AppData *app, int visible_idx);
const char *projects_get_shortcut_hint(AppData *app);
const char *projects_slot_payload_for(AppData *app, int visible_idx);
CofiActionStatus projects_slot_recall(AppData *app, const char *payload);

#ifdef COFI_TESTING
void projects_set_launch_impl_test_hook(gboolean (*impl)(const char *command));
void projects_set_command_impl_test_hook(gboolean (*impl)(const char *command));
void projects_set_argv_launch_impl_test_hook(gboolean (*impl)(const char *const *argv));
void projects_set_exec_impl_test_hook(gboolean (*impl)(const char *command));
#endif

#endif /* PROJECTS_H */
