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

gboolean handle_names_tab_keys(GdkEventKey *event, AppData *app) {
    (void)event;
    (void)app;
    return FALSE;
}

int find_named_window_index(const NamedWindowManager *manager, Window id) {
    if (!manager) return -1;
    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].id == id) return i;
    }
    return -1;
}

int find_named_window_by_name(const NamedWindowManager *manager, const char *custom_name) {
    if (!manager || !custom_name) return -1;
    for (int i = 0; i < manager->count; i++) {
        if (strcmp(manager->entries[i].custom_name, custom_name) == 0) return i;
    }
    return -1;
}

#include "../src/filter_names.c"
#include "../src/names_provider.c"

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    g_reset_selection_calls = 0;
}

static void seed_names(AppData *app) {
    app->names.count = 2;
    app->names.entries[0].id = (Window)0x111;
    app->names.entries[0].assigned = 1;
    g_strlcpy(app->names.entries[0].custom_name, "editor",
              sizeof(app->names.entries[0].custom_name));
    g_strlcpy(app->names.entries[0].original_title, "main.c",
              sizeof(app->names.entries[0].original_title));
    g_strlcpy(app->names.entries[0].class_name, "Code",
              sizeof(app->names.entries[0].class_name));
    g_strlcpy(app->names.entries[0].instance, "code",
              sizeof(app->names.entries[0].instance));

    app->names.entries[1].id = 0;
    app->names.entries[1].assigned = 0;
    g_strlcpy(app->names.entries[1].custom_name, "terminal",
              sizeof(app->names.entries[1].custom_name));
    g_strlcpy(app->names.entries[1].original_title, "shell",
              sizeof(app->names.entries[1].original_title));
    g_strlcpy(app->names.entries[1].class_name, "Mate-terminal",
              sizeof(app->names.entries[1].class_name));
    g_strlcpy(app->names.entries[1].instance, "mate-terminal",
              sizeof(app->names.entries[1].instance));
}

static void test_filter_and_format_row(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);
    seed_names(&app);

    filter_names(&app, "term");

    ASSERT_TRUE("filter narrows named windows", app.filtered_names_count == 1);
    ASSERT_TRUE("filter keeps matching name",
                strcmp(app.filtered_names[0].custom_name, "terminal") == 0);

    memset(&row, 0, sizeof(row));
    names_format_row(&app, 0, &row);
    ASSERT_TRUE("row has four cells", row.cell_count == 4);
    ASSERT_TRUE("row name text", strcmp(row.cells[0].text, "terminal") == 0);
    ASSERT_TRUE("orphan row shows none", strcmp(row.cells[3].text, "* NONE *") == 0);
    ASSERT_TRUE("row is actionable", row.row_flags == COFI_ROW_ACTIONABLE);
}

static void test_empty_row(void) {
    AppData app;
    CofiRowCells row;
    reset_state(&app);

    ASSERT_TRUE("empty provider exposes one status row", names_row_count(&app) == 1);

    memset(&row, 0, sizeof(row));
    names_format_row(&app, 0, &row);
    ASSERT_TRUE("empty row text", strcmp(row.cells[0].text, "No named windows found") == 0);
    ASSERT_TRUE("empty row not actionable", row.row_flags == 0);
}

static void test_query_resets_selection(void) {
    AppData app;
    reset_state(&app);
    seed_names(&app);

    names_on_query_changed(&app, "editor");

    ASSERT_TRUE("query filters", app.filtered_names_count == 1);
    ASSERT_TRUE("query resets selection", g_reset_selection_calls == 1);
}

static void test_selected_entry_and_manager_index(void) {
    AppData app;
    reset_state(&app);
    seed_names(&app);
    filter_names(&app, "");
    app.selection.provider_index = 99;

    NamedWindow *entry = names_selected_entry(&app);

    ASSERT_TRUE("selected entry clamps to last row", entry != NULL &&
                strcmp(entry->custom_name, "terminal") == 0);
    ASSERT_TRUE("provider index clamped", app.selection.provider_index == 1);
    ASSERT_TRUE("manager index resolved by name", names_selected_manager_index(&app) == 1);

    names_select_custom_name(&app, "editor");
    ASSERT_TRUE("select custom name sets provider index", app.selection.provider_index == 0);
}

int main(void) {
    printf("Names provider tests\n");
    printf("====================\n\n");

    test_filter_and_format_row();
    test_empty_row();
    test_query_resets_selection();
    test_selected_entry_and_manager_index();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
