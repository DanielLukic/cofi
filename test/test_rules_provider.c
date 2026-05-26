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
static CofiTabProvider g_registered_provider;
static int g_show_pattern_overlay_calls;
static int g_selected_pattern_match_id;
static char g_last_pattern_context[128];

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
    if (!p) return -1;
    g_registered_provider = *p;
    g_registered_provider.tab_mode = (TabMode)(TAB_COUNT + 1);
    return 0;
}

const CofiTabProvider *cofi_get_provider(int provider_id) {
    return provider_id == 0 ? &g_registered_provider : NULL;
}

int cofi_register_command(const CommandSpec *spec) {
    return spec ? 0 : -1;
}
int selected_match_id_for_pattern_edit(AppData *app) {
    (void)app;
    return g_selected_pattern_match_id;
}
gboolean show_pattern_edit_overlay(AppData *app, int match_id, const char *context_line) {
    (void)app;
    if (match_id <= 0) return FALSE;
    g_show_pattern_overlay_calls++;
    g_strlcpy(g_last_pattern_context, context_line ? context_line : "",
              sizeof(g_last_pattern_context));
    return TRUE;
}

int match_entry_find_index_by_match_id(const MatchEntryManager *manager, int match_id) {
    if (!manager || match_id <= 0) return -1;
    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].match_id == match_id) {
            return i;
        }
    }
    return -1;
}

void match_entry_manager_init(MatchEntryManager *manager) {
    if (!manager) return;
    memset(manager, 0, sizeof(*manager));
    manager->next_match_id = 1;
}

void show_overlay(AppData *app, OverlayType type, void *data) {
    (void)app; (void)type; (void)data;
}
void show_rule_delete_overlay(AppData *app, int rule_index) {
    (void)app;
    (void)rule_index;
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
    match_entry_manager_init(&app->matching);
    g_reset_selection_calls = 0;
    g_exit_command_mode_calls = 0;
    g_surface_tab_calls = 0;
    g_last_surface_tab = -1;
    g_show_pattern_overlay_calls = 0;
    g_selected_pattern_match_id = 0;
    g_last_pattern_context[0] = '\0';
    memset(&g_registered_provider, 0, sizeof(g_registered_provider));
}

