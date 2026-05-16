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
static int g_switch_desktop_calls;
static int g_last_desktop = -1;

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

void cofi_init_provider_defaults(CofiTabProvider *p) {
    if (p) memset(p, 0, sizeof(*p));
}

int cofi_register_tab_provider(const CofiTabProvider *p) {
    (void)p;
    return 0;
}

void switch_to_desktop(Display *display, int desktop) {
    (void)display;
    g_switch_desktop_calls++;
    g_last_desktop = desktop;
}

#include "../src/workspaces_provider.c"

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    g_reset_selection_calls = 0;
    g_switch_desktop_calls = 0;
    g_last_desktop = -1;
    app->current_tab = TAB_WORKSPACES;
}

static void seed_workspaces(AppData *app) {
    app->workspace_count = 3;

    app->workspaces[0].id = 0;
    g_strlcpy(app->workspaces[0].name, "main", sizeof(app->workspaces[0].name));
    app->workspaces[0].is_current = 1;

    app->workspaces[1].id = 1;
    g_strlcpy(app->workspaces[1].name, "code", sizeof(app->workspaces[1].name));

    app->workspaces[2].id = 2;
    g_strlcpy(app->workspaces[2].name, "browser", sizeof(app->workspaces[2].name));
}

static void test_filter_and_format_row(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);
    seed_workspaces(&app);

    filter_workspaces(&app, "code");

    ASSERT_TRUE("filter narrows workspaces", app.filtered_workspace_count == 1);
    ASSERT_TRUE("filter keeps workspace id", app.filtered_workspaces[0].id == 1);

    memset(&row, 0, sizeof(row));
    workspaces_format_row(&app, 0, &row);
    ASSERT_TRUE("row has three cells", row.cell_count == 3);
    ASSERT_TRUE("row shows workspace number", strcmp(row.cells[1].text, "[2]") == 0);
    ASSERT_TRUE("row shows workspace name", strcmp(row.cells[2].text, "code") == 0);
    ASSERT_TRUE("row is actionable", row.row_flags == COFI_ROW_ACTIONABLE);
}

static void test_empty_row(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);

    ASSERT_TRUE("empty provider exposes one status row", workspaces_row_count(&app) == 1);

    memset(&row, 0, sizeof(row));
    workspaces_format_row(&app, 0, &row);
    ASSERT_TRUE("empty row text", strcmp(row.cells[0].text, "No matching workspaces found") == 0);
    ASSERT_TRUE("empty row not actionable", row.row_flags == 0);
}

static void test_query_resets_selection(void) {
    AppData app;
    reset_state(&app);
    seed_workspaces(&app);

    workspaces_on_query_changed(&app, "main");

    ASSERT_TRUE("query filters", app.filtered_workspace_count == 1);
    ASSERT_TRUE("query resets selection", g_reset_selection_calls == 1);
}

static void test_selected_workspace_clamps(void) {
    AppData app;
    reset_state(&app);
    seed_workspaces(&app);
    filter_workspaces(&app, "");
    app.selection.provider_index = 99;

    WorkspaceInfo *workspace = workspaces_selected_workspace(&app);

    ASSERT_TRUE("selected workspace clamps", workspace != NULL && workspace->id == 2);
    ASSERT_TRUE("provider index clamped", app.selection.provider_index == 2);
}

static void test_enter_pressed_switches_workspace(void) {
    AppData app;
    reset_state(&app);
    seed_workspaces(&app);
    filter_workspaces(&app, "");

    CofiActionStatus status = workspaces_on_enter_pressed(&app, 1, 1, "", 0);

    ASSERT_TRUE("enter returns hide status", status == COFI_HANDLED_HIDE);
    ASSERT_TRUE("enter switches selected workspace", g_switch_desktop_calls == 1 && g_last_desktop == 1);
}

int main(void) {
    printf("Workspaces provider tests\n");
    printf("=========================\n\n");

    test_filter_and_format_row();
    test_empty_row();
    test_query_resets_selection();
    test_selected_workspace_clamps();
    test_enter_pressed_switches_workspace();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
