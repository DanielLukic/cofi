#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/cofi_tab_provider.h"

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

void cofi_init_provider_defaults(CofiTabProvider *p) {
    if (p) memset(p, 0, sizeof(*p));
}

int cofi_register_tab_provider(const CofiTabProvider *p) {
    (void)p;
    return 0;
}

void cleanup_hotkeys(AppData *app) { (void)app; }
void show_overlay(AppData *app, OverlayType type, void *data) {
    (void)app; (void)type; (void)data;
}
int remove_hotkey_binding(HotkeyConfig *config, const char *key) {
    (void)config; (void)key;
    return 1;
}
int save_hotkey_config(const HotkeyConfig *config) {
    (void)config;
    return 1;
}
void regrab_hotkeys(AppData *app) { (void)app; }
void update_display(AppData *app) { (void)app; }

#include "../src/hotkeys_provider.c"

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    g_reset_selection_calls = 0;
}

static void seed_hotkeys(AppData *app) {
    app->hotkey_config.count = 3;
    g_strlcpy(app->hotkey_config.bindings[0].key, "Mod4+w",
              sizeof(app->hotkey_config.bindings[0].key));
    g_strlcpy(app->hotkey_config.bindings[0].command, "show windows",
              sizeof(app->hotkey_config.bindings[0].command));
    g_strlcpy(app->hotkey_config.bindings[1].key, "Mod4+c",
              sizeof(app->hotkey_config.bindings[1].key));
    g_strlcpy(app->hotkey_config.bindings[1].command, "show command",
              sizeof(app->hotkey_config.bindings[1].command));
    g_strlcpy(app->hotkey_config.bindings[2].key, "Mod4+r",
              sizeof(app->hotkey_config.bindings[2].key));
    g_strlcpy(app->hotkey_config.bindings[2].command, "show run",
              sizeof(app->hotkey_config.bindings[2].command));
}

static void test_filter_and_row_format(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);
    seed_hotkeys(&app);

    filter_hotkeys(&app, "Mod4+c");

    ASSERT_TRUE("filter narrows hotkeys", app.filtered_hotkeys_count == 1);
    ASSERT_TRUE("filter keeps master index", app.filtered_hotkeys_indices[0] == 1);

    memset(&row, 0, sizeof(row));
    hotkeys_format_row(&app, 0, &row);
    ASSERT_TRUE("row has key and command cells", row.cell_count == 2);
    ASSERT_TRUE("row key text", strcmp(row.cells[0].text, "Mod4+c") == 0);
    ASSERT_TRUE("row command text", strcmp(row.cells[1].text, "show command") == 0);
    ASSERT_TRUE("row is actionable", row.row_flags == COFI_ROW_ACTIONABLE);
}

static void test_empty_row_and_hint(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);

    ASSERT_TRUE("empty provider exposes one status row", hotkeys_row_count(&app) == 1);
    memset(&row, 0, sizeof(row));
    hotkeys_format_row(&app, 0, &row);
    ASSERT_TRUE("empty row text", strcmp(row.cells[0].text, "No hotkey bindings found") == 0);
    ASSERT_TRUE("empty row not actionable", row.row_flags == 0);
    ASSERT_TRUE("empty hint add-only",
                strcmp(hotkeys_shortcut_hint(&app), "Shortcuts: Ctrl+A=Add binding") == 0);
}

static void test_query_resets_selection(void) {
    AppData app;
    reset_state(&app);
    seed_hotkeys(&app);

    hotkeys_on_query_changed(&app, "run");

    ASSERT_TRUE("query filters", app.filtered_hotkeys_count == 1);
    ASSERT_TRUE("query resets selection", g_reset_selection_calls == 1);
}

static void test_selected_binding_clamps_and_returns_master_index(void) {
    AppData app;
    int master_idx = -1;
    reset_state(&app);
    seed_hotkeys(&app);
    filter_hotkeys(&app, "");
    app.selection.provider_index = 99;

    HotkeyBinding *binding = hotkeys_selected_binding(&app, &master_idx);

    ASSERT_TRUE("selected binding clamps to last row", binding != NULL &&
                strcmp(binding->key, "Mod4+r") == 0);
    ASSERT_TRUE("selected master index returned", master_idx == 2);
    ASSERT_TRUE("provider index clamped", app.selection.provider_index == 2);
}

static void test_select_key(void) {
    AppData app;
    reset_state(&app);
    seed_hotkeys(&app);
    filter_hotkeys(&app, "");

    hotkeys_select_key(&app, "Mod4+c");
    ASSERT_TRUE("select key picks matching filtered row", app.selection.provider_index == 1);

    hotkeys_select_key(&app, "missing");
    ASSERT_TRUE("missing key resets selection", app.selection.provider_index == 0);
}

int main(void) {
    printf("Hotkeys provider tests\n");
    printf("======================\n\n");

    test_filter_and_row_format();
    test_empty_row_and_hint();
    test_query_resets_selection();
    test_selected_binding_clamps_and_returns_master_index();
    test_select_key();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
