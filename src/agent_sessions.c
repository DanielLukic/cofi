#include "agent_sessions.h"

#include "detach_launch.h"
#include "fzf_algo.h"
#include "log.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define AGENT_SESSIONS_MAX_PROCESSED_LINES 50000
#define AGENT_SESSIONS_MAX_MATCHES_PER_FILE 40

typedef struct {
    AgentSessionsMode *mode;
    int generation;
} ReadContext;

static gchar *home_path(const char *first, const char *second);
static void update_result_search_text(AgentSessionResult *result);
static unsigned int term_mask_for_text(const AgentSessionQuery *query,
                                       const char *text,
                                       const char *fallback);
static unsigned int metadata_mask_for_result(const AgentSessionQuery *query,
                                             const AgentSessionResult *result);

static gboolean default_launch_in_terminal(const char *command) {
    return detach_launch_in_terminal_cmd(command);
}

static AgentSessionsLaunchImpl s_launch_impl = default_launch_in_terminal;

static void notify_changed(AgentSessionsMode *mode) {
    if (mode && mode->changed_cb) {
        mode->changed_cb(mode->changed_user_data);
    }
}

static unsigned int full_query_mask(const AgentSessionQuery *query) {
    if (!query) return 0;
    return query->term_count >= 31
        ? 0xffffffffu : ((1u << query->term_count) - 1u);
}

static void copy_trimmed(char *out, size_t out_size, const char *start, size_t len) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!start) return;
    while (len > 0 && g_ascii_isspace((guchar)*start)) {
        start++;
        len--;
    }
    while (len > 0 && g_ascii_isspace((guchar)start[len - 1])) {
        len--;
    }
    size_t copy = len < out_size - 1 ? len : out_size - 1;
    memcpy(out, start, copy);
    out[copy] = '\0';
}

int agent_sessions_parse_query(const char *input, AgentSessionQuery *out) {
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    const char *text = input ? input : "";
    const char *pipe = strchr(text, '|');
    if (pipe) {
        copy_trimmed(out->left, sizeof(out->left), text, (size_t)(pipe - text));
        copy_trimmed(out->refine, sizeof(out->refine), pipe + 1, strlen(pipe + 1));
    } else {
        copy_trimmed(out->left, sizeof(out->left), text, strlen(text));
    }

    char left_copy[sizeof(out->left)];
    g_strlcpy(left_copy, out->left, sizeof(left_copy));
    char *save = NULL;
    for (char *tok = strtok_r(left_copy, " \t\r\n", &save);
         tok && out->term_count < AGENT_SESSION_MAX_TERMS;
         tok = strtok_r(NULL, " \t\r\n", &save)) {
        g_strlcpy(out->terms[out->term_count++], tok,
                  sizeof(out->terms[0]));
    }
    return out->term_count;
}

static void append_text(char *out, size_t out_size, const char *text) {
    if (!out || out_size == 0 || !text || text[0] == '\0') return;
    size_t used = strlen(out);
    if (used >= out_size - 1) return;
    if (used > 0) {
        g_strlcat(out, " ", out_size);
    }
    g_strlcat(out, text, out_size);
}

static void extract_content_node(JsonNode *node, char *out, size_t out_size) {
    if (!node || !out || out_size == 0) return;
    if (JSON_NODE_HOLDS_VALUE(node)) {
        const char *text = json_node_get_string(node);
        append_text(out, out_size, text);
        return;
    }
    if (JSON_NODE_HOLDS_ARRAY(node)) {
        JsonArray *array = json_node_get_array(node);
        guint len = json_array_get_length(array);
        for (guint i = 0; i < len; i++) {
            JsonNode *item = json_array_get_element(array, i);
            if (!item) continue;
            if (JSON_NODE_HOLDS_VALUE(item)) {
                extract_content_node(item, out, out_size);
                if (strlen(out) > out_size / 2) break;
                continue;
            }
            if (!JSON_NODE_HOLDS_OBJECT(item)) continue;
            JsonObject *obj = json_node_get_object(item);
            if (json_object_has_member(obj, "text")) {
                extract_content_node(json_object_get_member(obj, "text"), out, out_size);
            } else if (json_object_has_member(obj, "content")) {
                extract_content_node(json_object_get_member(obj, "content"), out, out_size);
            }
            if (strlen(out) > out_size / 2) break;
        }
    }
}

