#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../src/agent_sessions.h"

static int pass = 0;
static int fail = 0;
static int g_launch_calls = 0;
static char g_last_launch_command[AGENT_SESSION_COMMAND_LEN];

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

static void test_query_split(void) {
    AgentSessionQuery query;
    int terms = agent_sessions_parse_query(" marco unicode title | restart patch ", &query);

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
    char text[AGENT_SESSION_TEXT_LEN];

    ASSERT_TRUE("extract claude message",
                agent_sessions_extract_json_text(line, text, sizeof(text)));
    ASSERT_STR("claude text", text, "can you check marco unicode titles?");
}

static void test_extract_codex_payload_text(void) {
    const char *line =
        "{\"type\":\"response_item\",\"payload\":{\"type\":\"message\","
        "\"role\":\"user\",\"content\":[{\"type\":\"input_text\","
        "\"text\":\"give me session hits for marco\"}]}}";
    char text[AGENT_SESSION_TEXT_LEN];

    ASSERT_TRUE("extract codex message",
                agent_sessions_extract_json_text(line, text, sizeof(text)));
    ASSERT_STR("codex text", text, "give me session hits for marco");
}

static void test_group_requires_all_terms_across_lines(void) {
    AgentSessionsMode mode;
    agent_sessions_init(&mode);
    agent_sessions_parse_query("marco unicode | restart", &mode.query);

    const char *path = "/home/user/.claude/projects/-home-user-Projects-marco/abc.jsonl";
    agent_sessions_ingest_match_for_test(&mode, path,
        "{\"type\":\"user\",\"message\":{\"content\":\"Marco window manager\"}}");
    ASSERT_TRUE("single term not visible yet", mode.filtered_count == 0);

    agent_sessions_ingest_match_for_test(&mode, path,
        "{\"type\":\"assistant\",\"message\":{\"content\":\"unicode title fix restart\"}}");
    ASSERT_TRUE("all terms visible after second line", mode.filtered_count == 1);

    const AgentSessionResult *result = agent_sessions_result_at(&mode, 0);
    ASSERT_TRUE("result exists", result != NULL);
    ASSERT_STR("source parsed", result->source, "claude");
    ASSERT_STR("project parsed", result->project, "/home/user/Projects/marco");
    ASSERT_STR("cwd falls back to project path", result->cwd, "/home/user/Projects/marco");
    ASSERT_STR("session id parsed", result->session_id, "abc");
    ASSERT_TRUE("hit count grouped", result && result->hit_count == 2);
}

static void test_refine_filters_grouped_sessions(void) {
    AgentSessionsMode mode;
    agent_sessions_init(&mode);
    agent_sessions_parse_query("marco", &mode.query);

    agent_sessions_ingest_match_for_test(&mode,
        "/home/user/.claude/projects/-home-user-Projects-marco/abc.jsonl",
        "{\"type\":\"user\",\"message\":{\"content\":\"patched marco restart\"}}");
    agent_sessions_ingest_match_for_test(&mode,
        "/home/user/.claude/projects/-home-user-Projects-marco/def.jsonl",
        "{\"type\":\"user\",\"message\":{\"content\":\"marco title colors\"}}");

    ASSERT_TRUE("two sessions before refine", mode.filtered_count == 2);
    agent_sessions_apply_refine(&mode, "restart");
    ASSERT_TRUE("one session after refine", mode.filtered_count == 1);
    const AgentSessionResult *result = agent_sessions_result_at(&mode, 0);
    ASSERT_STR("restart result", result->session_id, "abc");
}

static void test_build_claude_resume_command(void) {
    AgentSessionResult result = {0};
    g_strlcpy(result.source, "claude", sizeof(result.source));
    g_strlcpy(result.cwd, "/home/user/Projects/marco", sizeof(result.cwd));
    g_strlcpy(result.session_id, "abc-123", sizeof(result.session_id));
    char command[AGENT_SESSION_COMMAND_LEN];

    ASSERT_TRUE("build claude resume command",
                agent_sessions_build_resume_command(&result, command, sizeof(command)));
    ASSERT_STR("claude resume command", command,
               "cd '/home/user/Projects/marco' && claude --resume 'abc-123'");
}

static void test_build_codex_resume_command(void) {
    AgentSessionResult result = {0};
    g_strlcpy(result.source, "codex", sizeof(result.source));
    g_strlcpy(result.cwd, "/home/user/Projects/cofi", sizeof(result.cwd));
    g_strlcpy(result.session_id, "019d3573-7342-7b43-85b0-c999341c4ba5",
              sizeof(result.session_id));
    char command[AGENT_SESSION_COMMAND_LEN];

    ASSERT_TRUE("build codex resume command",
                agent_sessions_build_resume_command(&result, command, sizeof(command)));
    ASSERT_STR("codex resume command", command,
               "cd '/home/user/Projects/cofi' && codex resume '019d3573-7342-7b43-85b0-c999341c4ba5'");
}

