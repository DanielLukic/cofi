#ifndef SESSIONS_H
#define SESSIONS_H

#include <gio/gio.h>

#define MAX_SESSION_RESULTS 512
#define SESSION_TEXT_LEN 512
#define SESSION_MATCH_LEN 1400
#define SESSION_PATH_LEN 512
#define SESSION_PROJECT_LEN 160
#define SESSION_ID_LEN 96
#define SESSION_NAME_LEN 160
#define SESSION_COMMAND_LEN 1024
#define SESSION_MAX_TERMS 8

typedef struct {
    char source[16];
    char project[SESSION_PROJECT_LEN];
    char project_label[SESSION_PROJECT_LEN];
    char cwd[SESSION_PATH_LEN];
    char session_id[SESSION_ID_LEN];
    char display_name[SESSION_NAME_LEN];
    char path[SESSION_PATH_LEN];
    char hit_text[16];
    char modified_text[12];
    char snippet[SESSION_TEXT_LEN];
    char match_text[SESSION_MATCH_LEN];
    char search_text[SESSION_MATCH_LEN];
    int hit_count;
    long long modified_time;
    unsigned int matched_mask;
    unsigned int metadata_mask;
} SessionResult;

typedef struct {
    char left[256];
    char refine[256];
    char terms[SESSION_MAX_TERMS][64];
    int term_count;
} SessionQuery;

typedef struct SessionsMode SessionsMode;
typedef void (*SessionsChanged)(gpointer user_data);
typedef gboolean (*SessionsLaunchImpl)(const char *command);

struct SessionsMode {
    SessionResult results[MAX_SESSION_RESULTS];
    int result_count;
    int filtered_indices[MAX_SESSION_RESULTS];
    int filtered_count;

    char status[160];
    char current_left[256];
    char current_refine[256];
    int searching;
    int generation;
    int processed_lines;

    GSubprocess *process;
    GDataInputStream *stdout_stream;
    SessionsChanged changed_cb;
    gpointer changed_user_data;

    SessionQuery query;
};

void sessions_init(SessionsMode *mode);
void sessions_cancel(SessionsMode *mode);
void sessions_search(SessionsMode *mode,
                           const char *query,
                           SessionsChanged changed_cb,
                           gpointer user_data);
void sessions_apply_refine(SessionsMode *mode, const char *refine);
const SessionResult *sessions_result_at(const SessionsMode *mode,
                                                   int visible_idx);
void sessions_format_match_text(const SessionResult *result,
                                      char *out,
                                      size_t out_size);
gboolean sessions_build_resume_command(const SessionResult *result,
                                             char *out,
                                             size_t out_size);
gboolean sessions_launch_result(const SessionResult *result);
gboolean sessions_delete_path(const char *path);
gboolean sessions_delete_result(const SessionResult *result);
void sessions_remove_path(SessionsMode *mode, const char *path);
gboolean sessions_rename_result(const SessionResult *result,
                                      const char *name);
void sessions_rename_path(SessionsMode *mode,
                                const char *path,
                                const char *name);

#ifdef COFI_TESTING
void sessions_set_launch_impl_for_test(SessionsLaunchImpl launch_impl);
gboolean sessions_write_claude_name_records_for_test(const char *path,
                                                           const char *session_id,
                                                           const char *name);
gboolean sessions_write_codex_name_record_for_test(const char *path,
                                                         const char *session_id,
                                                         const char *name);
#endif

int sessions_parse_query(const char *input, SessionQuery *out);
int sessions_extract_json_text(const char *line, char *out, size_t out_size);
int sessions_extract_name_metadata(const char *line, char *out, size_t out_size);

#ifdef COFI_TESTING
void sessions_ingest_match_for_test(SessionsMode *mode,
                                          const char *path,
                                          const char *line);
gboolean sessions_ingest_vimgrep_line_for_test(SessionsMode *mode,
                                                     char *line);
gchar **sessions_build_rg_argv_for_test(const SessionQuery *query);
void sessions_seed_file_for_test(SessionsMode *mode,
                                       const char *path);
#endif

#endif /* SESSIONS_H */
