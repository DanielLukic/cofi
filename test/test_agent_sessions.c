#include <stdio.h>
#include <string.h>

#include "../src/agent_sessions.h"

static int pass = 0;
static int fail = 0;

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

int main(void) {
    test_query_split();
    test_extract_claude_message_text();
    test_extract_codex_payload_text();
    test_group_requires_all_terms_across_lines();
    test_refine_filters_grouped_sessions();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
