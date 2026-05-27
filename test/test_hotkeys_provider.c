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
static int g_parse_hotkey_action;
static int g_add_hotkey_calls;
static int g_remove_hotkey_calls;
static int g_save_hotkey_calls;
static int g_regrab_hotkey_calls;
static int g_remove_hotkey_result = 1;
static char g_last_hotkey_key[64];
static char g_last_hotkey_command[256];
static CofiTabProvider g_registered_provider;

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

void cleanup_hotkeys(AppData *app) { (void)app; }
void show_overlay(AppData *app, OverlayType type, void *data) {
    (void)app; (void)type; (void)data;
}
int parse_hotkey_command(const char *args, char *key_out, size_t key_size,
                         char *cmd_out, size_t cmd_size) {
    (void)args;
    g_strlcpy(key_out, "Mod4+x", key_size);
    g_strlcpy(cmd_out, "show windows", cmd_size);
    return g_parse_hotkey_action;
}
int add_hotkey_binding(HotkeyConfig *config, const char *key, const char *command) {
    (void)config;
    g_add_hotkey_calls++;
    g_strlcpy(g_last_hotkey_key, key, sizeof(g_last_hotkey_key));
    g_strlcpy(g_last_hotkey_command, command, sizeof(g_last_hotkey_command));
    return 1;
}
int remove_hotkey_binding(HotkeyConfig *config, const char *key) {
    (void)config;
    g_remove_hotkey_calls++;
    g_strlcpy(g_last_hotkey_key, key, sizeof(g_last_hotkey_key));
    return g_remove_hotkey_result;
}
int save_hotkey_config(const HotkeyConfig *config) {
    (void)config;
    g_save_hotkey_calls++;
    return 1;
}
void regrab_hotkeys(AppData *app) {
    (void)app;
    g_regrab_hotkey_calls++;
}
void update_display(AppData *app) { (void)app; }
void exit_command_mode(AppData *app) {
    (void)app;
    g_exit_command_mode_calls++;
}
void surface_tab(AppData *app, TabMode tab) {
    if (app) app->current_tab = tab;
    g_surface_tab_calls++;
    g_last_surface_tab = tab;
}

#include "daemon/hotkeys_provider.c"

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    g_reset_selection_calls = 0;
    g_exit_command_mode_calls = 0;
    g_surface_tab_calls = 0;
    g_last_surface_tab = -1;
    g_parse_hotkey_action = 0;
    g_add_hotkey_calls = 0;
    g_remove_hotkey_calls = 0;
    g_save_hotkey_calls = 0;
    g_regrab_hotkey_calls = 0;
    g_remove_hotkey_result = 1;
    g_last_hotkey_key[0] = '\0';
    g_last_hotkey_command[0] = '\0';
    memset(&g_registered_provider, 0, sizeof(g_registered_provider));
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
    ASSERT_TRUE("empty hint shows full hotkey actions",
                strcmp(hotkeys_shortcut_hint(&app),
                       "Shortcuts: Ctrl+A=Add binding  Ctrl+B=Rebind key  Ctrl+E=Edit command  Ctrl+D=Delete binding") == 0);
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

static void test_command_metadata(void) {
    hotkeys_provider_register();

    ASSERT_TRUE("provider primary command is hotkeys",
                strcmp(s_hotkeys_command.primary, "hotkeys") == 0);
    ASSERT_TRUE("provider alias is hotkey",
                strcmp(s_hotkeys_command.aliases[0], "hotkey") == 0);
    ASSERT_TRUE("provider second alias is hk",
                strcmp(s_hotkeys_command.aliases[1], "hk") == 0);
    ASSERT_TRUE("provider command has help",
                strcmp(s_hotkeys_command.help_format,
                       "hotkeys [<key> [command] | <key>]") == 0);
    ASSERT_TRUE("provider command keeps open",
                s_hotkeys_command.keeps_open_on_hotkey_auto == 1);
    ASSERT_TRUE("provider command handler set", s_hotkeys_command.handler != NULL);
    ASSERT_TRUE("provider uses dynamic tab",
                g_registered_provider.tab_mode >= TAB_COUNT);
}