static void extract_object_text(JsonObject *obj, char *out, size_t out_size) {
    if (!obj) return;
    if (json_object_has_member(obj, "customTitle")) {
        extract_content_node(json_object_get_member(obj, "customTitle"), out, out_size);
    }
    if (json_object_has_member(obj, "agentName")) {
        extract_content_node(json_object_get_member(obj, "agentName"), out, out_size);
    }
    if (json_object_has_member(obj, "display")) {
        extract_content_node(json_object_get_member(obj, "display"), out, out_size);
    }
    if (json_object_has_member(obj, "text")) {
        extract_content_node(json_object_get_member(obj, "text"), out, out_size);
    }
    if (json_object_has_member(obj, "content")) {
        extract_content_node(json_object_get_member(obj, "content"), out, out_size);
    }
    if (json_object_has_member(obj, "message")) {
        JsonNode *message = json_object_get_member(obj, "message");
        if (message && JSON_NODE_HOLDS_VALUE(message)) {
            extract_content_node(message, out, out_size);
        } else if (message && JSON_NODE_HOLDS_OBJECT(message)) {
            JsonObject *msg_obj = json_node_get_object(message);
            if (json_object_has_member(msg_obj, "content")) {
                extract_content_node(json_object_get_member(msg_obj, "content"), out, out_size);
            }
        }
    }
    if (json_object_has_member(obj, "payload")) {
        JsonNode *payload = json_object_get_member(obj, "payload");
        if (payload && JSON_NODE_HOLDS_OBJECT(payload)) {
            extract_object_text(json_node_get_object(payload), out, out_size);
        }
    }
    if (json_object_has_member(obj, "output")) {
        extract_content_node(json_object_get_member(obj, "output"), out, out_size);
    }
}

static void normalize_snippet(char *text) {
    if (!text) return;
    char out[AGENT_SESSION_TEXT_LEN];
    int j = 0;
    int was_space = 1;
    for (int i = 0; text[i] && j < (int)sizeof(out) - 1; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c < 32 || c > 126 || g_ascii_isspace(c)) {
            if (!was_space) {
                out[j++] = ' ';
                was_space = 1;
            }
        } else {
            out[j++] = (char)c;
            was_space = 0;
        }
    }
    while (j > 0 && out[j - 1] == ' ') j--;
    out[j] = '\0';
    g_strlcpy(text, out, AGENT_SESSION_TEXT_LEN);
}

int agent_sessions_extract_json_text(const char *line, char *out, size_t out_size) {
    if (!out || out_size == 0) return 0;
    out[0] = '\0';
    if (!line || line[0] == '\0') return 0;

    GError *error = NULL;
    JsonParser *parser = json_parser_new();
    gboolean ok = json_parser_load_from_data(parser, line, -1, &error);
    if (!ok) {
        g_clear_error(&error);
        g_object_unref(parser);
        return 0;
    }
    JsonNode *root = json_parser_get_root(parser);
    if (root && JSON_NODE_HOLDS_OBJECT(root)) {
        extract_object_text(json_node_get_object(root), out, out_size);
    }
    g_object_unref(parser);
    normalize_snippet(out);
    return out[0] != '\0';
}

int agent_sessions_extract_name_metadata(const char *line, char *out, size_t out_size) {
    if (!out || out_size == 0) return 0;
    out[0] = '\0';
    if (!line || line[0] == '\0') return 0;

    GError *error = NULL;
    JsonParser *parser = json_parser_new();
    gboolean ok = json_parser_load_from_data(parser, line, -1, &error);
    if (!ok) {
        g_clear_error(&error);
        g_object_unref(parser);
        return 0;
    }

    JsonNode *root = json_parser_get_root(parser);
    if (root && JSON_NODE_HOLDS_OBJECT(root)) {
        JsonObject *obj = json_node_get_object(root);
        const char *type = NULL;
        if (json_object_has_member(obj, "type")) {
            JsonNode *node = json_object_get_member(obj, "type");
            if (node && JSON_NODE_HOLDS_VALUE(node)) {
                type = json_node_get_string(node);
            }
        }

        const char *field = NULL;
        if (g_strcmp0(type, "custom-title") == 0) {
            field = "customTitle";
        } else if (g_strcmp0(type, "agent-name") == 0) {
            field = "agentName";
        }
        if (field && json_object_has_member(obj, field)) {
            JsonNode *node = json_object_get_member(obj, field);
            if (node && JSON_NODE_HOLDS_VALUE(node)) {
                const char *name = json_node_get_string(node);
                if (name && name[0] != '\0') {
                    g_strlcpy(out, name, out_size);
                }
            }
        }
    }

    g_object_unref(parser);
    return out[0] != '\0';
}

static const char *basename_no_ext(const char *path, char *out, size_t out_size) {
    const char *base = path ? strrchr(path, '/') : NULL;
    base = base ? base + 1 : (path ? path : "");
    g_strlcpy(out, base, out_size);
    char *dot = g_strrstr(out, ".jsonl");
    if (dot) *dot = '\0';
    return out;
}

static void claude_project_from_slug(const char *slug, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!slug || slug[0] == '\0') return;
    if (strcmp(slug, "-") == 0) {
        g_strlcpy(out, "/", out_size);
        return;
    }
    int j = 0;
    for (int i = 0; slug[i] && j < (int)out_size - 1; i++) {
        out[j++] = slug[i] == '-' ? '/' : slug[i];
    }
    out[j] = '\0';
}

static void compact_project_label(const char *project, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!project || project[0] == '\0') return;
    if (!strchr(project, '/')) {
        g_strlcpy(out, project, out_size);
        return;
    }

    gchar *base = g_path_get_basename(project);
    g_strlcpy(out, base, out_size);
    g_free(base);
}

