#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "core/app/app_data.h"
#include "commands/command_registry.h"

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

static int g_apps_load_calls;
static int g_apps_filter_calls;
static int g_apps_launch_calls;
static int g_reset_selection_calls;
static int g_exit_command_mode_calls;
static int g_surface_tab_calls;
static TabMode g_last_surface_tab;
static char g_last_filter_query[64];
static const AppEntry *g_last_launched_app;
static CofiTabProvider g_registered_provider;
static int g_registered_provider_id;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

void apps_load(void) {
    g_apps_load_calls++;
}

void apps_filter(const char *query, AppEntry *out, int *out_count) {
    g_apps_filter_calls++;
    g_strlcpy(g_last_filter_query, query ? query : "", sizeof(g_last_filter_query));
    if (out_count) *out_count = 0;
    (void)out;
}

void apps_launch(const AppEntry *entry) {
    g_apps_launch_calls++;
    g_last_launched_app = entry;
}

void reset_selection(AppData *app) {
    (void)app;
    g_reset_selection_calls++;
}

void exit_command_mode(AppData *app) {
    (void)app;
    g_exit_command_mode_calls++;
}

void surface_tab(AppData *app, TabMode tab) {
    (void)app;
    g_surface_tab_calls++;
    g_last_surface_tab = tab;
}

void cofi_init_provider_defaults(CofiTabProvider *p) {
    if (p) memset(p, 0, sizeof(*p));
}

int cofi_register_tab_provider(const CofiTabProvider *p) {
    if (!p) return -1;
    g_registered_provider = *p;
    g_registered_provider.tab_mode = TAB_COUNT + 1;
    g_registered_provider_id = 0;
    return g_registered_provider_id;
}

const CofiTabProvider *cofi_get_provider(int provider_id) {
    return provider_id == g_registered_provider_id ? &g_registered_provider : NULL;
}

int cofi_register_command(const CommandSpec *spec) {
    return spec ? 0 : -1;
}

#include "apps/apps_provider.c"

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    g_apps_load_calls = 0;
    g_apps_filter_calls = 0;
    g_apps_launch_calls = 0;
    g_reset_selection_calls = 0;
    g_exit_command_mode_calls = 0;
    g_surface_tab_calls = 0;
    g_last_surface_tab = TAB_WINDOWS;
    g_last_filter_query[0] = '\0';
    g_last_launched_app = NULL;
    memset(&g_registered_provider, 0, sizeof(g_registered_provider));
    g_registered_provider_id = -1;
}

static void test_no_match_row(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);

    ASSERT_TRUE("no-match row count is one", apps_row_count(&app) == 1);

    memset(&row, 0, sizeof(row));
    apps_format_row(&app, 0, &row);
    ASSERT_TRUE("no-match text", strcmp(row.cells[0].text, "No matching applications found") == 0);
    ASSERT_TRUE("no-match is not actionable", row.row_flags == 0);
}

static void test_enter_launches_real_row_only(void) {
    AppData app;
    reset_state(&app);
    app.filtered_apps_count = 1;
    g_strlcpy(app.filtered_apps[0].name, "Firefox", sizeof(app.filtered_apps[0].name));

    CofiActionStatus status = apps_on_enter_pressed(&app, 0, 0, "", 0);
    ASSERT_TRUE("real row hides after launch", status == COFI_HANDLED_HIDE);
    ASSERT_TRUE("real row launched", g_apps_launch_calls == 1 &&
                g_last_launched_app == &app.filtered_apps[0]);

    status = apps_on_enter_pressed(&app, 0, 1, "", 0);
    ASSERT_TRUE("status row no-op", status == COFI_NO_OP);
}

static void test_query_uses_apps_filter(void) {
    AppData app;
    reset_state(&app);

    apps_on_query_changed(&app, "fire");
    ASSERT_TRUE("default mode uses apps_filter", g_apps_filter_calls == 1 &&
                strcmp(g_last_filter_query, "fire") == 0);
    ASSERT_TRUE("default query resets selection", g_reset_selection_calls == 1);
}

static void test_command_metadata(void) {
    AppData app;
    reset_state(&app);
    apps_provider_register();

    ASSERT_TRUE("apps provider uses dynamic tab", g_registered_provider.tab_mode >= TAB_COUNT);
    ASSERT_TRUE("apps command primary",
                strcmp(s_apps_command.primary, "apps") == 0);
    ASSERT_TRUE("apps alias applications",
                strcmp(s_apps_command.aliases[0], "applications") == 0);
    ASSERT_TRUE("apps alias app",
                strcmp(s_apps_command.aliases[1], "app") == 0);
    ASSERT_TRUE("apps command help",
                strcmp(s_apps_command.help_format,
                       "apps, app, applications") == 0);
    ASSERT_TRUE("apps command handler set", s_apps_command.handler != NULL);
    ASSERT_TRUE("apps command keep-open policy",
                s_apps_command.keeps_open_on_hotkey_auto == 1);
    ASSERT_TRUE("apps owns tab prefixes",
                strcmp(s_apps_provider.tab_prefix_chars, "\\") == 0);
}

static void test_command_handler_surfaces_tab(void) {
    AppData app;
    reset_state(&app);
    apps_provider_register();
    app.current_tab = TAB_WINDOWS;

    gboolean result = s_apps_command.handler(&app, NULL, "");

    ASSERT_TRUE("apps command returns false", result == FALSE);
    ASSERT_TRUE("apps command exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("apps command surfaces Apps tab",
                g_surface_tab_calls == 1 && g_last_surface_tab == (TabMode)g_registered_provider.tab_mode);
    ASSERT_TRUE("apps command records origin", app.prefix_origin_tab == TAB_WINDOWS);
}

int main(void) {
    printf("Apps provider tests\n");
    printf("===================\n\n");

    test_no_match_row();
    test_enter_launches_real_row_only();
    test_query_uses_apps_filter();
    test_command_metadata();
    test_command_handler_surfaces_tab();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
