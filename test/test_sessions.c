#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <glib/gstdio.h>

#include "../src/sessions.h"

static int pass = 0;
static int fail = 0;
static int g_launch_calls = 0;
static char g_last_launch_command[SESSION_COMMAND_LEN];

gboolean detach_launch_in_terminal_cmd(const char *command) {
    g_launch_calls++;
    g_strlcpy(g_last_launch_command, command ? command : "",
              sizeof(g_last_launch_command));
    return TRUE;
}

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

#define ASSERT_STR(name, actual, expected) \
    ASSERT_TRUE(name, strcmp((actual), (expected)) == 0)

static int argv_contains(char **argv, const char *value) {
    if (!argv || !value) return 0;
    for (int i = 0; argv[i]; i++) {
        if (strcmp(argv[i], value) == 0) return 1;
    }
    return 0;
}

static int argv_contains_substr(char **argv, const char *value) {
    if (!argv || !value) return 0;
    for (int i = 0; argv[i]; i++) {
        if (strstr(argv[i], value)) return 1;
    }
    return 0;
}

static void test_query_split(void) {
    SessionQuery query;
    int terms = sessions_parse_query(" marco unicode title | restart patch ", &query);

    ASSERT_TRUE("query has three left terms", terms == 3);
    ASSERT_STR("left side trimmed", query.left, "marco unicode title");
    ASSERT_STR("right side trimmed", query.refine, "restart patch");
    ASSERT_STR("term 0", query.terms[0], "marco");
    ASSERT_STR("term 1", query.terms[1], "unicode");
    ASSERT_STR("term 2", query.terms[2], "title");
}

static void test_extract_claude_message_text(void) {
    const char *line =
        "{\"type\":\"user\",\"message\":{\"role\":\"user\",\"content\":"
        "\"can you check marco unicode titles?\"}}";
    char text[SESSION_TEXT_LEN];

    ASSERT_TRUE("extract claude message",
                sessions_extract_json_text(line, text, sizeof(text)));
    ASSERT_STR("claude text", text, "can you check marco unicode titles?");
}

static void test_extract_codex_payload_text(void) {
    const char *line =
        "{\"type\":\"response_item\",\"payload\":{\"type\":\"message\","
        "\"role\":\"user\",\"content\":[{\"type\":\"input_text\","
        "\"text\":\"give me session hits for marco\"}]}}";
    char text[SESSION_TEXT_LEN];

    ASSERT_TRUE("extract codex message",
                sessions_extract_json_text(line, text, sizeof(text)));
    ASSERT_STR("codex text", text, "give me session hits for marco");
}

static void test_extract_claude_string_array_text(void) {
    const char *line =
        "{\"type\":\"assistant\",\"message\":{\"role\":\"assistant\","
        "\"content\":[\"first marco sentence\",\"second sentence\"]}}";
    char text[SESSION_TEXT_LEN];

    ASSERT_TRUE("extract claude string array",
                sessions_extract_json_text(line, text, sizeof(text)));
    ASSERT_STR("claude string array text", text,
               "first marco sentence second sentence");
}

static void test_extract_claude_session_name_metadata(void) {
    char name[SESSION_NAME_LEN];

    ASSERT_TRUE("extract custom title",
                sessions_extract_name_metadata(
                    "{\"type\":\"custom-title\",\"customTitle\":\"Architecture Sweep\","
                    "\"sessionId\":\"abc\"}",
                    name, sizeof(name)));
    ASSERT_STR("custom title", name, "Architecture Sweep");

    ASSERT_TRUE("extract agent name",
                sessions_extract_name_metadata(
                    "{\"type\":\"agent-name\",\"agentName\":\"Pascal\","
                    "\"sessionId\":\"abc\"}",
                    name, sizeof(name)));
    ASSERT_STR("agent name", name, "Pascal");
}

static void test_extract_codex_session_name_metadata(void) {
    char name[SESSION_NAME_LEN];

    ASSERT_TRUE("extract codex thread name",
                sessions_extract_name_metadata(
                    "{\"timestamp\":\"2026-05-18T09:00:00Z\","
                    "\"type\":\"event_msg\","
                    "\"payload\":{\"type\":\"thread_name_updated\","
                    "\"thread_id\":\"019d86e4-179d-7360-9bac-b66fafe6361c\","
                    "\"thread_name\":\"Marco Thread\"}}",
                    name, sizeof(name)));
    ASSERT_STR("codex thread name", name, "Marco Thread");
}

