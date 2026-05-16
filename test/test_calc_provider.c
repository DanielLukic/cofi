#include <stdio.h>
#include <string.h>

#include "../src/calc_provider.c"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else       { printf("FAIL: %s\n", name); fail++; } \
} while (0)

#define ASSERT_STR(name, a, b) \
    ASSERT_TRUE(name, strcmp((a), (b)) == 0)

static void test_error_result_column_is_wide_enough(void) {
    AppData app = {0};
    CofiRowCells row = {0};
    const char *error = "[error: bad expression]";

    g_strlcpy(app.calc_mode.entries[0].expr, "bad", CALC_EXPR_LEN);
    g_strlcpy(app.calc_mode.entries[0].result, error, CALC_RESULT_LEN);
    app.calc_mode.count = 1;

    calc_format_row(&app, 0, &row);

    ASSERT_STR("calc provider exposes error result", row.cells[0].text, error);
    ASSERT_TRUE("calc provider error result fits column",
                row.cells[0].width_hint >= (int)strlen(error));
}

int main(void) {
    test_error_result_column_is_wide_enough();

    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
