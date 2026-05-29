#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "providers/cofi_tab_provider.h"
#include "commands/command_registry.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(msg, cond) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("PASS: %s\n", msg); } \
    else { printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

static int g_reset_selection_calls;
static int g_edit_overlay_calls;
static int g_delete_overlay_calls;
static int g_pattern_overlay_calls;
static int g_pattern_match_id;
static int g_deleted_match_id;
static char g_deleted_name[MAX_TITLE_LEN];
static char g_pattern_context[128];
static CofiTabProvider g_registered_provider;
static CommandSpec g_registered_command;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

int has_match(const char *needle, const char *haystack) {
    return !needle || !needle[0] || (haystack && strstr(haystack, needle));
}

void reset_selection(AppData *app) {
    (void)app;
    g_reset_selection_calls++;
}

void exit_command_mode(AppData *app) {
    (void)app;
}

void surface_tab(AppData *app, TabMode tab) {
    if (app) app->current_tab = tab;
}

gint get_display_columns(AppData *app) {
    (void)app;
    return 120;
}

void cofi_init_provider_defaults(CofiTabProvider *provider) {
    if (provider) memset(provider, 0, sizeof(*provider));
}

int cofi_register_tab_provider(const CofiTabProvider *provider) {
    g_registered_provider = *provider;
    g_registered_provider.tab_mode = (TabMode)(TAB_COUNT + 11);
    return 0;
}

const CofiTabProvider *cofi_get_provider(int provider_id) {
    return provider_id == 0 ? &g_registered_provider : NULL;
}

int cofi_register_command(const CommandSpec *spec) {
    if (!spec) return -1;
    g_registered_command = *spec;
    return 0;
}

int match_entry_find_index_by_match_id(const MatchEntryManager *manager, int match_id) {
    if (!manager) return -1;
    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].match_id == match_id) return i;
    }
    return -1;
}

void show_name_edit_overlay(AppData *app) {
    (void)app;
    g_edit_overlay_calls++;
}

void show_name_delete_overlay(AppData *app, const char *custom_name, int match_id) {
    (void)app;
    g_delete_overlay_calls++;
    g_deleted_match_id = match_id;
    g_strlcpy(g_deleted_name, custom_name ? custom_name : "", sizeof(g_deleted_name));
}

gboolean show_pattern_edit_overlay(AppData *app, int match_id, const char *context_line) {
    (void)app;
    g_pattern_overlay_calls++;
    g_pattern_match_id = match_id;
    g_strlcpy(g_pattern_context, context_line ? context_line : "", sizeof(g_pattern_context));
    return TRUE;
}

#include "names/names_provider.c"

static void reset_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    g_reset_selection_calls = 0;
    g_edit_overlay_calls = 0;
    g_delete_overlay_calls = 0;
    g_pattern_overlay_calls = 0;
    g_pattern_match_id = 0;
    g_deleted_match_id = 0;
    g_deleted_name[0] = '\0';
    g_pattern_context[0] = '\0';
}

static void seed_names(AppData *app) {
    app->matching.count = 2;
    app->matching.entries[0].match_id = 101;
    app->matching.entries[0].assigned = 1;
    app->matching.entries[0].bound_x11_id = 0xaaa;
    g_strlcpy(app->matching.entries[0].original_title, "Alpha Terminal",
              sizeof(app->matching.entries[0].original_title));
    g_strlcpy(app->matching.entries[0].class_name, "Kitty",
              sizeof(app->matching.entries[0].class_name));
    g_strlcpy(app->matching.entries[0].instance, "kitty-alpha",
              sizeof(app->matching.entries[0].instance));
    g_strlcpy(app->matching.entries[0].type, "Normal",
              sizeof(app->matching.entries[0].type));

    app->matching.entries[1].match_id = 202;
    app->matching.entries[1].assigned = 0;
    g_strlcpy(app->matching.entries[1].original_title, "Beta Browser",
              sizeof(app->matching.entries[1].original_title));
    g_strlcpy(app->matching.entries[1].class_name, "Firefox",
              sizeof(app->matching.entries[1].class_name));
    g_strlcpy(app->matching.entries[1].instance, "firefox",
              sizeof(app->matching.entries[1].instance));
    g_strlcpy(app->matching.entries[1].type, "Normal",
              sizeof(app->matching.entries[1].type));

    app->names.count = 2;
    app->names.records[0].match_id = 101;
    g_strlcpy(app->names.records[0].custom_name, "billing",
              sizeof(app->names.records[0].custom_name));
    app->names.records[1].match_id = 202;
    g_strlcpy(app->names.records[1].custom_name, "research",
              sizeof(app->names.records[1].custom_name));
}