static void fill_modified_metadata(AgentSessionResult *result) {
    if (!result || result->path[0] == '\0') return;
    GStatBuf st;
    if (g_stat(result->path, &st) != 0) return;
    result->modified_time = (long long)st.st_mtime;

    struct tm local_tm;
    if (!localtime_r(&st.st_mtime, &local_tm)) return;
    if (strftime(result->modified_text, sizeof(result->modified_text),
                 "%m-%d %H:%M", &local_tm) == 0) {
        result->modified_text[0] = '\0';
    }
}

static void fill_result_metadata(AgentSessionResult *result, const char *path) {
    if (!result || !path) return;
    g_strlcpy(result->path, path, sizeof(result->path));

    const char *claude = strstr(path, "/.claude/projects/");
    const char *codex = strstr(path, "/.codex/sessions/");
    if (claude) {
        g_strlcpy(result->source, "claude", sizeof(result->source));
        const char *rest = claude + strlen("/.claude/projects/");
        const char *slash = strchr(rest, '/');
        if (slash) {
            char slug[AGENT_SESSION_PROJECT_LEN];
            copy_trimmed(slug, sizeof(slug), rest, (size_t)(slash - rest));
            claude_project_from_slug(slug, result->project, sizeof(result->project));
            g_strlcpy(result->cwd, result->project, sizeof(result->cwd));
            const char *subagents = strstr(slash + 1, "/subagents/");
            if (subagents) {
                char parent[AGENT_SESSION_ID_LEN];
                copy_trimmed(parent, sizeof(parent), slash + 1,
                             (size_t)(subagents - (slash + 1)));
                char *dot = g_strrstr(parent, ".jsonl");
                if (dot) *dot = '\0';
                g_strlcpy(result->session_id, parent, sizeof(result->session_id));
            } else {
                basename_no_ext(path, result->session_id, sizeof(result->session_id));
            }
        }
    } else if (codex) {
        g_strlcpy(result->source, "codex", sizeof(result->source));
        g_strlcpy(result->project, "codex", sizeof(result->project));
        basename_no_ext(path, result->session_id, sizeof(result->session_id));
    } else if (strstr(path, "/.claude/history.jsonl")) {
        g_strlcpy(result->source, "claude", sizeof(result->source));
        g_strlcpy(result->project, "history", sizeof(result->project));
        g_strlcpy(result->session_id, "history", sizeof(result->session_id));
    } else if (strstr(path, "/.codex/history.jsonl")) {
        g_strlcpy(result->source, "codex", sizeof(result->source));
        g_strlcpy(result->project, "history", sizeof(result->project));
        g_strlcpy(result->session_id, "history", sizeof(result->session_id));
    } else {
        g_strlcpy(result->source, "session", sizeof(result->source));
        basename_no_ext(path, result->session_id, sizeof(result->session_id));
    }

    compact_project_label(result->project[0] ? result->project : result->source,
                          result->project_label, sizeof(result->project_label));
    fill_modified_metadata(result);
}

static void extract_cwd_from_object(JsonObject *obj, char *out, size_t out_size) {
    if (!obj || !out || out_size == 0 || out[0] != '\0') return;
    if (json_object_has_member(obj, "cwd")) {
        JsonNode *cwd = json_object_get_member(obj, "cwd");
        if (cwd && JSON_NODE_HOLDS_VALUE(cwd)) {
            const char *text = json_node_get_string(cwd);
            if (text && text[0]) {
                g_strlcpy(out, text, out_size);
                return;
            }
        }
    }
    if (json_object_has_member(obj, "payload")) {
        JsonNode *payload = json_object_get_member(obj, "payload");
        if (payload && JSON_NODE_HOLDS_OBJECT(payload)) {
            extract_cwd_from_object(json_node_get_object(payload), out, out_size);
        }
    }
}

static gboolean extract_cwd_from_json_line(const char *line, char *out, size_t out_size) {
    if (!line || !out || out_size == 0) return FALSE;
    out[0] = '\0';
    GError *error = NULL;
    JsonParser *parser = json_parser_new();
    gboolean ok = json_parser_load_from_data(parser, line, -1, &error);
    if (!ok) {
        g_clear_error(&error);
        g_object_unref(parser);
        return FALSE;
    }
    JsonNode *root = json_parser_get_root(parser);
    if (root && JSON_NODE_HOLDS_OBJECT(root)) {
        extract_cwd_from_object(json_node_get_object(root), out, out_size);
    }
    g_object_unref(parser);
    return out[0] != '\0';
}

static void fill_cwd_from_session_file(const char *path, char *out, size_t out_size) {
    if (!path || !out || out_size == 0 || out[0] != '\0') return;
    FILE *file = fopen(path, "r");
    if (!file) return;
    char line[4096];
    for (int i = 0; i < 80 && fgets(line, sizeof(line), file); i++) {
        if (extract_cwd_from_json_line(line, out, out_size)) {
            break;
        }
    }
    fclose(file);
}