static void test_group_requires_all_terms_across_lines(void) {
    SessionsMode mode;
    sessions_init(&mode);
    sessions_parse_query("marco unicode | restart", &mode.query);

    const char *path = "/home/user/.claude/projects/-home-user-Projects-marco/abc.jsonl";
    sessions_ingest_match_for_test(&mode, path,
        "{\"type\":\"user\",\"message\":{\"content\":\"Marco window manager\"}}");
    ASSERT_TRUE("single term not visible yet", mode.filtered_count == 0);

    sessions_ingest_match_for_test(&mode, path,
        "{\"type\":\"assistant\",\"message\":{\"content\":\"unicode title fix restart\"}}");
    ASSERT_TRUE("all terms visible after second line", mode.filtered_count == 1);

    const SessionResult *result = sessions_result_at(&mode, 0);
    ASSERT_TRUE("result exists", result != NULL);
    ASSERT_STR("source parsed", result->source, "claude");
    ASSERT_STR("project parsed", result->project, "/home/user/Projects/marco");
    ASSERT_STR("project label shows path basename",
               result->project_label, "marco");
    ASSERT_STR("cwd falls back to project path", result->cwd, "/home/user/Projects/marco");
    ASSERT_STR("session id parsed", result->session_id, "abc");
    ASSERT_TRUE("hit count grouped", result && result->hit_count == 2);
}

static void test_project_label_uses_basename_outside_projects(void) {
    SessionsMode mode;
    sessions_init(&mode);
    sessions_parse_query("alpha", &mode.query);

    const char *path = "/home/user/.claude/projects/-home-user-work-backend/abc.jsonl";
    sessions_ingest_match_for_test(&mode, path,
        "{\"type\":\"user\",\"message\":{\"content\":\"alpha\"}}");

    const SessionResult *result = sessions_result_at(&mode, 0);
    ASSERT_TRUE("non-projects result exists", result != NULL);
    ASSERT_STR("non-projects project parsed", result->project, "/home/user/work/backend");
    ASSERT_STR("non-projects project label uses basename",
               result->project_label, "backend");
}

static void test_claude_subagent_hits_collapse_to_parent_session(void) {
    SessionsMode mode;
    sessions_init(&mode);
    sessions_parse_query("marco", &mode.query);

    const char *parent =
        "/home/user/.claude/projects/-home-user-Projects-marco/abc.jsonl";
    const char *subagent =
        "/home/user/.claude/projects/-home-user-Projects-marco/abc.jsonl/"
        "subagents/agent-check-123.jsonl";
    sessions_ingest_match_for_test(&mode, subagent,
        "{\"type\":\"assistant\",\"message\":{\"content\":\"marco from subagent\"}}");
    sessions_ingest_match_for_test(&mode, parent,
        "{\"type\":\"user\",\"message\":{\"content\":\"marco from parent\"}}");

    ASSERT_TRUE("subagent and parent collapse to one row", mode.filtered_count == 1);
    const SessionResult *result = sessions_result_at(&mode, 0);
    ASSERT_TRUE("collapsed result exists", result != NULL);
    ASSERT_STR("collapsed session id is parent", result->session_id, "abc");
    ASSERT_STR("collapsed result prefers parent path", result->path, parent);
    ASSERT_TRUE("collapsed hit count accumulates", result->hit_count == 2);
}

static void test_group_records_latest_claude_session_name(void) {
    SessionsMode mode;
    sessions_init(&mode);
    sessions_parse_query("marco", &mode.query);

    const char *path = "/home/user/.claude/projects/-home-user-Projects-marco/abc.jsonl";
    sessions_ingest_match_for_test(&mode, path,
        "{\"type\":\"custom-title\",\"customTitle\":\"Old Name\","
        "\"sessionId\":\"abc\"}");
    sessions_ingest_match_for_test(&mode, path,
        "{\"type\":\"agent-name\",\"agentName\":\"New Name\","
        "\"sessionId\":\"abc\"}");
    sessions_ingest_match_for_test(&mode, path,
        "{\"type\":\"user\",\"message\":{\"content\":\"marco alpha\"}}");

    const SessionResult *result = sessions_result_at(&mode, 0);
    ASSERT_TRUE("named result exists", result != NULL);
    ASSERT_STR("latest name wins", result->display_name, "New Name");
    ASSERT_STR("name metadata is not used as snippet",
               result->snippet, "marco alpha");
}

