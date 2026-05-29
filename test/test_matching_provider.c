#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "core/app/app_data.h"
#include "providers/cofi_tab_provider.h"
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

static int g_reset_selection_calls;
static int g_exit_command_mode_calls;
static int g_surface_tab_calls;
static TabMode g_last_surface_tab = -1;
static CofiTabProvider g_registered_provider;
static int g_show_pattern_edit_overlay_calls;
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

void show_name_edit_overlay(AppData *app) { (void)app; }
int selected_match_id_for_pattern_edit(AppData *app) {
    if (!app || app->filtered_matching_count <= 0) return 0;
    int idx = app->selection.provider_index;
    if (idx < 0) idx = 0;
    if (idx >= app->filtered_matching_count) idx = app->filtered_matching_count - 1;
    return app->filtered_matching[idx].match_id;
}
gboolean show_pattern_edit_overlay(AppData *app, int match_id, const char *context_line) {
    (void)app;
    if (match_id <= 0) return FALSE;
    g_show_pattern_edit_overlay_calls++;
    g_strlcpy(g_last_pattern_context, context_line ? context_line : "",
              sizeof(g_last_pattern_context));
    return TRUE;
}
void show_name_delete_overlay(AppData *app, const char *custom_name, int match_id) {
    (void)app; (void)custom_name; (void)match_id;
}

gint get_display_columns(AppData *app) {
    (void)app;
    return 115;
}

int match_entry_find_index_by_window(const MatchEntryManager *manager, Window id) {
    if (!manager) return -1;
    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].bound_x11_id == id) return i;
    }
    return -1;
}

int match_entry_find_index_by_match_id(const MatchEntryManager *manager, int match_id) {
    if (!manager) return -1;
    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].match_id == match_id) return i;
    }
    return -1;
}

#include "matching/filter_matching.c"
#include "matching/matching_provider.c"

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    g_reset_selection_calls = 0;
    g_exit_command_mode_calls = 0;
    g_surface_tab_calls = 0;
    g_last_surface_tab = -1;
    g_show_pattern_edit_overlay_calls = 0;
    g_last_pattern_context[0] = '\0';
    memset(&g_registered_provider, 0, sizeof(g_registered_provider));
}

static void seed_entries(AppData *app) {
    app->matching.count = 2;
    app->matching.entries[0].match_id = 101;
    app->matching.entries[0].bound_x11_id = (Window)0x111;
    app->matching.entries[0].assigned = 1;
    g_strlcpy(app->matching.entries[0].original_title, "main.c",
              sizeof(app->matching.entries[0].original_title));
    g_strlcpy(app->matching.entries[0].class_name, "Code",
              sizeof(app->matching.entries[0].class_name));
    g_strlcpy(app->matching.entries[0].instance, "code",
              sizeof(app->matching.entries[0].instance));
    g_strlcpy(app->matching.entries[0].type, "Normal",
              sizeof(app->matching.entries[0].type));

    app->matching.entries[1].match_id = 202;
    app->matching.entries[1].bound_x11_id = 0;
    app->matching.entries[1].assigned = 0;
    g_strlcpy(app->matching.entries[1].original_title, "shell",
              sizeof(app->matching.entries[1].original_title));
    g_strlcpy(app->matching.entries[1].class_name, "Alacritty",
              sizeof(app->matching.entries[1].class_name));
    g_strlcpy(app->matching.entries[1].instance, "alacritty",
              sizeof(app->matching.entries[1].instance));
    g_strlcpy(app->matching.entries[1].type, "Normal",
              sizeof(app->matching.entries[1].type));
}

static void test_filter_and_format_row(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);
    seed_entries(&app);

    filter_matching(&app, "shell");

    ASSERT_TRUE("filter narrows match entries", app.filtered_matching_count == 1);
    ASSERT_TRUE("filter keeps matching pattern",
                strcmp(app.filtered_matching[0].original_title, "shell") == 0);

    memset(&row, 0, sizeof(row));
    matching_format_row(&app, 0, &row);
    ASSERT_TRUE("row has five cells", row.cell_count == 5);
    ASSERT_TRUE("row pattern text", strcmp(row.cells[0].text, "shell") == 0);
    ASSERT_TRUE("row class text", strcmp(row.cells[1].text, "Alacritty") == 0);
    ASSERT_TRUE("row instance text", strcmp(row.cells[2].text, "alacritty") == 0);
    ASSERT_TRUE("row type text", strcmp(row.cells[3].text, "Normal") == 0);
    ASSERT_TRUE("orphan row shows none", strcmp(row.cells[4].text, "* NONE *") == 0);
    ASSERT_TRUE("debug row is read-only", row.row_flags == 0);
}

static void test_filter_matches_class_instance_type(void) {
    AppData app;
    reset_state(&app);
    seed_entries(&app);

    filter_matching(&app, "Alacritty");
    ASSERT_TRUE("class participates in filter", app.filtered_matching_count == 1);
    ASSERT_TRUE("class filter returns shell row",
                strcmp(app.filtered_matching[0].original_title, "shell") == 0);

    filter_matching(&app, "code");
    ASSERT_TRUE("instance participates in filter", app.filtered_matching_count == 1);
    ASSERT_TRUE("instance filter returns main.c row",
                strcmp(app.filtered_matching[0].original_title, "main.c") == 0);
}