static void seed_rules(AppData *app) {
    app->matching.count = 2;
    app->matching.entries[0].match_id = 101;
    g_strlcpy(app->matching.entries[0].original_title, "*term*",
              sizeof(app->matching.entries[0].original_title));
    app->matching.entries[1].match_id = 202;
    g_strlcpy(app->matching.entries[1].original_title, "*firefox*",
              sizeof(app->matching.entries[1].original_title));

    app->rules_config.count = 2;
    g_strlcpy(app->rules_config.rules[0].pattern, "*term*",
              sizeof(app->rules_config.rules[0].pattern));
    g_strlcpy(app->rules_config.rules[0].commands, "sb on",
              sizeof(app->rules_config.rules[0].commands));
    app->rules_config.rules[0].match_id = 101;
    g_strlcpy(app->rules_config.rules[1].pattern, "*firefox*",
              sizeof(app->rules_config.rules[1].pattern));
    g_strlcpy(app->rules_config.rules[1].commands, "ew off",
              sizeof(app->rules_config.rules[1].commands));
    app->rules_config.rules[1].match_id = 202;
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

static void test_filter_hides_tagged_rules_by_default(void) {
    AppData app;
    reset_state(&app);
    seed_rules(&app);
    g_strlcpy(app.rules_config.rules[1].tag, "geom", sizeof(app.rules_config.rules[1].tag));
    app.config.rules_show_all_tags = 0;

    filter_rules(&app, "");

    ASSERT_TRUE("default filter hides tagged rule", app.filtered_rules_count == 1);
    ASSERT_TRUE("untagged rule remains visible",
                strcmp(app.filtered_rules[0].pattern, "*term*") == 0);
}

static void test_filter_shows_tagged_rules_when_enabled(void) {
    AppData app;
    reset_state(&app);
    seed_rules(&app);
    g_strlcpy(app.rules_config.rules[1].tag, "geom", sizeof(app.rules_config.rules[1].tag));
    app.config.rules_show_all_tags = 1;

    filter_rules(&app, "");

    ASSERT_TRUE("show_all_tags includes tagged rule", app.filtered_rules_count == 2);
}

static void test_search_still_hides_tagged_rules_when_toggle_off(void) {
    AppData app;
    reset_state(&app);
    seed_rules(&app);
    g_strlcpy(app.rules_config.rules[1].tag, "geom", sizeof(app.rules_config.rules[1].tag));
    app.config.rules_show_all_tags = 0;

    filter_rules(&app, "fire");

    ASSERT_TRUE("search branch still hides tagged rule", app.filtered_rules_count == 0);
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

static void test_orphan_rule_row_fallback_indicator(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);
    seed_rules(&app);
    app.rules_config.rules[1].match_id = 9999;
    filter_rules(&app, "fire");

    memset(&row, 0, sizeof(row));
    rules_format_row(&app, 0, &row);
    ASSERT_TRUE("orphan row marks fallback", strcmp(row.cells[0].text, "*firefox* (orphan)") == 0);
}

static void test_command_metadata(void) {
    rules_provider_register();

    ASSERT_TRUE("provider primary command is rules",
                strcmp(s_rules_command.primary, "rules") == 0);
    ASSERT_TRUE("provider alias is rs",
                strcmp(s_rules_command.aliases[0], "rs") == 0);
    ASSERT_TRUE("provider command has help",
                strcmp(s_rules_command.help_format, "rules, rs") == 0);
    ASSERT_TRUE("provider command keeps open",
                s_rules_command.keeps_open_on_hotkey_auto == 1);
    ASSERT_TRUE("provider command handler set", s_rules_command.handler != NULL);
    ASSERT_TRUE("rules provider tab is dynamic",
                g_registered_provider.tab_mode >= TAB_COUNT);
}

static void test_command_handler_surfaces_tab(void) {
    AppData app;
    reset_state(&app);
    app.current_tab = TAB_WINDOWS;
    rules_provider_register();

    gboolean result = s_rules_command.handler(&app, NULL, NULL);

    ASSERT_TRUE("rules command returns false", result == FALSE);
    ASSERT_TRUE("rules command exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("rules command records origin tab", app.prefix_origin_tab == TAB_WINDOWS);
    ASSERT_TRUE("rules command surfaces rules tab", g_surface_tab_calls == 1 &&
                g_last_surface_tab == (TabMode)g_registered_provider.tab_mode &&
                app.current_tab == (TabMode)g_registered_provider.tab_mode);
}

static void test_ctrl_p_uses_shared_pattern_overlay(void) {
    AppData app;
    reset_state(&app);
    seed_rules(&app);
    filter_rules(&app, "");
    app.current_tab = rules_tab_mode();
    g_selected_pattern_match_id = 101;

    GdkEventKey event = {0};
    event.keyval = GDK_KEY_p;
    event.state = GDK_CONTROL_MASK;
    ASSERT_TRUE("Ctrl+P handled in rules tab", handle_rules_tab_keys(&event, &app) == TRUE);
    ASSERT_TRUE("Ctrl+P opens shared pattern overlay", g_show_pattern_overlay_calls == 1);
    ASSERT_TRUE("Ctrl+P passes commands context",
                strcmp(g_last_pattern_context, "Commands: sb on") == 0);
}

int main(void) {
    printf("Rules provider tests\n");
    printf("====================\n\n");

    test_filter_and_format_row();
    test_empty_row();
    test_query_resets_selection();
    test_on_enter_filters_all_rules();
    test_filter_hides_tagged_rules_by_default();
    test_filter_shows_tagged_rules_when_enabled();
    test_search_still_hides_tagged_rules_when_toggle_off();
    test_selected_rule_and_config_index();
    test_orphan_rule_row_fallback_indicator();
    test_command_metadata();
    test_command_handler_surfaces_tab();
    test_ctrl_p_uses_shared_pattern_overlay();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
