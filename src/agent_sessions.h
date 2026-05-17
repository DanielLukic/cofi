#ifndef AGENT_SESSIONS_H
#define AGENT_SESSIONS_H

#include <gio/gio.h>

#define MAX_AGENT_SESSION_RESULTS 128
#define AGENT_SESSION_TEXT_LEN 512
#define AGENT_SESSION_MATCH_LEN 1400
#define AGENT_SESSION_PATH_LEN 512
#define AGENT_SESSION_PROJECT_LEN 160
#define AGENT_SESSION_ID_LEN 96
#define AGENT_SESSION_COMMAND_LEN 1024
#define AGENT_SESSION_MAX_TERMS 8

typedef struct {
    char source[16];
    char project[AGENT_SESSION_PROJECT_LEN];
    char cwd[AGENT_SESSION_PATH_LEN];
    char session_id[AGENT_SESSION_ID_LEN];
    char path[AGENT_SESSION_PATH_LEN];
    char snippet[AGENT_SESSION_TEXT_LEN];
    char match_text[AGENT_SESSION_MATCH_LEN];
    int hit_count;
    unsigned int matched_mask;
} AgentSessionResult;

typedef struct {
    char left[256];
    char refine[256];
    char terms[AGENT_SESSION_MAX_TERMS][64];
    int term_count;
} AgentSessionQuery;

typedef struct AgentSessionsMode AgentSessionsMode;
typedef void (*AgentSessionsChanged)(gpointer user_data);
typedef gboolean (*AgentSessionsLaunchImpl)(const char *command);

struct AgentSessionsMode {
    AgentSessionResult results[MAX_AGENT_SESSION_RESULTS];
    int result_count;
    int filtered_indices[MAX_AGENT_SESSION_RESULTS];
    int filtered_count;

    char status[160];
    char current_left[256];
    char current_refine[256];
    int searching;
    int generation;
    int processed_lines;

    GSubprocess *process;
    GDataInputStream *stdout_stream;
    AgentSessionsChanged changed_cb;
    gpointer changed_user_data;

    AgentSessionQuery query;
};

void agent_sessions_init(AgentSessionsMode *mode);
void agent_sessions_cancel(AgentSessionsMode *mode);
void agent_sessions_search(AgentSessionsMode *mode,
                           const char *query,
                           AgentSessionsChanged changed_cb,
                           gpointer user_data);
void agent_sessions_apply_refine(AgentSessionsMode *mode, const char *refine);
const AgentSessionResult *agent_sessions_result_at(const AgentSessionsMode *mode,
                                                   int visible_idx);
void agent_sessions_format_match_text(const AgentSessionResult *result,
                                      char *out,
                                      size_t out_size);
gboolean agent_sessions_build_resume_command(const AgentSessionResult *result,
                                             char *out,
                                             size_t out_size);
gboolean agent_sessions_launch_result(const AgentSessionResult *result);
gboolean agent_sessions_delete_path(const char *path);
gboolean agent_sessions_delete_result(const AgentSessionResult *result);
void agent_sessions_remove_path(AgentSessionsMode *mode, const char *path);

#ifdef COFI_TESTING
void agent_sessions_set_launch_impl_for_test(AgentSessionsLaunchImpl launch_impl);
#endif

int agent_sessions_parse_query(const char *input, AgentSessionQuery *out);
int agent_sessions_extract_json_text(const char *line, char *out, size_t out_size);
void agent_sessions_ingest_match_for_test(AgentSessionsMode *mode,
                                          const char *path,
                                          const char *line);

#endif /* AGENT_SESSIONS_H */
