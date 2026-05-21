#ifndef PROJECTS_REMOTE_SCOPE_H
#define PROJECTS_REMOTE_SCOPE_H

#include <glib.h>

#include "app_data.h"

void projects_remote_scope_init(void);
void projects_remote_scope_begin_fetch(AppData *app, const char *host);
gboolean projects_remote_scope_apply(ProjectsMode *mode);
gboolean projects_remote_scope_is_active(void);
gboolean projects_remote_scope_is_loading(void);
const char *projects_remote_scope_current_host(void);
void projects_remote_scope_clear(void);
const char *projects_remote_scope_status_message(void);
void projects_remote_scope_clear_status_message(void);

#ifdef COFI_TESTING
typedef gboolean (*ProjectsRemoteExecFn)(const char *host,
                                         const char *const *remote_argv,
                                         gchar **stdout_out,
                                         gchar **stderr_out);
void projects_remote_scope_reset_for_test(void);
void projects_remote_scope_set_exec_for_test(ProjectsRemoteExecFn exec_fn);
gboolean projects_remote_scope_load_from_outputs_for_test(const char *host,
                                                          const char *tmux_out,
                                                          const char *zellij_out,
                                                          const char *zoxide_out,
                                                          char *error_out,
                                                          size_t error_size);
void projects_remote_scope_set_loading_for_test(const char *host, gboolean loading);
void projects_remote_scope_set_active_for_test(gboolean active);
gboolean projects_remote_scope_fetch_sync_for_test(const char *host, char *error_out, size_t error_size);
void projects_remote_scope_set_status_for_test(const char *status);
int projects_remote_scope_build_ssh_argv_for_test(const char *host,
                                                  const char *const *remote_argv,
                                                  gchar **argv_out,
                                                  int argv_cap);
#endif

#endif
