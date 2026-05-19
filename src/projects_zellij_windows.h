#ifndef PROJECTS_ZELLIJ_WINDOWS_H
#define PROJECTS_ZELLIJ_WINDOWS_H

#include <glib.h>
#include <stddef.h>

typedef struct AppData AppData;

gboolean projects_zellij_cmdline_matches_session(const char *cmdline,
                                                 size_t len,
                                                 const char *zellij_path,
                                                 const char *session_name);
gboolean projects_activate_zellij_window(AppData *app,
                                         const char *zellij_path,
                                         const char *session_name);

#endif /* PROJECTS_ZELLIJ_WINDOWS_H */
