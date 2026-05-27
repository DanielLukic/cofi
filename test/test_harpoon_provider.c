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
static int g_show_delete_calls;
static int g_exit_command_mode_calls;
static int g_surface_tab_calls;
static int g_last_overlay_slot;
static int g_show_pattern_overlay_calls;
static char g_last_pattern_context[64];
static TabMode g_last_surface_tab = TAB_WINDOWS;
static CofiTabProvider g_registered_provider;

#define TEST_HARPOON_TAB ((TabMode)(TAB_COUNT + 1))

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

int has_match(const char *needle, const char *haystack) {
    return !needle || needle[0] == '\0' || (haystack && strstr(haystack, needle) != NULL);
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
    memset(&g_registered_provider, 0, sizeof(g_registered_provider));
    if (p) g_registered_provider = *p;
    g_registered_provider.tab_mode = TEST_HARPOON_TAB;
    return 0;
}

const CofiTabProvider *cofi_get_provider(int provider_id) {
    (void)provider_id;
    return &g_registered_provider;
}

int cofi_register_command(const CommandSpec *spec) {
    return spec ? 0 : -1;
}
int selected_match_id_for_pattern_edit(AppData *app) {
    (void)app;
    return 101;
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
        if (manager->entries[i].match_id == match_id) return i;
    }
    return -1;
}

void show_harpoon_delete_overlay(AppData *app, int slot) {
    (void)app;
    g_show_delete_calls++;
    g_last_overlay_slot = slot;
}

#include "harpoon/harpoon_provider.c"

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    g_reset_selection_calls = 0;
    g_show_delete_calls = 0;
    g_exit_command_mode_calls = 0;
    g_surface_tab_calls = 0;
    g_last_overlay_slot = -1;
    g_show_pattern_overlay_calls = 0;
    g_last_pattern_context[0] = '\0';
    g_last_surface_tab = TAB_WINDOWS;
    memset(&g_registered_provider, 0, sizeof(g_registered_provider));
    g_registered_provider.tab_mode = TEST_HARPOON_TAB;
    app->current_tab = TEST_HARPOON_TAB;
}

static void seed_slots(AppData *app) {
    app->harpoon.slots[3].assigned = 1;
    app->harpoon.slots[3].match_id = 101;
    app->matching.entries[0].match_id = 101;
    app->matching.entries[0].bound_x11_id = 0x101;
    g_strlcpy(app->matching.entries[0].original_title, "Terminal",
              sizeof(app->matching.entries[0].original_title));
    app->matching.entries[0].assigned = 1;
    app->windows[0].id = 0x101;
    g_strlcpy(app->windows[0].class_name, "Mate-terminal", sizeof(app->windows[0].class_name));
    g_strlcpy(app->windows[0].instance, "mate-terminal", sizeof(app->windows[0].instance));
    g_strlcpy(app->windows[0].type, "normal", sizeof(app->windows[0].type));

    app->harpoon.slots[12].assigned = 1;
    app->harpoon.slots[12].match_id = 202;
    app->matching.entries[1].match_id = 202;
    app->matching.entries[1].bound_x11_id = 0x202;
    g_strlcpy(app->matching.entries[1].original_title, "Browser",
              sizeof(app->matching.entries[1].original_title));
    app->matching.entries[1].assigned = 1;
    app->windows[1].id = 0x202;
    g_strlcpy(app->windows[1].class_name, "Firefox", sizeof(app->windows[1].class_name));
    g_strlcpy(app->windows[1].instance, "firefox", sizeof(app->windows[1].instance));
    g_strlcpy(app->windows[1].type, "normal", sizeof(app->windows[1].type));
    app->window_count = 2;
    app->matching.count = 2;
}

static void test_filter_and_format_row(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);
    seed_slots(&app);

    filter_harpoon(&app, "fire");

    ASSERT_TRUE("filter narrows harpoon slots", app.filtered_harpoon_count == 1);
    ASSERT_TRUE("filter keeps actual slot index", app.filtered_harpoon_indices[0] == 12);

    memset(&row, 0, sizeof(row));
    harpoon_format_row(&app, 0, &row);
    ASSERT_TRUE("row has five cells", row.cell_count == 5);
    ASSERT_TRUE("row shows letter slot key", strcmp(row.cells[0].text, "c") == 0);
    ASSERT_TRUE("row title text", strcmp(row.cells[1].text, "Browser") == 0);
    ASSERT_TRUE("row is actionable", row.row_flags == COFI_ROW_ACTIONABLE);
}