gboolean agent_sessions_build_resume_command(const AgentSessionResult *result,
                                             char *out,
                                             size_t out_size) {
    if (!out || out_size == 0) return FALSE;
    out[0] = '\0';
    if (!result || result->session_id[0] == '\0' ||
        strcmp(result->session_id, "history") == 0) {
        return FALSE;
    }

    const char *binary = NULL;
    const char *resume_verb = NULL;
    if (strcmp(result->source, "claude") == 0) {
        binary = "claude";
        resume_verb = "--resume";
    } else if (strcmp(result->source, "codex") == 0) {
        binary = "codex";
        resume_verb = "resume";
    } else {
        return FALSE;
    }

    char cwd[AGENT_SESSION_PATH_LEN];
    g_strlcpy(cwd, result->cwd, sizeof(cwd));
    fill_cwd_from_session_file(result->path, cwd, sizeof(cwd));

    gchar *quoted_session = g_shell_quote(result->session_id);
    gchar *command = g_strdup_printf("%s %s %s", binary, resume_verb, quoted_session);
    if (cwd[0] != '\0' && g_file_test(cwd, G_FILE_TEST_IS_DIR)) {
        gchar *quoted_cwd = g_shell_quote(cwd);
        gchar *with_cd = g_strdup_printf("cd %s && %s", quoted_cwd, command);
        g_strlcpy(out, with_cd, out_size);
        g_free(with_cd);
        g_free(quoted_cwd);
    } else {
        g_strlcpy(out, command, out_size);
    }
    g_free(command);
    g_free(quoted_session);
    return out[0] != '\0';
}

gboolean agent_sessions_launch_result(const AgentSessionResult *result) {
    char command[AGENT_SESSION_COMMAND_LEN];
    if (!agent_sessions_build_resume_command(result, command, sizeof(command))) {
        return FALSE;
    }
    return s_launch_impl(command);
}

static gboolean path_is_under_dir(const char *path, const char *dir) {
    if (!path || !dir || !dir[0]) return FALSE;
    size_t len = strlen(dir);
    return strncmp(path, dir, len) == 0 && (path[len] == '/' || path[len] == '\0');
}

static gboolean path_allowed_for_delete(const char *path) {
    if (!path || !g_str_has_suffix(path, ".jsonl")) return FALSE;
    gchar *canonical = g_canonicalize_filename(path, NULL);
    gchar *claude_root = home_path(".claude", "projects");
    gchar *codex_root = home_path(".codex", "sessions");
    gboolean allowed = path_is_under_dir(canonical, claude_root) ||
                       path_is_under_dir(canonical, codex_root);
    g_free(canonical);
    g_free(claude_root);
    g_free(codex_root);
    return allowed;
}

static gboolean path_allowed_for_claude_rename(const char *path) {
    if (!path || !g_str_has_suffix(path, ".jsonl")) return FALSE;
    gchar *canonical = g_canonicalize_filename(path, NULL);
    gchar *claude_root = home_path(".claude", "projects");
    gboolean allowed = path_is_under_dir(canonical, claude_root);
    g_free(canonical);
    g_free(claude_root);
    return allowed;
}

static gchar *build_name_record(const char *type,
                                const char *field,
                                const char *name,
                                const char *session_id) {
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "type");
    json_builder_add_string_value(builder, type);
    json_builder_set_member_name(builder, field);
    json_builder_add_string_value(builder, name);
    json_builder_set_member_name(builder, "sessionId");
    json_builder_add_string_value(builder, session_id);
    json_builder_end_object(builder);

    JsonNode *root = json_builder_get_root(builder);
    JsonGenerator *generator = json_generator_new();
    json_generator_set_root(generator, root);
    gchar *json = json_generator_to_data(generator, NULL);

    g_object_unref(generator);
    json_node_free(root);
    g_object_unref(builder);
    return json;
}

static gboolean write_claude_name_records(const char *path,
                                          const char *session_id,
                                          const char *name) {
    if (!path || !session_id || session_id[0] == '\0' ||
        !name || name[0] == '\0') {
        return FALSE;
    }

    gchar *title = build_name_record("custom-title", "customTitle",
                                     name, session_id);
    gchar *agent = build_name_record("agent-name", "agentName",
                                     name, session_id);
    FILE *file = fopen(path, "a");
    if (!file) {
        log_warn("Failed to open agent session for rename '%s': %s",
                 path, g_strerror(errno));
        g_free(title);
        g_free(agent);
        return FALSE;
    }

    gboolean ok = fprintf(file, "%s\n%s\n", title, agent) > 0;
    if (fclose(file) != 0) {
        ok = FALSE;
    }
    g_free(title);
    g_free(agent);
    if (!ok) {
        log_warn("Failed to append agent session name records to '%s'", path);
    }
    return ok;
}