static void test_vimgrep_path_allows_colons(void) {
    SessionsMode mode;
    sessions_init(&mode);
    sessions_parse_query("marco", &mode.query);

    char line[] =
        "/home/user/.claude/projects/-home-user-Projects-marco:branch/abc.jsonl:"
        "12:3:{\"type\":\"user\",\"message\":{\"content\":\"marco alpha\"}}";
    ASSERT_TRUE("ingest vimgrep line with colon path",
                sessions_ingest_vimgrep_line_for_test(&mode, line));
    const SessionResult *result = sessions_result_at(&mode, 0);
    ASSERT_TRUE("colon path result exists", result != NULL);
    ASSERT_STR("colon path preserved", result->path,
               "/home/user/.claude/projects/-home-user-Projects-marco:branch/abc.jsonl");
}

static void test_refine_filters_grouped_sessions(void) {
    SessionsMode mode;
    sessions_init(&mode);
    sessions_parse_query("marco", &mode.query);

    sessions_ingest_match_for_test(&mode,
        "/home/user/.claude/projects/-home-user-Projects-marco/abc.jsonl",
        "{\"type\":\"user\",\"message\":{\"content\":\"patched marco restart\"}}");
    sessions_ingest_match_for_test(&mode,
        "/home/user/.claude/projects/-home-user-Projects-marco/def.jsonl",
        "{\"type\":\"user\",\"message\":{\"content\":\"marco title colors\"}}");

    ASSERT_TRUE("two sessions before refine", mode.filtered_count == 2);
    sessions_apply_refine(&mode, "restart");
    ASSERT_TRUE("one session after refine", mode.filtered_count == 1);
    const SessionResult *result = sessions_result_at(&mode, 0);
    ASSERT_STR("restart result", result->session_id, "abc");
}

static void test_raw_json_metadata_is_not_display_snippet(void) {
    SessionsMode mode;
    sessions_init(&mode);
    sessions_parse_query("marco", &mode.query);

    sessions_ingest_match_for_test(&mode,
        "/home/user/.claude/projects/-home-user-Projects-marco/abc.jsonl",
        "{\"parentUuid\":\"abc\",\"promptId\":\"marco\",\"isSidechain\":true}");

    const SessionResult *result = sessions_result_at(&mode, 0);
    ASSERT_TRUE("raw-json-only match still surfaces", result != NULL);
    ASSERT_STR("raw json is not row snippet", result->snippet, "");
    ASSERT_TRUE("raw json is not accumulated match text",
                strstr(result->match_text, "parentUuid") == NULL);
}

static void test_metadata_match_sorts_before_content_only_match(void) {
    SessionsMode mode;
    sessions_init(&mode);
    sessions_parse_query("marco", &mode.query);

    sessions_ingest_match_for_test(&mode,
        "/home/user/.claude/projects/-home-user-Projects-other/newer.jsonl",
        "{\"type\":\"user\",\"message\":{\"content\":\"marco content\"}}");
    sessions_ingest_match_for_test(&mode,
        "/home/user/.claude/projects/-home-user-Projects-marco/older.jsonl",
        "{\"type\":\"user\",\"message\":{\"content\":\"marco content\"}}");

    const SessionResult *result = sessions_result_at(&mode, 0);
    ASSERT_TRUE("metadata path match is first", result != NULL);
    ASSERT_STR("metadata path match project label", result->project_label, "marco");
}

static void test_seed_file_surfaces_path_match_without_content_hit(void) {
    SessionsMode mode;
    sessions_init(&mode);
    sessions_parse_query("marco", &mode.query);

    sessions_seed_file_for_test(
        &mode, "/home/user/.claude/projects/-home-user-Projects-marco/abc.jsonl");

    const SessionResult *result = sessions_result_at(&mode, 0);
    ASSERT_TRUE("metadata-only path match surfaces", result != NULL);
    ASSERT_STR("metadata-only project label", result->project_label, "marco");
    ASSERT_STR("metadata-only snippet stays empty", result->snippet, "");
}

static void test_metadata_match_gets_later_extracted_snippet(void) {
    SessionsMode mode;
    sessions_init(&mode);
    sessions_parse_query("marco", &mode.query);

    const char *path = "/home/user/.claude/projects/-home-user-Projects-marco/abc.jsonl";
    sessions_ingest_match_for_test(&mode, path,
        "{\"cwd\":\"/tmp/marco\"}");
    const SessionResult *result = sessions_result_at(&mode, 0);
    ASSERT_TRUE("metadata-only result exists", result != NULL);
    ASSERT_STR("metadata-only line does not create snippet", result->snippet, "");

    sessions_ingest_match_for_test(&mode, path,
        "{\"type\":\"user\",\"message\":{\"content\":\"human readable marco text\"}}");
    result = sessions_result_at(&mode, 0);
    ASSERT_STR("later extracted text becomes snippet",
               result->snippet, "human readable marco text");
}

