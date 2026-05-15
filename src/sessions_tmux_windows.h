#ifndef SESSIONS_TMUX_WINDOWS_H
#define SESSIONS_TMUX_WINDOWS_H

#include <glib.h>
#include <sys/types.h>

typedef struct AppData AppData;

int sessions_parse_tmux_client_pids(const char *output, pid_t *pids, int max_pids);
gboolean sessions_activate_tmux_window(AppData *app, const char *session_name);

#endif /* SESSIONS_TMUX_WINDOWS_H */