gboolean agent_sessions_delete_path(const char *path) {
    if (!path_allowed_for_delete(path)) {
        log_warn("Refusing to delete non-agent-session path: %s", path ? path : "(null)");
        return FALSE;
    }
    if (g_remove(path) != 0) {
        log_warn("Failed to delete agent session '%s': %s", path, g_strerror(errno));
        return FALSE;
    }
    log_info("Deleted agent session file: %s", path);
    return TRUE;
}

gboolean agent_sessions_delete_result(const AgentSessionResult *result) {
    return result ? agent_sessions_delete_path(result->path) : FALSE;
}

gboolean agent_sessions_rename_result(const AgentSessionResult *result,
                                      const char *name) {
    char trimmed[AGENT_SESSION_NAME_LEN];
    copy_trimmed(trimmed, sizeof(trimmed), name ? name : "", strlen(name ? name : ""));
    if (!result || strcmp(result->source, "claude") != 0 ||
        strcmp(result->session_id, "history") == 0 ||
        trimmed[0] == '\0' ||
        !path_allowed_for_claude_rename(result->path)) {
        return FALSE;
    }
    if (!write_claude_name_records(result->path, result->session_id, trimmed)) {
        return FALSE;
    }
    log_info("Renamed Claude agent session '%s' to '%s'",
             result->session_id, trimmed);
    return TRUE;
}

void agent_sessions_remove_path(AgentSessionsMode *mode, const char *path) {
    if (!mode || !path || !path[0]) return;
    for (int i = 0; i < mode->result_count; i++) {
        if (strcmp(mode->results[i].path, path) != 0) continue;
        for (int j = i; j < mode->result_count - 1; j++) {
            mode->results[j] = mode->results[j + 1];
        }
        mode->result_count--;
        memset(&mode->results[mode->result_count], 0, sizeof(mode->results[0]));
        agent_sessions_apply_refine(mode, NULL);
        return;
    }
}

void agent_sessions_rename_path(AgentSessionsMode *mode,
                                const char *path,
                                const char *name) {
    if (!mode || !path || !path[0] || !name) return;
    for (int i = 0; i < mode->result_count; i++) {
        if (strcmp(mode->results[i].path, path) != 0) continue;
        g_strlcpy(mode->results[i].display_name, name,
                  sizeof(mode->results[i].display_name));
        update_result_search_text(&mode->results[i]);
        mode->results[i].metadata_mask =
            metadata_mask_for_result(&mode->query, &mode->results[i]);
        mode->results[i].matched_mask |= mode->results[i].metadata_mask;
        agent_sessions_apply_refine(mode, NULL);
        return;
    }
}

#ifdef COFI_TESTING
void agent_sessions_set_launch_impl_for_test(AgentSessionsLaunchImpl launch_impl) {
    s_launch_impl = launch_impl ? launch_impl : default_launch_in_terminal;
}

gboolean agent_sessions_write_claude_name_records_for_test(const char *path,
                                                           const char *session_id,
                                                           const char *name) {
    return write_claude_name_records(path, session_id, name);
}
#endif

static void update_result_hit_text(AgentSessionResult *result) {
    if (!result) return;
    g_snprintf(result->hit_text, sizeof(result->hit_text), "%d", result->hit_count);
}

static gboolean claude_subagent_path(const char *path) {
    return path && strstr(path, "/subagents/");
}

static void merge_result_metadata(AgentSessionResult *result,
                                  const AgentSessionResult *candidate) {
    if (!result || !candidate) return;
    gboolean prefer_candidate_path =
        result->path[0] == '\0' ||
        (claude_subagent_path(result->path) && !claude_subagent_path(candidate->path)) ||
        (claude_subagent_path(result->path) == claude_subagent_path(candidate->path) &&
         candidate->modified_time > result->modified_time);
    if (prefer_candidate_path) {
        g_strlcpy(result->path, candidate->path, sizeof(result->path));
        g_strlcpy(result->cwd, candidate->cwd, sizeof(result->cwd));
    }
    if (candidate->modified_time > result->modified_time) {
        result->modified_time = candidate->modified_time;
        g_strlcpy(result->modified_text, candidate->modified_text,
                  sizeof(result->modified_text));
    }
}

static AgentSessionResult *find_or_add_result(AgentSessionsMode *mode, const char *path) {
    if (!mode || !path) return NULL;
    AgentSessionResult candidate;
    memset(&candidate, 0, sizeof(candidate));
    fill_result_metadata(&candidate, path);
    if (candidate.source[0] == '\0' || candidate.session_id[0] == '\0') {
        return NULL;
    }

    for (int i = 0; i < mode->result_count; i++) {
        if (strcmp(mode->results[i].source, candidate.source) == 0 &&
            strcmp(mode->results[i].session_id, candidate.session_id) == 0) {
            merge_result_metadata(&mode->results[i], &candidate);
            update_result_search_text(&mode->results[i]);
            return &mode->results[i];
        }
    }
    if (mode->result_count >= MAX_AGENT_SESSION_RESULTS) {
        return NULL;
    }
    AgentSessionResult *result = &mode->results[mode->result_count++];
    *result = candidate;
    update_result_search_text(result);
    result->metadata_mask = metadata_mask_for_result(&mode->query, result);
    result->matched_mask |= result->metadata_mask;
    return result;
}