static void test_broad_query_can_hold_hundreds_of_sessions(void) {
    SessionsMode mode;
    sessions_init(&mode);
    sessions_parse_query("alpha", &mode.query);

    char path[256];
    for (int i = 0; i < 220; i++) {
        snprintf(path, sizeof(path),
                 "/home/user/.claude/projects/-home-user-Projects-bulk/%03d.jsonl",
                 i);
        sessions_ingest_match_for_test(&mode, path,
            "{\"type\":\"user\",\"message\":{\"content\":\"alpha\"}}");
    }

    ASSERT_TRUE("broad query keeps hundreds of sessions",
                mode.filtered_count == 220);
}

static void test_rg_argv_excludes_history_files(void) {
    SessionQuery query;
    sessions_parse_query("alpha", &query);
    char **single = sessions_build_rg_argv_for_test(&query);
    ASSERT_TRUE("single-term rg excludes claude history",
                !argv_contains_substr(single, ".claude/history.jsonl"));
    ASSERT_TRUE("single-term rg excludes codex history",
                !argv_contains_substr(single, ".codex/history.jsonl"));
    g_strfreev(single);

    sessions_parse_query("alpha beta", &query);
    char **multi = sessions_build_rg_argv_for_test(&query);
    ASSERT_TRUE("multi-term rg does not rely on rg max-count",
                !argv_contains(multi, "--max-count"));
    g_strfreev(multi);
}

static void test_ingest_caps_hit_count_per_session(void) {
    SessionsMode mode;
    sessions_init(&mode);
    sessions_parse_query("alpha", &mode.query);

    const char *path = "/home/user/.claude/projects/-home-user-Projects-alpha/abc.jsonl";
    for (int i = 0; i < 55; i++) {
        sessions_ingest_match_for_test(&mode, path,
            "{\"type\":\"user\",\"message\":{\"content\":\"alpha repeated\"}}");
    }

    const SessionResult *result = sessions_result_at(&mode, 0);
    ASSERT_TRUE("capped hit result exists", result != NULL);
    ASSERT_TRUE("hit count is capped", result->hit_count == 40);
    ASSERT_STR("hit text is capped", result->hit_text, "40");
}

static void test_write_claude_name_records_escapes_json(void) {
    char path[] = "/tmp/cofi-session-name-XXXXXX";
    int fd = mkstemp(path);
    ASSERT_TRUE("create temp name session", fd >= 0);
    if (fd < 0) return;
    close(fd);

    ASSERT_TRUE("write claude name records",
                sessions_write_claude_name_records_for_test(
                    path, "abc-123", "Plan \"A\""));

    gchar *content = NULL;
    gsize len = 0;
    ASSERT_TRUE("read temp name session",
                g_file_get_contents(path, &content, &len, NULL));
    ASSERT_TRUE("writes custom title record",
                strstr(content, "\"type\":\"custom-title\"") != NULL);
    ASSERT_TRUE("writes agent name record",
                strstr(content, "\"type\":\"agent-name\"") != NULL);
    ASSERT_TRUE("escapes quotes",
                strstr(content, "Plan \\\"A\\\"") != NULL);
    ASSERT_TRUE("writes session id",
                strstr(content, "\"sessionId\":\"abc-123\"") != NULL);

    g_free(content);
    unlink(path);
}

static void test_write_codex_name_record_escapes_json(void) {
    char path[] = "/tmp/cofi-session-codex-name-XXXXXX";
    int fd = mkstemp(path);
    ASSERT_TRUE("create temp codex name session", fd >= 0);
    if (fd < 0) return;
    close(fd);

    ASSERT_TRUE("write codex name record",
                sessions_write_codex_name_record_for_test(
                    path,
                    "rollout-2026-04-13T14-49-48-019d86e4-179d-7360-9bac-b66fafe6361c",
                    "Plan \"B\""));

    gchar *content = NULL;
    gsize len = 0;
    ASSERT_TRUE("read temp codex name session",
                g_file_get_contents(path, &content, &len, NULL));
    ASSERT_TRUE("writes codex event type",
                strstr(content, "\"type\":\"event_msg\"") != NULL);
    ASSERT_TRUE("writes codex thread name update",
                strstr(content, "\"type\":\"thread_name_updated\"") != NULL);
    ASSERT_TRUE("writes codex thread id",
                strstr(content,
                       "\"thread_id\":\"019d86e4-179d-7360-9bac-b66fafe6361c\"") != NULL);
    ASSERT_TRUE("escapes codex quotes",
                strstr(content, "Plan \\\"B\\\"") != NULL);

    g_free(content);
    unlink(path);
}

