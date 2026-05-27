#ifndef PROJECTS_PARSE_H
#define PROJECTS_PARSE_H

#include <glib.h>
#include <stddef.h>

#include "projects/projects.h"

typedef enum {
    PROJECT_SLOT_INVALID,
    PROJECT_SLOT_SESSION,
    PROJECT_SLOT_FOLDER,
} ProjectSlotKind;

typedef struct {
    ProjectSlotKind kind;
    ProjectBackend backend;
    const char *value;
} ProjectSlotTarget;

int projects_parse_tmux_list(const char *output,
                             ProjectSessionEntry *out,
                             int max_out,
                             char *error_out,
                             size_t error_size);
int projects_parse_zellij_list(const char *output,
                               ProjectSessionEntry *out,
                               int max_out,
                               char *error_out,
                               size_t error_size);
int projects_parse_zoxide_list(const char *output,
                               ProjectFolder *out,
                               int max_out,
                               char *error_out,
                               size_t error_size);

void projects_clear_folders(ProjectFolder *folders, int count);
gchar *projects_build_folder_session_name(const char *path);
gchar *projects_build_session_slot_payload(ProjectBackend backend, const char *name);
gchar *projects_build_folder_slot_payload(const char *path);
gboolean projects_parse_slot_payload(const char *payload, ProjectSlotTarget *out);

const char *projects_session_marker(ProjectBackend backend);
const char *projects_folder_marker(void);
void projects_format_session_match_text(const ProjectSessionEntry *session,
                                        char *out,
                                        size_t out_size);
void projects_format_folder_match_text(const ProjectFolder *folder,
                                       char *out,
                                       size_t out_size);

#endif /* PROJECTS_PARSE_H */
