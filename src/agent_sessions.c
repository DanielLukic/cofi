#include "agent_sessions.h"

#include "fzf_algo.h"
#include "log.h"

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <string.h>

#define AGENT_SESSIONS_MAX_PROCESSED_LINES 3000

typedef struct {
    AgentSessionsMode *mode;
    int generation;
} ReadContext;

static void notify_changed(AgentSessionsMode *mode) {
    if (mode && mode->changed_cb) {
        mode->changed_cb(mode->changed_user_data);
    }
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
            if (!item || !JSON_NODE_HOLDS_OBJECT(item)) continue;
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
}

static AgentSessionResult *find_or_add_result(AgentSessionsMode *mode, const char *path) {
    if (!mode || !path) return NULL;
    for (int i = 0; i < mode->result_count; i++) {
        if (strcmp(mode->results[i].path, path) == 0) {
            return &mode->results[i];
        }
    }
    if (mode->result_count >= MAX_AGENT_SESSION_RESULTS) {
        return NULL;
    }
    AgentSessionResult *result = &mode->results[mode->result_count++];
    memset(result, 0, sizeof(*result));
    fill_result_metadata(result, path);
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
    g_snprintf(out, out_size, "%s %s %s %s %d %s",
               result->source,
               result->project,
               result->session_id,
               result->path,
               result->hit_count,
               result->match_text[0] ? result->match_text : result->snippet);
}

static int result_matches_refine(const AgentSessionsMode *mode,
                                 const AgentSessionResult *result) {
    if (!mode || !result) return 0;
    unsigned int full_mask = mode->query.term_count >= 31
        ? 0xffffffffu : ((1u << mode->query.term_count) - 1u);
    if ((result->matched_mask & full_mask) != full_mask) {
        return 0;
    }
    if (mode->query.refine[0] == '\0') {
        return 1;
    }
    const char *text = result->match_text[0] ? result->match_text : result->snippet;
    return fzf_has_match(mode->query.refine, text);
}

static int filtered_result_cmp(const void *lhs, const void *rhs, gpointer data) {
    const AgentSessionsMode *mode = (const AgentSessionsMode *)data;
    int li = *(const int *)lhs;
    int ri = *(const int *)rhs;
    const AgentSessionResult *left = &mode->results[li];
    const AgentSessionResult *right = &mode->results[ri];
    if (left->hit_count != right->hit_count) {
        return right->hit_count - left->hit_count;
    }
    return strcmp(left->project, right->project);
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

    char text[AGENT_SESSION_TEXT_LEN];
    if (!agent_sessions_extract_json_text(line, text, sizeof(text))) {
        g_strlcpy(text, line, sizeof(text));
        normalize_snippet(text);
    }
    result->hit_count++;
    result->matched_mask |= term_mask_for_text(&mode->query, text, line);
    if (result->snippet[0] == '\0' && text[0] != '\0') {
        g_strlcpy(result->snippet, text, sizeof(result->snippet));
    }
    append_text(result->match_text, sizeof(result->match_text), text);
    agent_sessions_apply_refine(mode, NULL);
}

void agent_sessions_ingest_match_for_test(AgentSessionsMode *mode,
                                          const char *path,
                                          const char *line) {
    ingest_match(mode, path, line);
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
        notify_changed(mode);
        g_free(ctx);
        return;
    }

    mode->processed_lines++;
    char *first = strchr(line, ':');
    char *second = first ? strchr(first + 1, ':') : NULL;
    char *third = second ? strchr(second + 1, ':') : NULL;
    if (first && second && third) {
        *first = '\0';
        ingest_match(mode, line, third + 1);
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

static gchar *history_path(const char *first) {
    return g_build_filename(g_get_home_dir(), first, "history.jsonl", NULL);
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
    g_ptr_array_add(argv, history_path(".claude"));
    g_ptr_array_add(argv, history_path(".codex"));
    g_ptr_array_add(argv, NULL);
    return (gchar **)g_ptr_array_free(argv, FALSE);
}

void agent_sessions_search(AgentSessionsMode *mode,
                           const char *query_text,
                           AgentSessionsChanged changed_cb,
                           gpointer user_data) {
    if (!mode) return;
    AgentSessionQuery parsed;
    agent_sessions_parse_query(query_text, &parsed);

    mode->changed_cb = changed_cb;
    mode->changed_user_data = user_data;

    if (strcmp(parsed.left, mode->current_left) == 0 && mode->searching) {
        g_strlcpy(mode->query.refine, parsed.refine, sizeof(mode->query.refine));
        agent_sessions_apply_refine(mode, parsed.refine);
        notify_changed(mode);
        return;
    }
    if (strcmp(parsed.left, mode->current_left) == 0 && !mode->searching) {
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