static void test_build_claude_resume_command(void) {
    SessionResult result = {0};
    g_strlcpy(result.source, "claude", sizeof(result.source));
    g_strlcpy(result.cwd, "/home/user/Projects/cofi", sizeof(result.cwd));
    g_strlcpy(result.session_id, "abc-123", sizeof(result.session_id));
    char command[SESSION_COMMAND_LEN];

    ASSERT_TRUE("build claude resume command",
                sessions_build_resume_command(&result, command, sizeof(command)));
    ASSERT_STR("claude resume command", command,
               "cd '/home/user/Projects/cofi' && claude --resume 'abc-123'");
}

static void test_resume_command_skips_stale_cwd(void) {
    SessionResult result = {0};
    g_strlcpy(result.source, "claude", sizeof(result.source));
    g_strlcpy(result.cwd, "/tmp/cofi-no-such-cwd-for-session",
              sizeof(result.cwd));
    g_strlcpy(result.session_id, "abc-123", sizeof(result.session_id));
    char command[SESSION_COMMAND_LEN];

    ASSERT_TRUE("build resume with stale cwd",
                sessions_build_resume_command(&result, command, sizeof(command)));
    ASSERT_STR("stale cwd is skipped", command, "claude --resume 'abc-123'");
}

static void test_build_codex_resume_command(void) {
    SessionResult result = {0};
    g_strlcpy(result.source, "codex", sizeof(result.source));
    g_strlcpy(result.cwd, "/home/user/Projects/cofi", sizeof(result.cwd));
    g_strlcpy(result.session_id, "019d3573-7342-7b43-85b0-c999341c4ba5",
              sizeof(result.session_id));
    char command[SESSION_COMMAND_LEN];

    ASSERT_TRUE("build codex resume command",
                sessions_build_resume_command(&result, command, sizeof(command)));
    ASSERT_STR("codex resume command", command,
               "cd '/home/user/Projects/cofi' && codex resume '019d3573-7342-7b43-85b0-c999341c4ba5'");
}

static void test_build_codex_resume_command_uses_rollout_uuid(void) {
    SessionResult result = {0};
    g_strlcpy(result.source, "codex", sizeof(result.source));
    g_strlcpy(result.cwd, "/home/user/Projects/cofi", sizeof(result.cwd));
    g_strlcpy(result.session_id,
              "rollout-2026-04-13T14-49-48-019d86e4-179d-7360-9bac-b66fafe6361c",
              sizeof(result.session_id));
    char command[SESSION_COMMAND_LEN];

    ASSERT_TRUE("build codex rollout resume command",
                sessions_build_resume_command(&result, command, sizeof(command)));
    ASSERT_STR("codex rollout resume command uses uuid", command,
               "cd '/home/user/Projects/cofi' && codex resume '019d86e4-179d-7360-9bac-b66fafe6361c'");
}

static void test_build_codex_resume_command_reads_cwd_from_file(void) {
    char path[] = "/tmp/cofi-session-XXXXXX";
    int fd = mkstemp(path);
    ASSERT_TRUE("create temp codex session", fd >= 0);
    if (fd < 0) return;
    close(fd);
    const char *content =
        "{\"timestamp\":\"2026-03-28T17:17:33.130Z\","
        "\"type\":\"session_meta\","
        "\"payload\":{\"id\":\"019d\",\"cwd\":\"/home/user/Projects/cofi\"}}\n";
    ASSERT_TRUE("write temp codex session",
                g_file_set_contents(path, content, -1, NULL));

    SessionResult result = {0};
    g_strlcpy(result.source, "codex", sizeof(result.source));
    g_strlcpy(result.path, path, sizeof(result.path));
    g_strlcpy(result.session_id, "019d", sizeof(result.session_id));
    char command[SESSION_COMMAND_LEN];

    ASSERT_TRUE("build codex resume command with file cwd",
                sessions_build_resume_command(&result, command, sizeof(command)));
    ASSERT_STR("codex resume command uses file cwd", command,
               "cd '/home/user/Projects/cofi' && codex resume '019d'");
    unlink(path);
}