static void test_empty_row(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);

    ASSERT_TRUE("empty provider exposes one status row", matching_row_count(&app) == 1);

    memset(&row, 0, sizeof(row));
    matching_format_row(&app, 0, &row);
    ASSERT_TRUE("empty row text", strcmp(row.cells[0].text, "No match entries found") == 0);
    ASSERT_TRUE("empty row not actionable", row.row_flags == 0);
}

static void test_query_resets_selection(void) {
    AppData app;
    reset_state(&app);
    seed_entries(&app);

    matching_on_query_changed(&app, "main");

    ASSERT_TRUE("query filters", app.filtered_matching_count == 1);
    ASSERT_TRUE("query resets selection", g_reset_selection_calls == 1);
}

static void test_selected_entry_and_manager_index(void) {
    AppData app;
    reset_state(&app);
    seed_entries(&app);
    filter_matching(&app, "");
    app.selection.provider_index = 99;

    MatchEntry *entry = matching_selected_entry(&app);

    ASSERT_TRUE("selected entry clamps to last row", entry != NULL &&
                strcmp(entry->original_title, "shell") == 0);
    ASSERT_TRUE("provider index clamped", app.selection.provider_index == 1);
    ASSERT_TRUE("manager index resolved by match_id", matching_selected_manager_index(&app) == 1);
}

static void test_selected_manager_index_uses_match_id_with_duplicates(void) {
    AppData app;
    reset_state(&app);
    seed_entries(&app);
    app.matching.count = 3;
    app.matching.entries[2] = app.matching.entries[0];
    app.matching.entries[2].match_id = 303;
    app.matching.entries[2].bound_x11_id = app.matching.entries[0].bound_x11_id;
    filter_matching(&app, "");
    app.selection.provider_index = 2;

    ASSERT_TRUE("selected duplicate row has distinct match_id",
                app.filtered_matching[2].match_id == 303);
    ASSERT_TRUE("manager index uses selected row match_id", matching_selected_manager_index(&app) == 2);
}

static void test_command_metadata(void) {
    matching_provider_register();

    ASSERT_TRUE("provider primary command is matching",
                strcmp(s_matching_command.primary, "matching") == 0);
    ASSERT_TRUE("provider alias m",
                strcmp(s_matching_command.aliases[0], "m") == 0);
    ASSERT_TRUE("provider has no names alias",
                s_matching_command.aliases[1] == NULL);
    ASSERT_TRUE("provider command has help",
                strcmp(s_matching_command.help_format, "matching, m") == 0);
    ASSERT_TRUE("provider has no shortcut hint (read-only, no footer)",
                g_registered_provider.shortcut_hint == NULL);
    ASSERT_TRUE("matching provider has no key handler", g_registered_provider.handle_key == NULL);
    ASSERT_TRUE("provider command keeps open",
                s_matching_command.keeps_open_on_hotkey_auto == 1);
    ASSERT_TRUE("provider command handler set", s_matching_command.handler != NULL);
    ASSERT_TRUE("matching provider uses dynamic tab", g_registered_provider.tab_mode >= TAB_COUNT);
}

static void test_command_handler_surfaces_tab(void) {
    AppData app;
    reset_state(&app);
    matching_provider_register();
    app.current_tab = TAB_WINDOWS;

    gboolean result = s_matching_command.handler(&app, NULL, NULL);

    ASSERT_TRUE("matching command returns false", result == FALSE);
    ASSERT_TRUE("matching command exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("matching command records origin tab", app.prefix_origin_tab == TAB_WINDOWS);
    ASSERT_TRUE("matching command surfaces matching tab", g_surface_tab_calls == 1 &&
                g_last_surface_tab == (TabMode)g_registered_provider.tab_mode &&
                app.current_tab == (TabMode)g_registered_provider.tab_mode);
}

static void test_matching_tab_has_no_pattern_edit_key(void) {
    AppData app;
    reset_state(&app);
    seed_entries(&app);
    filter_matching(&app, "");
    app.current_tab = matching_tab_mode();
    app.selection.provider_index = 0;

    matching_provider_register();
    ASSERT_TRUE("read-only matching tab has no key handler",
                g_registered_provider.handle_key == NULL);
    ASSERT_TRUE("Ctrl+P does not open pattern edit overlay", g_show_pattern_edit_overlay_calls == 0);
}

static void test_matching_tab_has_no_name_edit_delete_keys(void) {
    AppData app;
    reset_state(&app);
    seed_entries(&app);
    filter_matching(&app, "");
    app.current_tab = matching_tab_mode();
    app.selection.provider_index = 0;

    matching_provider_register();
    ASSERT_TRUE("read-only matching tab has no key handler",
                g_registered_provider.handle_key == NULL);
}

int main(void) {
    printf("Matching provider tests\n");
    printf("====================\n\n");

    test_filter_and_format_row();
    test_empty_row();
    test_query_resets_selection();
    test_filter_matches_class_instance_type();
    test_selected_entry_and_manager_index();
    test_selected_manager_index_uses_match_id_with_duplicates();
    test_command_metadata();
    test_command_handler_surfaces_tab();
    test_matching_tab_has_no_pattern_edit_key();
    test_matching_tab_has_no_name_edit_delete_keys();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
