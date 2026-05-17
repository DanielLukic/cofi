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
static int g_show_delete_calls;
static int g_show_edit_calls;
static int g_exit_command_mode_calls;
static int g_surface_tab_calls;
static int g_last_overlay_slot;
static TabMode g_last_surface_tab = TAB_WINDOWS;

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
    (void)p;
    return 0;
}

void show_harpoon_delete_overlay(AppData *app, int slot) {
    (void)app;
    g_show_delete_calls++;
    g_last_overlay_slot = slot;
}

void show_harpoon_edit_overlay(AppData *app, int slot) {
    (void)app;
    g_show_edit_calls++;
    g_last_overlay_slot = slot;
}

#include "../src/harpoon_provider.c"

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    g_reset_selection_calls = 0;
    g_show_delete_calls = 0;
    g_show_edit_calls = 0;
    g_exit_command_mode_calls = 0;
    g_surface_tab_calls = 0;
    g_last_overlay_slot = -1;
    g_last_surface_tab = TAB_WINDOWS;
    app->current_tab = TAB_HARPOON;
}

static void seed_slots(AppData *app) {
    app->harpoon.slots[3].assigned = 1;
    g_strlcpy(app->harpoon.slots[3].title, "Terminal",
              sizeof(app->harpoon.slots[3].title));
    g_strlcpy(app->harpoon.slots[3].class_name, "Mate-terminal",
              sizeof(app->harpoon.slots[3].class_name));
    g_strlcpy(app->harpoon.slots[3].instance, "mate-terminal",
              sizeof(app->harpoon.slots[3].instance));
    g_strlcpy(app->harpoon.slots[3].type, "normal",
              sizeof(app->harpoon.slots[3].type));

    app->harpoon.slots[12].assigned = 1;
    g_strlcpy(app->harpoon.slots[12].title, "Browser",
              sizeof(app->harpoon.slots[12].title));
    g_strlcpy(app->harpoon.slots[12].class_name, "Firefox",
              sizeof(app->harpoon.slots[12].class_name));
    g_strlcpy(app->harpoon.slots[12].instance, "firefox",
              sizeof(app->harpoon.slots[12].instance));
    g_strlcpy(app->harpoon.slots[12].type, "normal",
              sizeof(app->harpoon.slots[12].type));
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
    ev.keyval = GDK_KEY_e;
    ev.state = GDK_CONTROL_MASK;
    ASSERT_TRUE("Ctrl+E handled", handle_harpoon_tab_keys(&ev, &app) == TRUE);
    ASSERT_TRUE("Ctrl+E opens selected actual slot", g_show_edit_calls == 1 && g_last_overlay_slot == 12);

    ev.keyval = GDK_KEY_d;
    ASSERT_TRUE("Ctrl+D handled", handle_harpoon_tab_keys(&ev, &app) == TRUE);
    ASSERT_TRUE("Ctrl+D opens selected actual slot", g_show_delete_calls == 1 && g_last_overlay_slot == 12);
}

static void test_command_metadata(void) {
    AppData app;
    reset_state(&app);

    harpoon_provider_register();

    ASSERT_TRUE("primary command is harpoon",
                strcmp(s_harpoon_provider.primary_cmd, "harpoon") == 0);
    ASSERT_TRUE("hp alias registered",
                s_harpoon_provider.aliases && strcmp(s_harpoon_provider.aliases[0], "hp") == 0);
    ASSERT_TRUE("command keeps hotkey open",
                s_harpoon_provider.command_keeps_open_on_hotkey_auto == 1);
    ASSERT_TRUE("command handler exists", s_harpoon_provider.command_handler != NULL);
}

static void test_command_handler_surfaces_tab(void) {
    AppData app;
    reset_state(&app);
    app.current_tab = TAB_WINDOWS;

    harpoon_provider_register();
    gboolean result = s_harpoon_provider.command_handler(&app, NULL, NULL);

    ASSERT_TRUE("command handler returns false", result == FALSE);
    ASSERT_TRUE("command exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("command stores prefix origin", app.prefix_origin_tab == TAB_WINDOWS);
    ASSERT_TRUE("command surfaces harpoon tab",
                g_surface_tab_calls == 1 && g_last_surface_tab == TAB_HARPOON);
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