static unsigned int term_mask_for_text(const AgentSessionQuery *query,
                                       const char *text,
                                       const char *fallback) {
    unsigned int mask = 0;
    gchar *lower_text = text ? g_ascii_strdown(text, -1) : NULL;
    gchar *lower_fallback = fallback ? g_ascii_strdown(fallback, -1) : NULL;
    for (int i = 0; i < query->term_count; i++) {
        gchar *lower_term = g_ascii_strdown(query->terms[i], -1);
        if ((lower_text && g_strrstr_len(lower_text, -1, lower_term)) ||
            (lower_fallback && g_strrstr_len(lower_fallback, -1, lower_term))) {
            mask |= (1u << i);
        }
        g_free(lower_term);
    }
    g_free(lower_text);
    g_free(lower_fallback);
    return mask;
}

void agent_sessions_format_match_text(const AgentSessionResult *result,
                                      char *out,
                                      size_t out_size) {
    if (!out || out_size == 0) return;
    if (!result) {
        out[0] = '\0';
        return;
    }
    g_snprintf(out, out_size, "%s %s %s %s %s %s %d %s",
               result->source,
               result->project,
               result->project_label,
               result->display_name,
               result->session_id,
               result->path,
               result->hit_count,
               result->match_text[0] ? result->match_text : result->snippet);
}

static void update_result_search_text(AgentSessionResult *result) {
    if (!result) return;
    agent_sessions_format_match_text(result, result->search_text,
                                     sizeof(result->search_text));
}

static unsigned int metadata_mask_for_result(const AgentSessionQuery *query,
                                             const AgentSessionResult *result) {
    if (!query || !result) return 0;
    char text[AGENT_SESSION_MATCH_LEN];
    g_snprintf(text, sizeof(text), "%s %s %s",
               result->project_label,
               result->display_name,
               result->session_id);
    return term_mask_for_text(query, text, NULL);
}

static int result_matches_refine(const AgentSessionsMode *mode,
                                 const AgentSessionResult *result) {
    if (!mode || !result) return 0;
    unsigned int full_mask = full_query_mask(&mode->query);
    if ((result->matched_mask & full_mask) != full_mask) {
        return 0;
    }
    if (mode->query.refine[0] == '\0') {
        return 1;
    }
    char text[AGENT_SESSION_MATCH_LEN];
    g_snprintf(text, sizeof(text), "%s %s %s %s",
               result->project_label,
               result->display_name,
               result->session_id,
               result->match_text[0] ? result->match_text : result->snippet);
    return fzf_has_match(mode->query.refine, text);
}

static int filtered_result_cmp(const void *lhs, const void *rhs, gpointer data) {
    const AgentSessionsMode *mode = (const AgentSessionsMode *)data;
    int li = *(const int *)lhs;
    int ri = *(const int *)rhs;
    const AgentSessionResult *left = &mode->results[li];
    const AgentSessionResult *right = &mode->results[ri];
    unsigned int full_mask = full_query_mask(&mode->query);
    int left_metadata = (left->metadata_mask & full_mask) == full_mask;
    int right_metadata = (right->metadata_mask & full_mask) == full_mask;
    if (left_metadata != right_metadata) {
        return right_metadata - left_metadata;
    }
    if (left->modified_time != right->modified_time) {
        return left->modified_time < right->modified_time ? 1 : -1;
    }
    if (left->hit_count != right->hit_count) {
        return right->hit_count - left->hit_count;
    }
    return strcmp(left->project_label, right->project_label);
}

void agent_sessions_apply_refine(AgentSessionsMode *mode, const char *refine) {
    if (!mode) return;
    if (refine) {
        g_strlcpy(mode->query.refine, refine, sizeof(mode->query.refine));
        g_strlcpy(mode->current_refine, refine, sizeof(mode->current_refine));
    }
    mode->filtered_count = 0;
    for (int i = 0; i < mode->result_count; i++) {
        if (result_matches_refine(mode, &mode->results[i])) {
            mode->filtered_indices[mode->filtered_count++] = i;
        }
    }
    g_qsort_with_data(mode->filtered_indices, mode->filtered_count,
                      sizeof(mode->filtered_indices[0]),
                      filtered_result_cmp, mode);
}