static void test_command_handler_surfaces_tab(void) {
    AppData app;
    reset_state(&app);
    hotkeys_provider_register();
    app.current_tab = TAB_WINDOWS;

    gboolean result = s_hotkeys_command.handler(&app, NULL, "");

    ASSERT_TRUE("hotkeys command returns false", result == FALSE);
    ASSERT_TRUE("hotkeys command exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("hotkeys command surfaces hotkeys tab",
                g_surface_tab_calls == 1 &&
                g_last_surface_tab == (TabMode)g_registered_provider.tab_mode &&
                app.current_tab == (TabMode)g_registered_provider.tab_mode);
    ASSERT_TRUE("hotkeys command records origin tab",
                app.prefix_origin_tab == TAB_WINDOWS);
    ASSERT_TRUE("hotkeys bare command does not mutate bindings",
                g_add_hotkey_calls == 0 && g_remove_hotkey_calls == 0 &&
                g_save_hotkey_calls == 0 && g_regrab_hotkey_calls == 0);
}

static void test_command_handler_adds_binding(void) {
    AppData app;
    reset_state(&app);
    hotkeys_provider_register();
    g_parse_hotkey_action = 1;

    s_hotkeys_command.handler(&app, NULL, "Mod4+x show windows");

    ASSERT_TRUE("hotkeys add command calls add", g_add_hotkey_calls == 1);
    ASSERT_TRUE("hotkeys add command captures key", strcmp(g_last_hotkey_key, "Mod4+x") == 0);
    ASSERT_TRUE("hotkeys add command captures command", strcmp(g_last_hotkey_command, "show windows") == 0);
    ASSERT_TRUE("hotkeys add command saves and regrabs",
                g_save_hotkey_calls == 1 && g_regrab_hotkey_calls == 1);
    ASSERT_TRUE("hotkeys add command surfaces tab",
                app.current_tab == (TabMode)g_registered_provider.tab_mode);
}

static void test_command_handler_removes_binding(void) {
    AppData app;
    reset_state(&app);
    hotkeys_provider_register();
    g_parse_hotkey_action = 2;

    s_hotkeys_command.handler(&app, NULL, "Mod4+x");

    ASSERT_TRUE("hotkeys remove command calls remove", g_remove_hotkey_calls == 1);
    ASSERT_TRUE("hotkeys remove command captures key", strcmp(g_last_hotkey_key, "Mod4+x") == 0);
    ASSERT_TRUE("hotkeys remove command saves and regrabs",
                g_save_hotkey_calls == 1 && g_regrab_hotkey_calls == 1);
    ASSERT_TRUE("hotkeys remove command surfaces tab",
                app.current_tab == (TabMode)g_registered_provider.tab_mode);
}

static void test_command_handler_missing_remove_still_surfaces(void) {
    AppData app;
    reset_state(&app);
    hotkeys_provider_register();
    g_parse_hotkey_action = 2;
    g_remove_hotkey_result = 0;

    s_hotkeys_command.handler(&app, NULL, "Mod4+x");

    ASSERT_TRUE("missing hotkeys remove still calls remove", g_remove_hotkey_calls == 1);
    ASSERT_TRUE("missing hotkeys remove does not save or regrab",
                g_save_hotkey_calls == 0 && g_regrab_hotkey_calls == 0);
    ASSERT_TRUE("missing hotkeys remove still surfaces tab",
                app.current_tab == (TabMode)g_registered_provider.tab_mode);
}

int main(void) {
    printf("Hotkeys provider tests\n");
    printf("======================\n\n");

    test_filter_and_row_format();
    test_empty_row_and_hint();
    test_query_resets_selection();
    test_selected_binding_clamps_and_returns_master_index();
    test_select_key();
    test_command_metadata();
    test_command_handler_surfaces_tab();
    test_command_handler_adds_binding();
    test_command_handler_removes_binding();
    test_command_handler_missing_remove_still_surfaces();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
