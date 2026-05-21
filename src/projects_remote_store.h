#ifndef PROJECTS_REMOTE_STORE_H
#define PROJECTS_REMOTE_STORE_H

#include <glib.h>

#include "projects.h"

typedef struct {
    char host[128];
    ProjectBackend backend;
    char name[MAX_PROJECT_SESSION_NAME_LEN];
    char cwd[512];
} ProjectRemoteEntry;

void projects_remote_store_init(void);
gboolean projects_remote_store_reload(void);
int projects_remote_store_count(void);
const ProjectRemoteEntry *projects_remote_store_entry_at(int index);
int projects_remote_store_append_sessions(ProjectSessionEntry *out, int start, int max_out);
gboolean projects_remote_store_forget(const char *host,
                                      ProjectBackend backend,
                                      const char *name,
                                      const char *cwd);
gboolean projects_remote_store_save_intent(const char *host,
                                           ProjectBackend backend,
                                           const char *name,
                                           const char *cwd);

#ifdef COFI_TESTING
void projects_remote_store_set_path_for_test(const char *path);
void projects_remote_store_reset_for_test(void);
gboolean projects_remote_store_add_for_test(const char *host,
                                            ProjectBackend backend,
                                            const char *name,
                                            const char *cwd);
gboolean projects_remote_store_save_for_test(void);
#endif

#endif
