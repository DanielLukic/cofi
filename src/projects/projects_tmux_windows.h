#ifndef PROJECTS_TMUX_WINDOWS_H
#define PROJECTS_TMUX_WINDOWS_H

#include <glib.h>
#include <sys/types.h>

typedef struct AppData AppData;

int projects_parse_tmux_client_pids(const char *output, pid_t *pids, int max_pids);
gboolean projects_activate_tmux_window(AppData *app,
                                       const char *tmux_path,
                                       const char *session_name);

#endif /* PROJECTS_TMUX_WINDOWS_H */
