#ifndef SESSIONS_PARSE_H
#define SESSIONS_PARSE_H

#include <glib.h>
#include <stddef.h>

#include "sessions.h"

typedef enum {
    SESSION_SLOT_INVALID,
    SESSION_SLOT_SESSION,
    SESSION_SLOT_FOLDER,
} SessionSlotKind;

typedef struct {
    SessionSlotKind kind;
    SessionBackend backend;
    const char *value;
} SessionSlotTarget;

int sessions_parse_tmux_list(const char *output,
                             SessionEntry *out,
                             int max_out,
                             char *error_out,
                             size_t error_size);
int sessions_parse_zellij_list(const char *output,
                               SessionEntry *out,
                               int max_out,
                               char *error_out,
                               size_t error_size);
int sessions_parse_zoxide_list(const char *output,
                               SessionFolder *out,
                               int max_out,
                               char *error_out,
                               size_t error_size);

void sessions_clear_folders(SessionFolder *folders, int count);
gchar *sessions_build_folder_session_name(const char *path);
gchar *sessions_build_session_slot_payload(SessionBackend backend, const char *name);
gchar *sessions_build_folder_slot_payload(const char *path);
gboolean sessions_parse_slot_payload(const char *payload, SessionSlotTarget *out);

const char *sessions_session_marker(SessionBackend backend);
const char *sessions_folder_marker(void);
void sessions_format_session_match_text(const SessionEntry *session,
                                        char *out,
                                        size_t out_size);
void sessions_format_folder_match_text(const SessionFolder *folder,
                                       char *out,
                                       size_t out_size);

#endif /* SESSIONS_PARSE_H */
