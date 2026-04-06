#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "../src/app_data.h"
#include "../src/dynamic_display.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("  PASS: %s\n", name); pass++; } \
    else { printf("  FAIL: %s\n", name); fail++; } \
} while (0)

static void test_calculate_fixed_window_grid_sane_defaults(void) {
    printf("\n--- calculate_fixed_window_grid sane defaults ---\n");

    FixedWindowGridConfig cfg = {
        .target_columns = 115,
        .visible_rows = 22,
        .chrome_rows = 4,
        .min_columns = 80,
        .min_rows = 8,
    };

    gint cols = 0;
    gint rows = 0;
    gint width_px = 0;
    gint height_px = 0;

    gboolean ok = calculate_fixed_window_grid(9, 22, &cfg, &cols, &rows, &width_px, &height_px);

    ASSERT_TRUE("calculation succeeds", ok);
    ASSERT_TRUE("columns are target width", cols == 115);
    ASSERT_TRUE("rows are visible content rows", rows == 22);
    ASSERT_TRUE("pixel width uses char width", width_px == 1035);
    ASSERT_TRUE("pixel height includes chrome rows", height_px == 572);
}

static void test_calculate_fixed_window_grid_rejects_invalid_inputs(void) {
    printf("\n--- calculate_fixed_window_grid invalid inputs ---\n");

    FixedWindowGridConfig cfg = {
        .target_columns = 115,
        .visible_rows = 22,
        .chrome_rows = 4,
        .min_columns = 80,
        .min_rows = 8,
    };

    gint cols = 0;
    gint rows = 0;
    gint width_px = 0;
    gint height_px = 0;

    ASSERT_TRUE("rejects char_width <= 0",
                !calculate_fixed_window_grid(0, 22, &cfg, &cols, &rows, &width_px, &height_px));
    ASSERT_TRUE("rejects line_height <= 0",
                !calculate_fixed_window_grid(9, 0, &cfg, &cols, &rows, &width_px, &height_px));
}

static void test_get_display_columns_uses_fixed_authority(void) {
    printf("\n--- get_display_columns uses fixed authority ---\n");

    AppData app = {0};
    app.fixed_cols = 123;

    gint columns = get_display_columns(&app);
    ASSERT_TRUE("returns fixed_cols directly", columns == 123);
}

static void test_get_dynamic_max_display_lines_uses_fixed_rows(void) {
    printf("\n--- get_dynamic_max_display_lines uses fixed rows ---\n");

    AppData app = {0};
    app.fixed_rows = 31;

    gint lines = get_dynamic_max_display_lines(&app);
    ASSERT_TRUE("returns fixed_rows directly", lines == 31);
}

int main(void) {
    printf("Dynamic fixed-window sizing tests\n");
    printf("===============================\n");

    test_calculate_fixed_window_grid_sane_defaults();
    test_calculate_fixed_window_grid_rejects_invalid_inputs();
    test_get_display_columns_uses_fixed_authority();
    test_get_dynamic_max_display_lines_uses_fixed_rows();

    printf("\n=== Summary: %d/%d passed ===\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
