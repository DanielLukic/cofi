#include <stdio.h>
#include <glib.h>

#include "../src/display_pipeline.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("  PASS: %s\n", name); pass++; } \
    else { printf("  FAIL: %s\n", name); fail++; } \
} while (0)

typedef struct {
    GString *order;
} RenderCapture;

typedef struct {
    gboolean called;
    gint total_items;
    gint visible_items;
    gint scroll_offset;
    gint target_columns;
} ScrollbarCapture;

static void render_index_item(gpointer context, gint index, gint selected_idx, GString *text) {
    (void)selected_idx;
    RenderCapture *capture = (RenderCapture *)context;
    g_string_append_printf(capture->order, "%d,", index);
    g_string_append_printf(text, "%d\n", index);
}

static void capture_scrollbar(gpointer context, GString *text, gint total_items,
                              gint visible_items, gint scroll_offset,
                              gint target_columns) {
    (void)text;
    ScrollbarCapture *capture = (ScrollbarCapture *)context;

    capture->called = TRUE;
    capture->total_items = total_items;
    capture->visible_items = visible_items;
    capture->scroll_offset = scroll_offset;
    capture->target_columns = target_columns;
}

static void test_calculate_display_range(void) {
    printf("\n--- calculate_display_range ---\n");

    gint start_idx = -1;
    gint end_idx = -1;

    calculate_display_range(10, 3, 2, &start_idx, &end_idx);
    ASSERT_TRUE("start index uses scroll offset", start_idx == 2);
    ASSERT_TRUE("end index is start + max_lines", end_idx == 5);

    calculate_display_range(4, 10, 0, &start_idx, &end_idx);
    ASSERT_TRUE("start index at top", start_idx == 0);
    ASSERT_TRUE("end index clamped to total", end_idx == 4);
}

static void test_render_display_pipeline_bottom_up_order(void) {
    printf("\n--- render_display_pipeline bottom-up order ---\n");

    RenderCapture capture = {0};
    capture.order = g_string_new("");
    GString *text = g_string_new("");

    DisplayPipelineRequest request = {
        .total_count = 5,
        .max_lines = 3,
        .scroll_offset = 1,
        .selected_idx = 2,
        .target_columns = 80,
        .context = &capture,
        .render_item = render_index_item,
        .overlay_scrollbar = NULL,
    };

    gboolean ok = render_display_pipeline(&request, text);

    ASSERT_TRUE("pipeline renders successfully", ok);
    ASSERT_TRUE("indexes render in reverse visible order", strcmp(capture.order->str, "3,2,1,") == 0);

    g_string_free(text, TRUE);
    g_string_free(capture.order, TRUE);
}

static void test_render_display_pipeline_scrollbar_offset_flip(void) {
    printf("\n--- render_display_pipeline scrollbar offset flip ---\n");

    RenderCapture capture = {0};
    capture.order = g_string_new("");
    GString *text = g_string_new("");

    ScrollbarCapture sb = {0};

    DisplayPipelineRequest request = {
        .total_count = 10,
        .max_lines = 4,
        .scroll_offset = 2,
        .selected_idx = 0,
        .target_columns = 115,
        .context = &capture,
        .render_item = render_index_item,
        .overlay_context = &sb,
        .overlay_scrollbar = capture_scrollbar,
    };

    gboolean ok = render_display_pipeline(&request, text);

    ASSERT_TRUE("pipeline renders successfully", ok);
    ASSERT_TRUE("scrollbar callback called", sb.called);
    ASSERT_TRUE("scrollbar receives flipped offset", sb.scroll_offset == 4);
    ASSERT_TRUE("scrollbar receives total count", sb.total_items == 10);
    ASSERT_TRUE("scrollbar receives max_lines as visible", sb.visible_items == 4);
    ASSERT_TRUE("scrollbar receives target columns", sb.target_columns == 115);

    g_string_free(text, TRUE);
    g_string_free(capture.order, TRUE);
}

int main(void) {
    printf("Display pipeline tests\n");
    printf("======================\n");

    test_calculate_display_range();
    test_render_display_pipeline_bottom_up_order();
    test_render_display_pipeline_scrollbar_offset_flip();

    printf("\n=== Summary: %d/%d passed ===\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
