#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/cofi_tab_provider.h"
#include "../src/command_registry.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(msg, cond) \
    do { \
        tests_run++; \
        if (cond) { \
            tests_passed++; \
            printf("PASS: %s\n", msg); \
        } else { \
            printf("FAIL: %s (line %d)\n", msg, __LINE__); \
        } \
    } while (0)

#define TEST_AGENT_SESSIONS_TAB ((TabMode)(TAB_COUNT + 1))

static int g_reset_selection_calls;
static int g_update_display_calls;
static int g_update_scroll_calls;
static int g_validate_selection_calls;
static int g_exit_command_mode_calls;
static int g_surface_tab_calls;
static int g_delete_overlay_calls;
static int g_registered_provider_id = -1;
static char g_last_delete_path[AGENT_SESSION_PATH_LEN];
static CofiTabProvider g_registered_provider;
static CommandSpec g_registered_command;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

void reset_selection(AppData *app) {
    (void)app;
    g_reset_selection_calls++;
}

void validate_selection(AppData *app) {
    (void)app;
    g_validate_selection_calls++;
}

void update_scroll_position(AppData *app) {
    (void)app;
    g_update_scroll_calls++;
}

void update_display(AppData *app) {
    (void)app;
    g_update_display_calls++;
}

void exit_command_mode(AppData *app) {
    (void)app;
    g_exit_command_mode_calls++;
}

void surface_tab(AppData *app, TabMode tab) {
    g_surface_tab_calls++;
    if (app) app->current_tab = tab;
}

void show_agent_session_delete_overlay(AppData *app,
                                       const char *source,
                                       const char *session_id,
                                       const char *path) {
    (void)app;
    (void)source;
    (void)session_id;
    g_delete_overlay_calls++;
    g_strlcpy(g_last_delete_path, path ? path : "", sizeof(g_last_delete_path));
}

gboolean detach_launch_in_terminal_cmd(const char *command) {
    (void)command;
    return TRUE;
}

void cofi_init_provider_defaults(CofiTabProvider *p) {
    if (p) memset(p, 0, sizeof(*p));
}

int cofi_register_tab_provider(const CofiTabProvider *p) {
    memset(&g_registered_provider, 0, sizeof(g_registered_provider));
    if (p) g_registered_provider = *p;
    if (g_registered_provider.tab_mode == COFI_PROVIDER_DYNAMIC_TAB) {
        g_registered_provider.tab_mode = TEST_AGENT_SESSIONS_TAB;
    }
    g_registered_provider_id = 0;
    return g_registered_provider_id;
}

const CofiTabProvider *cofi_get_provider(int provider_id) {
    return provider_id == g_registered_provider_id ? &g_registered_provider : NULL;
}

int cofi_register_command(const CommandSpec *spec) {
    if (!spec) return -1;
    g_registered_command = *spec;
    return 0;
}

#include "../src/agent_sessions.c"
#include "../src/agent_sessions_provider.c"

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    agent_sessions_init(&s_agent_sessions_mode);
    s_agent_sessions_provider_id = -1;
    g_reset_selection_calls = 0;
    g_update_display_calls = 0;
    g_update_scroll_calls = 0;
    g_validate_selection_calls = 0;
    g_exit_command_mode_calls = 0;
    g_surface_tab_calls = 0;
    g_delete_overlay_calls = 0;
    g_registered_provider_id = -1;
    g_last_delete_path[0] = '\0';
    memset(&g_registered_provider, 0, sizeof(g_registered_provider));
    memset(&g_registered_command, 0, sizeof(g_registered_command));
}

static void seed_agent_result(void) {
    agent_sessions_parse_query("marco", &s_agent_sessions_mode.query);
    agent_sessions_ingest_match_for_test(
        &s_agent_sessions_mode,
        "/home/user/.claude/projects/-home-user-Projects-marco/abc.jsonl",
        "{\"type\":\"user\",\"message\":{\"content\":\"marco alpha\"}}");
}

static void test_registers_metadata(void) {
    AppData app;
    reset_state(&app);

    agent_sessions_provider_register();

    ASSERT_TRUE("provider registered", g_registered_provider_id == 0);
    ASSERT_TRUE("provider gets dynamic tab",
                g_registered_provider.tab_mode == TEST_AGENT_SESSIONS_TAB);
    ASSERT_TRUE("provider hidden by default", g_registered_provider.hidden_by_default == 1);
    ASSERT_TRUE("provider primary command",
                strcmp(g_registered_command.primary, "agent-sessions") == 0);
    ASSERT_TRUE("provider command alias",
                strcmp(g_registered_command.aliases[0], "agents") == 0);
}

static void test_command_surfaces_tab(void) {
    AppData app;
    reset_state(&app);
    app.current_tab = TAB_WINDOWS;
    agent_sessions_provider_register();

    g_registered_command.handler(&app, NULL, NULL);

    ASSERT_TRUE("command exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("command records origin tab", app.prefix_origin_tab == TAB_WINDOWS);
    ASSERT_TRUE("command surfaces dynamic tab", app.current_tab == TEST_AGENT_SESSIONS_TAB);
    ASSERT_TRUE("surface called once", g_surface_tab_calls == 1);
}

static void test_delete_key_opens_overlay(void) {
    AppData app;
    reset_state(&app);
    agent_sessions_provider_register();
    app.current_tab = TEST_AGENT_SESSIONS_TAB;
    app.selection.provider_index = 0;
    seed_agent_result();

    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = GDK_KEY_Delete;
    gboolean handled = g_registered_provider.handle_key(&event, &app);

    ASSERT_TRUE("Delete opens overlay", handled == TRUE);
    ASSERT_TRUE("delete overlay called", g_delete_overlay_calls == 1);
    ASSERT_TRUE("overlay receives session path",
                strcmp(g_last_delete_path,
                       "/home/user/.claude/projects/-home-user-Projects-marco/abc.jsonl") == 0);
}

static void test_status_row_does_not_open_overlay(void) {
    AppData app;
    reset_state(&app);
    agent_sessions_provider_register();
    app.current_tab = TEST_AGENT_SESSIONS_TAB;

    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = GDK_KEY_Delete;
    gboolean handled = g_registered_provider.handle_key(&event, &app);

    ASSERT_TRUE("status row delete ignored", handled == FALSE);
    ASSERT_TRUE("delete overlay not called", g_delete_overlay_calls == 0);
}

static void test_remove_path_refreshes_provider_surface(void) {
    AppData app;
    reset_state(&app);
    agent_sessions_provider_register();
    seed_agent_result();

    agent_sessions_provider_remove_path(
        &app, "/home/user/.claude/projects/-home-user-Projects-marco/abc.jsonl");

    ASSERT_TRUE("path removed from results", s_agent_sessions_mode.filtered_count == 0);
    ASSERT_TRUE("selection validated", g_validate_selection_calls == 1);
    ASSERT_TRUE("scroll updated", g_update_scroll_calls == 1);
    ASSERT_TRUE("display updated", g_update_display_calls == 1);
}

int main(void) {
    printf("Agent sessions provider tests\n");
    printf("=============================\n\n");

    test_registers_metadata();
    test_command_surfaces_tab();
    test_delete_key_opens_overlay();
    test_status_row_does_not_open_overlay();
    test_remove_path_refreshes_provider_surface();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
