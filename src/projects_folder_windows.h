#ifndef PROJECTS_FOLDER_WINDOWS_H
#define PROJECTS_FOLDER_WINDOWS_H

#include <X11/Xlib.h>
#include <glib.h>

#include "window_info.h"

gboolean projects_find_caja_folder_window(const WindowInfo *windows,
                                          int window_count,
                                          const Window *stack,
                                          unsigned long stack_count,
                                          const char *path,
                                          Window *window_out);

#endif /* PROJECTS_FOLDER_WINDOWS_H */
