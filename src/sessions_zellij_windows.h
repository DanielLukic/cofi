#ifndef SESSIONS_ZELLIJ_WINDOWS_H
#define SESSIONS_ZELLIJ_WINDOWS_H

#include <X11/Xlib.h>
#include <glib.h>
#include <stddef.h>

typedef struct AppData AppData;

gboolean sessions_zellij_cmdline_matches_session(const char *cmdline,
                                                 size_t len,
                                                 const char *session_name);
gboolean sessions_windowid_from_environ(const char *environ_data,
                                        size_t len,
                                        Window *window_out);
gboolean sessions_activate_zellij_window(AppData *app, const char *session_name);

#endif /* SESSIONS_ZELLIJ_WINDOWS_H */