static void ingest_match(AgentSessionsMode *mode, const char *path, const char *line) {
    if (!mode || !path || !line) return;
    AgentSessionResult *result = find_or_add_result(mode, path);
    if (!result) return;

    char name[AGENT_SESSION_NAME_LEN];
    gboolean name_metadata =
        agent_sessions_extract_name_metadata(line, name, sizeof(name));
    if (name_metadata) {
        g_strlcpy(result->display_name, name, sizeof(result->display_name));
        update_result_search_text(result);
        result->metadata_mask = metadata_mask_for_result(&mode->query, result);
        result->matched_mask |= result->metadata_mask;
    }

    char text[AGENT_SESSION_TEXT_LEN];
    if (!agent_sessions_extract_json_text(line, text, sizeof(text))) {
        text[0] = '\0';
    }
    if (result->hit_count < AGENT_SESSIONS_MAX_MATCHES_PER_FILE) {
        result->hit_count++;
        update_result_hit_text(result);
    }
    result->matched_mask |= term_mask_for_text(&mode->query, text, line);
    if (!name_metadata && result->snippet[0] == '\0' && text[0] != '\0') {
        g_strlcpy(result->snippet, text, sizeof(result->snippet));
    }
    if (!name_metadata) {
        append_text(result->match_text, sizeof(result->match_text), text);
    }
    update_result_search_text(result);
    agent_sessions_apply_refine(mode, NULL);
}

const AgentSessionResult *agent_sessions_result_at(const AgentSessionsMode *mode,
                                                   int visible_idx) {
    if (!mode || visible_idx < 0 || visible_idx >= mode->filtered_count) return NULL;
    int raw = mode->filtered_indices[visible_idx];
    if (raw < 0 || raw >= mode->result_count) return NULL;
    return &mode->results[raw];
}

void agent_sessions_cancel(AgentSessionsMode *mode) {
    if (!mode) return;
    mode->generation++;
    mode->searching = 0;
    if (mode->process) {
        g_subprocess_force_exit(mode->process);
        g_clear_object(&mode->process);
    }
    g_clear_object(&mode->stdout_stream);
}

void agent_sessions_init(AgentSessionsMode *mode) {
    if (!mode) return;
    memset(mode, 0, sizeof(*mode));
    g_strlcpy(mode->status, "Type terms to search agent sessions", sizeof(mode->status));
}

static void read_next_line(ReadContext *ctx);

static gboolean ascii_digits_between(const char *start, const char *end) {
    if (!start || !end || start >= end) return FALSE;
    for (const char *p = start; p < end; p++) {
        if (!g_ascii_isdigit((guchar)*p)) return FALSE;
    }
    return TRUE;
}

static gboolean ingest_vimgrep_line(AgentSessionsMode *mode, char *line) {
    if (!mode || !line) return FALSE;
    for (char *first = strchr(line, ':'); first; first = strchr(first + 1, ':')) {
        char *second = strchr(first + 1, ':');
        if (!second) return FALSE;
        char *third = strchr(second + 1, ':');
        if (!third) return FALSE;
        if (!ascii_digits_between(first + 1, second) ||
            !ascii_digits_between(second + 1, third)) {
            continue;
        }
        *first = '\0';
        ingest_match(mode, line, third + 1);
        return TRUE;
    }
    return FALSE;
}

#ifdef COFI_TESTING
void agent_sessions_ingest_match_for_test(AgentSessionsMode *mode,
                                          const char *path,
                                          const char *line) {
    ingest_match(mode, path, line);
}

gboolean agent_sessions_ingest_vimgrep_line_for_test(AgentSessionsMode *mode,
                                                     char *line) {
    return ingest_vimgrep_line(mode, line);
}

void agent_sessions_seed_file_for_test(AgentSessionsMode *mode,
                                       const char *path) {
    if (!mode || !path) return;
    AgentSessionResult *result = find_or_add_result(mode, path);
    if (!result) return;
    agent_sessions_apply_refine(mode, NULL);
}
#endif

static void on_rg_stdout_line(GObject *source, GAsyncResult *res, gpointer user_data) {
    ReadContext *ctx = (ReadContext *)user_data;
    AgentSessionsMode *mode = ctx->mode;
    if (!mode || ctx->generation != mode->generation) {
        g_free(ctx);
        return;
    }

    GError *error = NULL;
    gsize length = 0;
    char *line = g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(source),
                                                      res, &length, &error);
    (void)length;
    if (error) {
        g_clear_error(&error);
        mode->searching = 0;
        if (mode->filtered_count == 0) {
            g_strlcpy(mode->status, "Agent session search failed", sizeof(mode->status));
        }
        notify_changed(mode);
        g_free(ctx);
        return;
    }

    if (!line) {
        mode->searching = 0;
        if (mode->filtered_count == 0) {
            g_strlcpy(mode->status, "No matching agent sessions found", sizeof(mode->status));
        } else {
            g_snprintf(mode->status, sizeof(mode->status), "%d matching sessions",
                       mode->filtered_count);
        }
        log_debug("Agent sessions search complete: %s", mode->status);
        notify_changed(mode);
        g_free(ctx);
        return;
    }

    mode->processed_lines++;
    if (ingest_vimgrep_line(mode, line)) {
        notify_changed(mode);
    }
    g_free(line);

    if (mode->processed_lines >= AGENT_SESSIONS_MAX_PROCESSED_LINES) {
        mode->searching = 0;
        g_snprintf(mode->status, sizeof(mode->status),
                   "Result limit reached (%d lines)", mode->processed_lines);
        if (mode->process) {
            g_subprocess_force_exit(mode->process);
        }
        notify_changed(mode);
        g_free(ctx);
        return;
    }
    if (mode->result_count >= MAX_AGENT_SESSION_RESULTS) {
        mode->searching = 0;
        g_snprintf(mode->status, sizeof(mode->status),
                   "Result limit reached (%d sessions)", mode->result_count);
        if (mode->process) {
            g_subprocess_force_exit(mode->process);
        }
        notify_changed(mode);
        g_free(ctx);
        return;
    }

    read_next_line(ctx);
}