static void test_empty_row(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);

    ASSERT_TRUE("empty provider exposes one status row", harpoon_row_count(&app) == 1);

    memset(&row, 0, sizeof(row));
    harpoon_format_row(&app, 0, &row);
    ASSERT_TRUE("empty row text", strcmp(row.cells[0].text, "No harpoon slots found") == 0);
    ASSERT_TRUE("empty row not actionable", row.row_flags == 0);
}

static void test_query_resets_selection(void) {
    AppData app;
    reset_state(&app);
    seed_slots(&app);

    harpoon_on_query_changed(&app, "term");

    ASSERT_TRUE("query filters", app.filtered_harpoon_count == 1);
    ASSERT_TRUE("query resets selection", g_reset_selection_calls == 1);
}

static void test_selected_slot_clamps_and_keys_use_actual_slot(void) {
    AppData app;
    reset_state(&app);
    seed_slots(&app);
    filter_harpoon(&app, "");
    app.selection.provider_index = 99;

    int actual_slot = -1;
    HarpoonSlot *slot = harpoon_selected_slot(&app, &actual_slot);

    ASSERT_TRUE("selected slot clamps to last filtered row", slot != NULL && actual_slot == 12);
    ASSERT_TRUE("provider index clamped", app.selection.provider_index == 1);

    GdkEventKey ev;
    memset(&ev, 0, sizeof(ev));
    ev.state = GDK_CONTROL_MASK;
    ev.keyval = GDK_KEY_e;
    ASSERT_TRUE("Ctrl+E falls through", handle_harpoon_tab_keys(&ev, &app) == FALSE);
    ev.keyval = GDK_KEY_d;
    ASSERT_TRUE("Ctrl+D handled", handle_harpoon_tab_keys(&ev, &app) == TRUE);
    ASSERT_TRUE("Ctrl+D opens selected actual slot", g_show_delete_calls == 1 && g_last_overlay_slot == 12);

    ev.keyval = GDK_KEY_p;
    ASSERT_TRUE("Ctrl+P handled", handle_harpoon_tab_keys(&ev, &app) == TRUE);
    ASSERT_TRUE("Ctrl+P opens pattern overlay", g_show_pattern_overlay_calls == 1);
    ASSERT_TRUE("Ctrl+P passes slot context", strcmp(g_last_pattern_context, "Slot: c") == 0);
}

static void test_command_metadata(void) {
    AppData app;
    reset_state(&app);

    harpoon_provider_register();

    ASSERT_TRUE("primary command is harpoon",
                strcmp(s_harpoon_command.primary, "harpoon") == 0);
    ASSERT_TRUE("hp alias registered",
                strcmp(s_harpoon_command.aliases[0], "hp") == 0);
    ASSERT_TRUE("command keeps hotkey open",
                s_harpoon_command.keeps_open_on_hotkey_auto == 1);
    ASSERT_TRUE("command handler exists", s_harpoon_command.handler != NULL);
    ASSERT_TRUE("harpoon tab is dynamic", g_registered_provider.tab_mode >= TAB_COUNT);
    ASSERT_TRUE("harpoon claims delegate opcode",
                g_registered_provider.delegate_opcode == COFI_OPCODE_HARPOON);
    ASSERT_TRUE("harpoon claims hotkey mode",
                g_registered_provider.hotkey_mode_claim ==
                COFI_PROVIDER_HOTKEY_MODE(SHOW_MODE_HARPOON));
}

static void test_command_handler_surfaces_tab(void) {
    AppData app;
    reset_state(&app);
    app.current_tab = TAB_WINDOWS;

    harpoon_provider_register();
    gboolean result = s_harpoon_command.handler(&app, NULL, NULL);

    ASSERT_TRUE("command handler returns false", result == FALSE);
    ASSERT_TRUE("command exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("command stores prefix origin", app.prefix_origin_tab == TAB_WINDOWS);
    ASSERT_TRUE("command surfaces harpoon tab",
                g_surface_tab_calls == 1 &&
                g_last_surface_tab == (TabMode)g_registered_provider.tab_mode);
}

int main(void) {
    printf("Harpoon provider tests\n");
    printf("======================\n\n");

    test_filter_and_format_row();
    test_empty_row();
    test_query_resets_selection();
    test_selected_slot_clamps_and_keys_use_actual_slot();
    test_command_metadata();
    test_command_handler_surfaces_tab();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