static void test_launch_result_uses_terminal_launcher(void) {
    g_launch_calls = 0;
    g_last_launch_command[0] = '\0';
    sessions_set_launch_impl_for_test(detach_launch_in_terminal_cmd);

    SessionResult result = {0};
    g_strlcpy(result.source, "claude", sizeof(result.source));
    g_strlcpy(result.cwd, "/home/user/Projects/cofi", sizeof(result.cwd));
    g_strlcpy(result.session_id, "abc", sizeof(result.session_id));

    ASSERT_TRUE("launch result succeeds", sessions_launch_result(&result));
    ASSERT_TRUE("terminal launcher called", g_launch_calls == 1);
    ASSERT_STR("terminal launcher command", g_last_launch_command,
               "cd '/home/user/Projects/cofi' && claude --resume 'abc'");
    sessions_set_launch_impl_for_test(NULL);
}

static void test_delete_result_rejects_unknown_path(void) {
    char path[] = "/tmp/cofi-session-XXXXXX.jsonl";
    int fd = mkstemps(path, 6);
    ASSERT_TRUE("create temp guarded delete session", fd >= 0);
    if (fd < 0) return;
    close(fd);
    ASSERT_TRUE("write temp guarded delete session",
                g_file_set_contents(path, "{\"type\":\"x\"}\n", -1, NULL));

    SessionResult result = {0};
    g_strlcpy(result.source, "codex", sizeof(result.source));
    g_strlcpy(result.path, path, sizeof(result.path));

    ASSERT_TRUE("plain temp path is not deleted",
                !sessions_delete_result(&result));
    ASSERT_TRUE("plain temp file still exists",
                g_file_test(path, G_FILE_TEST_EXISTS));
    unlink(path);
}

static void test_remove_path_updates_visible_results(void) {
    SessionsMode mode;
    sessions_init(&mode);
    sessions_parse_query("marco", &mode.query);

    sessions_ingest_match_for_test(&mode,
        "/home/user/.claude/projects/-home-user-Projects-marco/abc.jsonl",
        "{\"type\":\"user\",\"message\":{\"content\":\"marco alpha\"}}");
    sessions_ingest_match_for_test(&mode,
        "/home/user/.claude/projects/-home-user-Projects-marco/def.jsonl",
        "{\"type\":\"user\",\"message\":{\"content\":\"marco beta\"}}");

    ASSERT_TRUE("two visible before remove", mode.filtered_count == 2);
    sessions_remove_path(&mode,
        "/home/user/.claude/projects/-home-user-Projects-marco/abc.jsonl");
    ASSERT_TRUE("one visible after remove", mode.filtered_count == 1);
    const SessionResult *result = sessions_result_at(&mode, 0);
    ASSERT_STR("remaining result", result->session_id, "def");
}

int main(void) {
    test_query_split();
    test_extract_claude_message_text();
    test_extract_codex_payload_text();
    test_extract_claude_string_array_text();
    test_extract_claude_session_name_metadata();
    test_extract_codex_session_name_metadata();
    test_group_requires_all_terms_across_lines();
    test_project_label_uses_basename_outside_projects();
    test_claude_subagent_hits_collapse_to_parent_session();
    test_group_records_latest_claude_session_name();
    test_vimgrep_path_allows_colons();
    test_refine_filters_grouped_sessions();
    test_raw_json_metadata_is_not_display_snippet();
    test_metadata_match_sorts_before_content_only_match();
    test_seed_file_surfaces_path_match_without_content_hit();
    test_metadata_match_gets_later_extracted_snippet();
    test_broad_query_can_hold_hundreds_of_sessions();
    test_rg_argv_excludes_history_files();
    test_ingest_caps_hit_count_per_session();
    test_write_claude_name_records_escapes_json();
    test_write_codex_name_record_escapes_json();
    test_build_claude_resume_command();
    test_resume_command_skips_stale_cwd();
    test_build_codex_resume_command();
    test_build_codex_resume_command_uses_rollout_uuid();
    test_build_codex_resume_command_reads_cwd_from_file();
    test_launch_result_uses_terminal_launcher();
    test_delete_result_rejects_unknown_path();
    test_remove_path_updates_visible_results();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
