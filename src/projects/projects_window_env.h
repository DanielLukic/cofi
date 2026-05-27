#ifndef PROJECTS_WINDOW_ENV_H
#define PROJECTS_WINDOW_ENV_H

#include <X11/Xlib.h>
#include <glib.h>
#include <stddef.h>

gboolean projects_windowid_from_environ(const char *environ_data,
                                        size_t len,
                                        Window *window_out);

#endif /* PROJECTS_WINDOW_ENV_H */
