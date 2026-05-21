#ifndef PROJECTS_REMOTE_WINDOWS_H
#define PROJECTS_REMOTE_WINDOWS_H

#include <glib.h>
#include <stddef.h>

typedef struct AppData AppData;

gboolean projects_remote_cmdline_matches_attach(const char *cmdline,
                                                size_t len,
                                                const char *host,
                                                const char *tool,
                                                const char *session_name);

gboolean projects_activate_remote_attach_window(AppData *app,
                                                const char *host,
                                                const char *tool,
                                                const char *session_name);

#endif /* PROJECTS_REMOTE_WINDOWS_H */
