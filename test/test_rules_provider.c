#include <stdio.h>
#include <stdarg.h>
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

static int g_reset_selection_calls;
static int g_exit_command_mode_calls;
static int g_surface_tab_calls;
static TabMode g_last_surface_tab = -1;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

int has_match(const char *needle, const char *haystack) {
    return !needle || needle[0] == '\0' || strstr(haystack, needle) != NULL;
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
    if (app) app->current_tab = tab;
    g_surface_tab_calls++;
    g_last_surface_tab = tab;
}

void cofi_init_provider_defaults(CofiTabProvider *p) {
    if (p) memset(p, 0, sizeof(*p));
}

int cofi_register_tab_provider(const CofiTabProvider *p) {
    (void)p;
    return 0;
}

int cofi_register_command(const CommandSpec *spec) {
    return spec ? 0 : -1;
}

void show_overlay(AppData *app, OverlayType type, void *data) {
    (void)app; (void)type; (void)data;
}
int replay_all_rules_against_open_windows(AppData *app) {
    (void)app;
    return 0;
}
gboolean replay_selected_filtered_rule(AppData *app) {
    (void)app;
    return TRUE;
}

#include "../src/rules_provider.c"

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    g_reset_selection_calls = 0;
    g_exit_command_mode_calls = 0;
    g_surface_tab_calls = 0;
    g_last_surface_tab = -1;
}

static void seed_rules(AppData *app) {
    app->rules_config.count = 2;
    g_strlcpy(app->rules_config.rules[0].pattern, "*term*",
              sizeof(app->rules_config.rules[0].pattern));
    g_strlcpy(app->rules_config.rules[0].commands, "sb on",
              sizeof(app->rules_config.rules[0].commands));
    g_strlcpy(app->rules_config.rules[1].pattern, "*firefox*",
              sizeof(app->rules_config.rules[1].pattern));
    g_strlcpy(app->rules_config.rules[1].commands, "ew off",
              sizeof(app->rules_config.rules[1].commands));
}

static void test_filter_and_format_row(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);
    seed_rules(&app);

    filter_rules(&app, "fire");

    ASSERT_TRUE("filter narrows rules", app.filtered_rules_count == 1);
    ASSERT_TRUE("filter keeps matching rule",
                strcmp(app.filtered_rules[0].pattern, "*firefox*") == 0);

    memset(&row, 0, sizeof(row));
    rules_format_row(&app, 0, &row);
    ASSERT_TRUE("row has two cells", row.cell_count == 2);
    ASSERT_TRUE("row pattern text", strcmp(row.cells[0].text, "*firefox*") == 0);
    ASSERT_TRUE("row command text", strcmp(row.cells[1].text, "ew off") == 0);
    ASSERT_TRUE("row is actionable", row.row_flags == COFI_ROW_ACTIONABLE);
}

static void test_empty_row(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);

    ASSERT_TRUE("empty provider exposes one status row", rules_row_count(&app) == 1);

    memset(&row, 0, sizeof(row));
    rules_format_row(&app, 0, &row);
    ASSERT_TRUE("empty row text", strcmp(row.cells[0].text, "No rules found") == 0);
    ASSERT_TRUE("empty row not actionable", row.row_flags == 0);
}

static void test_query_resets_selection(void) {
    AppData app;
    reset_state(&app);
    seed_rules(&app);

    rules_on_query_changed(&app, "sb");

    ASSERT_TRUE("query filters", app.filtered_rules_count == 1);
    ASSERT_TRUE("query resets selection", g_reset_selection_calls == 1);
}

static void test_on_enter_filters_all_rules(void) {
    AppData app;
    reset_state(&app);
    seed_rules(&app);

    rules_on_enter(&app);

    ASSERT_TRUE("on enter filters rules", app.filtered_rules_count == 2);
    ASSERT_TRUE("on enter keeps first rule",
                strcmp(app.filtered_rules[0].pattern, "*term*") == 0);
}

static void test_selected_rule_and_config_index(void) {
    AppData app;
    reset_state(&app);
    seed_rules(&app);
    filter_rules(&app, "");
    app.selection.provider_index = 99;

    Rule *rule = rules_selected_rule(&app);

    ASSERT_TRUE("selected rule clamps to last row", rule != NULL &&
                strcmp(rule->pattern, "*firefox*") == 0);
    ASSERT_TRUE("provider index clamped", app.selection.provider_index == 1);
    ASSERT_TRUE("config index resolved", rules_selected_config_index(&app) == 1);

    rules_select_config_index(&app, 0);
    ASSERT_TRUE("select config index sets provider index", app.selection.provider_index == 0);
}

static void test_command_metadata(void) {
    rules_provider_register();

    ASSERT_TRUE("provider primary command is rules",
                strcmp(s_rules_command.primary, "rules") == 0);
    ASSERT_TRUE("provider alias is rl",
                strcmp(s_rules_command.aliases[0], "rl") == 0);
    ASSERT_TRUE("provider command has help",
                strcmp(s_rules_command.help_format, "rules, rl") == 0);
    ASSERT_TRUE("provider command keeps open",
                s_rules_command.keeps_open_on_hotkey_auto == 1);
    ASSERT_TRUE("provider command handler set", s_rules_command.handler != NULL);
}

static void test_command_handler_surfaces_tab(void) {
    AppData app;
    reset_state(&app);
    app.current_tab = TAB_WINDOWS;

    gboolean result = s_rules_command.handler(&app, NULL, NULL);

    ASSERT_TRUE("rules command returns false", result == FALSE);
    ASSERT_TRUE("rules command exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("rules command records origin tab", app.prefix_origin_tab == TAB_WINDOWS);
    ASSERT_TRUE("rules command surfaces rules tab", g_surface_tab_calls == 1 &&
                g_last_surface_tab == TAB_RULES && app.current_tab == TAB_RULES);
}

int main(void) {
    printf("Rules provider tests\n");
    printf("====================\n\n");

    test_filter_and_format_row();
    test_empty_row();
    test_query_resets_selection();
    test_on_enter_filters_all_rules();
    test_selected_rule_and_config_index();
    test_command_metadata();
    test_command_handler_surfaces_tab();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