static void test_filter_format_and_identity(void) {
    AppData app;
    CofiRowCells row = {0};
    reset_app(&app);
    seed_names(&app);

    names_on_query_changed(&app, "billing");

    ASSERT_TRUE("filter narrows names", app.filtered_names_count == 1);
    ASSERT_TRUE("filter tracks store index", app.filtered_names_indices[0] == 0);
    ASSERT_TRUE("query resets selection", g_reset_selection_calls == 1);
    ASSERT_TRUE("row count exposes filtered rows", names_row_count(&app) == 1);
    names_format_row(&app, 0, &row);
    ASSERT_TRUE("row has six cells", row.cell_count == 6);
    ASSERT_TRUE("row name cell", strcmp(row.cells[0].text, "billing") == 0);
    ASSERT_TRUE("row pattern cell", strcmp(row.cells[1].text, "Alpha Terminal") == 0);
    ASSERT_TRUE("row class cell", strcmp(row.cells[2].text, "Kitty") == 0);
    ASSERT_TRUE("row instance cell", strcmp(row.cells[3].text, "kitty-alpha") == 0);
    ASSERT_TRUE("row type cell", strcmp(row.cells[4].text, "Normal") == 0);
    ASSERT_TRUE("row binding cell", strcmp(row.cells[5].text, "0xaaa") == 0);
    ASSERT_TRUE("row actionable", row.row_flags == COFI_ROW_ACTIONABLE);
    ASSERT_TRUE("row identity is match id", strcmp(names_row_identity(&app, 0), "name:101") == 0);
    ASSERT_TRUE("match string includes name", strstr(names_match_string(&app, 0), "billing") != NULL);
}

static void test_empty_state(void) {
    AppData app;
    CofiRowCells row = {0};
    reset_app(&app);

    names_on_query_changed(&app, "");
    ASSERT_TRUE("empty names tab has one status row", names_row_count(&app) == 1);
    names_format_row(&app, 0, &row);
    ASSERT_TRUE("empty row has one cell", row.cell_count == 1);
    ASSERT_TRUE("empty row text", strcmp(row.cells[0].text, "No named windows found") == 0);
    ASSERT_TRUE("empty row not actionable", row.row_flags == 0);
    ASSERT_TRUE("empty selected record null", names_selected_record(&app) == NULL);
}

static void test_selection_and_key_handlers(void) {
    AppData app;
    reset_app(&app);
    seed_names(&app);
    names_on_query_changed(&app, "");
    app.current_tab = names_tab_mode();
    app.selection.provider_index = 9;

    NameRecord *selected = names_selected_record(&app);
    ASSERT_TRUE("selection clamps to last row", selected && selected->match_id == 202);
    ASSERT_TRUE("store index follows clamped row", names_selected_store_index(&app) == 1);

    names_select_custom_name(&app, "billing");
    ASSERT_TRUE("select by custom name", app.selection.provider_index == 0);

    GdkEventKey edit = {.keyval = GDK_KEY_e, .state = GDK_CONTROL_MASK};
    ASSERT_TRUE("Ctrl+E handled", handle_names_tab_keys(&edit, &app) == TRUE);
    ASSERT_TRUE("Ctrl+E opens edit overlay", g_edit_overlay_calls == 1);

    GdkEventKey del = {.keyval = GDK_KEY_d, .state = GDK_CONTROL_MASK};
    ASSERT_TRUE("Ctrl+D handled", handle_names_tab_keys(&del, &app) == TRUE);
    ASSERT_TRUE("Ctrl+D opens delete overlay", g_delete_overlay_calls == 1);
    ASSERT_TRUE("Ctrl+D passes name", strcmp(g_deleted_name, "billing") == 0);
    ASSERT_TRUE("Ctrl+D passes match id", g_deleted_match_id == 101);

    GdkEventKey pattern = {.keyval = GDK_KEY_p, .state = GDK_CONTROL_MASK};
    ASSERT_TRUE("Ctrl+P handled", handle_names_tab_keys(&pattern, &app) == TRUE);
    ASSERT_TRUE("Ctrl+P opens pattern edit overlay", g_pattern_overlay_calls == 1);
    ASSERT_TRUE("Ctrl+P passes selected match id", g_pattern_match_id == 101);
    ASSERT_TRUE("Ctrl+P passes names context",
                strcmp(g_pattern_context, "Name: billing") == 0);
}

static void test_provider_metadata(void) {
    ASSERT_TRUE("provider id is names", strcmp(g_registered_provider.id, "names") == 0);
    ASSERT_TRUE("provider hidden by default", g_registered_provider.hidden_by_default == 1);
    ASSERT_TRUE("provider delegate opcode", g_registered_provider.delegate_opcode == COFI_OPCODE_NAMES);
    ASSERT_TRUE("command primary names", strcmp(g_registered_command.primary, "names") == 0);
    ASSERT_TRUE("command alias nm", strcmp(g_registered_command.aliases[0], "nm") == 0);
    ASSERT_TRUE("shortcut hint includes pattern edit",
                strstr(g_registered_provider.shortcut_hint, "Ctrl+P=Edit pattern") != NULL);
}

int main(void) {
    printf("Names provider tests\n");
    printf("====================\n\n");

    names_provider_register();
    test_filter_format_and_identity();
    test_empty_state();
    test_selection_and_key_handlers();
    test_provider_metadata();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
