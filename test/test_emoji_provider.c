#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "commands/command_registry.h"
#include "providers/cofi_tab_provider.h"
#include "emoji/emoji_data.h"

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

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

void cofi_init_provider_defaults(CofiTabProvider *p) {
    if (p) memset(p, 0, sizeof(*p));
}

int cofi_register_tab_provider(const CofiTabProvider *p) {
    (void)p;
    return 0;
}

const CofiTabProvider *cofi_get_provider(int provider_id) {
    (void)provider_id;
    return NULL;
}

int cofi_register_command(const CommandSpec *spec) {
    (void)spec;
    return 0;
}

void exit_command_mode(AppData *app) {
    (void)app;
}

void surface_tab(AppData *app, TabMode tab) {
    (void)app;
    (void)tab;
}

#include "emoji/emoji_provider.c"

static void test_row_count_matches_table(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    emoji_on_query_changed(&app, "");
    ASSERT_TRUE("empty query includes full table", app.filtered_emoji_count == EMOJI_TABLE_LEN);
    ASSERT_TRUE("row_count matches filtered table len", emoji_row_count(&app) == EMOJI_TABLE_LEN);
}

static void test_match_string_and_identity(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    emoji_on_query_changed(&app, "");

    const char *match = emoji_match_string(&app, 0);
    const char *id1 = emoji_row_identity(&app, 0);
    const char *id2 = emoji_row_identity(&app, 0);

    ASSERT_TRUE("match string non-null", match != NULL && match[0] != '\0');
    ASSERT_TRUE("row identity non-null", id1 != NULL && id1[0] != '\0');
    ASSERT_TRUE("row identity stable", strcmp(id1, id2) == 0);
}

static void test_format_row_shape(void) {
    AppData app;
    CofiRowCells row;
    memset(&app, 0, sizeof(app));
    memset(&row, 0, sizeof(row));

    emoji_on_query_changed(&app, "joy");
    emoji_format_row(&app, 0, &row);

    ASSERT_TRUE("row has 2 cells", row.cell_count == 2);
    ASSERT_TRUE("cell0 width_hint 2", row.cells[0].width_hint == 2);
    ASSERT_TRUE("cell0 non-empty", row.cells[0].text && row.cells[0].text[0] != '\0');
    ASSERT_TRUE("cell1 width_hint 0", row.cells[1].width_hint == 0);
    ASSERT_TRUE("cell1 non-empty", row.cells[1].text && row.cells[1].text[0] != '\0');
}

static void test_query_filtering_contract(void) {
    AppData app;
    memset(&app, 0, sizeof(app));

    emoji_on_query_changed(&app, "joy");
    ASSERT_TRUE("joy query yields some results", app.filtered_emoji_count > 0);
    ASSERT_TRUE("joy query yields subset", app.filtered_emoji_count < EMOJI_TABLE_LEN);
    ASSERT_TRUE("row_count follows joy filter", emoji_row_count(&app) == app.filtered_emoji_count);

    int found_joy = 0;
    for (int i = 0; i < app.filtered_emoji_count; i++) {
        int idx = app.filtered_emoji[i];
        if (strcmp(EMOJI_TABLE[idx].glyph, "😂") == 0 &&
            strstr(EMOJI_TABLE[idx].name, "tears of joy") != NULL) {
            found_joy = 1;
            break;
        }
    }
    ASSERT_TRUE("joy filter includes tears of joy", found_joy == 1);

    emoji_on_query_changed(&app, "zzzzzzzz");
    ASSERT_TRUE("no-match query has zero rows", app.filtered_emoji_count == 0);
    ASSERT_TRUE("row_count zero for no-match query", emoji_row_count(&app) == 0);
}

int main(void) {
    printf("Emoji provider tests\n");
    printf("====================\n\n");

    test_row_count_matches_table();
    test_match_string_and_identity();
    test_format_row_shape();
    test_query_filtering_contract();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_run == tests_passed ? 0 : 1;
}