static void read_next_line(ReadContext *ctx) {
    g_data_input_stream_read_line_async(ctx->mode->stdout_stream,
                                        G_PRIORITY_DEFAULT,
                                        NULL,
                                        on_rg_stdout_line,
                                        ctx);
}

static gchar *home_path(const char *first, const char *second) {
    return g_build_filename(g_get_home_dir(), first, second, NULL);
}

static gchar **build_rg_argv(const AgentSessionQuery *query) {
    GPtrArray *argv = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(argv, g_strdup("rg"));
    g_ptr_array_add(argv, g_strdup("--vimgrep"));
    g_ptr_array_add(argv, g_strdup("--line-buffered"));
    g_ptr_array_add(argv, g_strdup("--ignore-case"));
    g_ptr_array_add(argv, g_strdup("--fixed-strings"));
    g_ptr_array_add(argv, g_strdup("--trim"));
    g_ptr_array_add(argv, g_strdup("--max-columns"));
    g_ptr_array_add(argv, g_strdup("240"));
    g_ptr_array_add(argv, g_strdup("--max-columns-preview"));
    g_ptr_array_add(argv, g_strdup("--glob"));
    g_ptr_array_add(argv, g_strdup("*.jsonl"));
    g_ptr_array_add(argv, g_strdup("--glob"));
    g_ptr_array_add(argv, g_strdup("!**/file-history/**"));
    g_ptr_array_add(argv, g_strdup("--glob"));
    g_ptr_array_add(argv, g_strdup("!**/telemetry/**"));

    for (int i = 0; i < query->term_count; i++) {
        g_ptr_array_add(argv, g_strdup("-e"));
        g_ptr_array_add(argv, g_strdup(query->terms[i]));
    }

    g_ptr_array_add(argv, home_path(".claude", "projects"));
    g_ptr_array_add(argv, home_path(".codex", "sessions"));
    g_ptr_array_add(argv, NULL);
    return (gchar **)g_ptr_array_free(argv, FALSE);
}

#ifdef COFI_TESTING
gchar **agent_sessions_build_rg_argv_for_test(const AgentSessionQuery *query) {
    return build_rg_argv(query);
}
#endif

void agent_sessions_search(AgentSessionsMode *mode,
                           const char *query_text,
                           AgentSessionsChanged changed_cb,
                           gpointer user_data) {
    if (!mode) return;
    AgentSessionQuery parsed;
    agent_sessions_parse_query(query_text, &parsed);

    mode->changed_cb = changed_cb;
    mode->changed_user_data = user_data;

    if (strcmp(parsed.left, mode->current_left) == 0) {
        g_strlcpy(mode->query.refine, parsed.refine, sizeof(mode->query.refine));
        agent_sessions_apply_refine(mode, parsed.refine);
        notify_changed(mode);
        return;
    }

    agent_sessions_cancel(mode);
    memset(mode->results, 0, sizeof(mode->results));
    mode->result_count = 0;
    mode->filtered_count = 0;
    mode->processed_lines = 0;
    mode->query = parsed;
    g_strlcpy(mode->current_left, parsed.left, sizeof(mode->current_left));
    g_strlcpy(mode->current_refine, parsed.refine, sizeof(mode->current_refine));

    if (parsed.term_count == 0) {
        g_strlcpy(mode->status, "Type terms to search agent sessions", sizeof(mode->status));
        notify_changed(mode);
        return;
    }

    g_snprintf(mode->status, sizeof(mode->status), "Searching agent sessions for '%s'...",
               parsed.left);
    mode->searching = 1;
    int generation = ++mode->generation;

    gchar **argv = build_rg_argv(&parsed);
    GError *error = NULL;
    mode->process = g_subprocess_newv((const gchar * const *)argv,
                                      G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                      G_SUBPROCESS_FLAGS_STDERR_SILENCE,
                                      &error);
    g_strfreev(argv);
    if (!mode->process) {
        mode->searching = 0;
        g_snprintf(mode->status, sizeof(mode->status),
                   "Cannot start rg: %s", error ? error->message : "unknown error");
        g_clear_error(&error);
        notify_changed(mode);
        return;
    }

    GInputStream *stdout_pipe = g_subprocess_get_stdout_pipe(mode->process);
    mode->stdout_stream = g_data_input_stream_new(stdout_pipe);
    ReadContext *ctx = g_new0(ReadContext, 1);
    ctx->mode = mode;
    ctx->generation = generation;
    notify_changed(mode);
    read_next_line(ctx);
}
