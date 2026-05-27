#ifndef PROCESS_WINDOWS_H
#define PROCESS_WINDOWS_H

#include <glib.h>
#include <sys/types.h>
#include <X11/Xlib.h>

typedef struct AppData AppData;

gboolean process_find_window_for_pid(AppData *app, pid_t pid, Window *window_out);
int process_read_parent_pid(pid_t pid);
gboolean process_find_window_for_pid_ancestry(AppData *app,
                                              pid_t pid,
                                              int max_depth,
                                              Window *window_out);

#ifdef COFI_TESTING
void process_set_window_resolvers_test_hook(int (*window_for_pid)(AppData *, pid_t, Window *),
                                            int (*parent_pid)(pid_t));
#endif

#endif /* PROCESS_WINDOWS_H */