static void test_build_codex_resume_command_reads_cwd_from_file(void) {
    char path[] = "/tmp/cofi-agent-session-XXXXXX";
    int fd = mkstemp(path);
    ASSERT_TRUE("create temp codex session", fd >= 0);
    if (fd < 0) return;
    close(fd);
    const char *content =
        "{\"timestamp\":\"2026-03-28T17:17:33.130Z\","
        "\"type\":\"session_meta\","
        "\"payload\":{\"id\":\"019d\",\"cwd\":\"/home/user/Projects/cbtrdr.gsd\"}}\n";
    ASSERT_TRUE("write temp codex session",
                g_file_set_contents(path, content, -1, NULL));

    AgentSessionResult result = {0};
    g_strlcpy(result.source, "codex", sizeof(result.source));
    g_strlcpy(result.path, path, sizeof(result.path));
    g_strlcpy(result.session_id, "019d", sizeof(result.session_id));
    char command[AGENT_SESSION_COMMAND_LEN];

    ASSERT_TRUE("build codex resume command with file cwd",
                agent_sessions_build_resume_command(&result, command, sizeof(command)));
    ASSERT_STR("codex resume command uses file cwd", command,
               "cd '/home/user/Projects/cbtrdr.gsd' && codex resume '019d'");
    unlink(path);
}

static void test_launch_result_uses_terminal_launcher(void) {
    g_launch_calls = 0;
    g_last_launch_command[0] = '\0';
    agent_sessions_set_launch_impl_for_test(detach_launch_in_terminal_cmd);

    AgentSessionResult result = {0};
    g_strlcpy(result.source, "claude", sizeof(result.source));
    g_strlcpy(result.cwd, "/home/user/Projects/cofi", sizeof(result.cwd));
    g_strlcpy(result.session_id, "abc", sizeof(result.session_id));

    ASSERT_TRUE("launch result succeeds", agent_sessions_launch_result(&result));
    ASSERT_TRUE("terminal launcher called", g_launch_calls == 1);
    ASSERT_STR("terminal launcher command", g_last_launch_command,
               "cd '/home/user/Projects/cofi' && claude --resume 'abc'");
    agent_sessions_set_launch_impl_for_test(NULL);
}

static void test_delete_result_rejects_unknown_path(void) {
    char path[] = "/tmp/cofi-agent-session-XXXXXX.jsonl";
    int fd = mkstemps(path, 6);
    ASSERT_TRUE("create temp guarded delete session", fd >= 0);
    if (fd < 0) return;
    close(fd);
    ASSERT_TRUE("write temp guarded delete session",
                g_file_set_contents(path, "{\"type\":\"x\"}\n", -1, NULL));

    AgentSessionResult result = {0};
    g_strlcpy(result.source, "codex", sizeof(result.source));
    g_strlcpy(result.path, path, sizeof(result.path));

    ASSERT_TRUE("plain temp path is not deleted",
                !agent_sessions_delete_result(&result));
    ASSERT_TRUE("plain temp file still exists",
                g_file_test(path, G_FILE_TEST_EXISTS));
    unlink(path);
}

static void test_remove_path_updates_visible_results(void) {
    AgentSessionsMode mode;
    agent_sessions_init(&mode);
    agent_sessions_parse_query("marco", &mode.query);

    agent_sessions_ingest_match_for_test(&mode,
        "/home/user/.claude/projects/-home-user-Projects-marco/abc.jsonl",
        "{\"type\":\"user\",\"message\":{\"content\":\"marco alpha\"}}");
    agent_sessions_ingest_match_for_test(&mode,
        "/home/user/.claude/projects/-home-user-Projects-marco/def.jsonl",
        "{\"type\":\"user\",\"message\":{\"content\":\"marco beta\"}}");

    ASSERT_TRUE("two visible before remove", mode.filtered_count == 2);
    agent_sessions_remove_path(&mode,
        "/home/user/.claude/projects/-home-user-Projects-marco/abc.jsonl");
    ASSERT_TRUE("one visible after remove", mode.filtered_count == 1);
    const AgentSessionResult *result = agent_sessions_result_at(&mode, 0);
    ASSERT_STR("remaining result", result->session_id, "def");
}

int main(void) {
    test_query_split();
    test_extract_claude_message_text();
    test_extract_codex_payload_text();
    test_group_requires_all_terms_across_lines();
    test_refine_filters_grouped_sessions();
    test_build_claude_resume_command();
    test_build_codex_resume_command();
    test_build_codex_resume_command_reads_cwd_from_file();
    test_launch_result_uses_terminal_launcher();
    test_delete_result_rejects_unknown_path();
    test_remove_path_updates_visible_results();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
