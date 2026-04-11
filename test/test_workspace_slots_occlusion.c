/*
 * Behavioral regression tests for workspace slot occlusion filtering.
 *
 * TDD refactoring approach: these tests document DESIRED behavior.
 * Before the refactor, some tests fail (documenting the current bugs).
 * After the refactor (rectangle subtraction), all must pass.
 *
 * UX contract: "digit slots = what you SEE (visible content area).
 *              harpoon/fuzzy = what you HAVE (all windows)."
 *
 * Occlusion excludes windows whose visible area falls below a
 * configurable threshold (default 2%). Windows with >= threshold
 * visible area survive. This filters out frame edges and debris
 * while keeping windows showing actual content.
 */

#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>

#include "../src/app_data.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

/* ---- Geometry/stubs ---- */

typedef struct {
    Window id;
    int x, y, w, h;
} TestGeometry;

static TestGeometry test_geometries[MAX_WINDOWS];
static int test_geometry_count = 0;
static int test_current_desktop = 0;

static Window test_stack[MAX_WINDOWS];
static unsigned long test_stack_count = 0;

static void reset_test_state(void) {
    memset(test_geometries, 0, sizeof(test_geometries));
    test_geometry_count = 0;
    test_current_desktop = 0;
    memset(test_stack, 0, sizeof(test_stack));
    test_stack_count = 0;
}

static void add_geometry(Window id, int x, int y, int w, int h) {
    test_geometries[test_geometry_count].id = id;
    test_geometries[test_geometry_count].x = x;
    test_geometries[test_geometry_count].y = y;
    test_geometries[test_geometry_count].w = w;
    test_geometries[test_geometry_count].h = h;
    test_geometry_count++;
}

static void set_stack(Window *stack, int count) {
    memcpy(test_stack, stack, count * sizeof(Window));
    test_stack_count = count;
}

/* Helper: check if slot manager contains a specific window */
static int slot_contains(const WorkspaceSlotManager *mgr, Window id) {
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->slots[i].id == id) return 1;
    }
    return 0;
}

/* ---- X11/config stubs ---- */

int get_current_desktop(Display *display) {
    (void)display;
    return test_current_desktop;
}

int get_window_state(Display *display, Window window, const char *state_name) {
    (void)display;
    (void)window;
    (void)state_name;
    return 0;
}

int get_window_geometry(Display *display, Window window, int *x, int *y, int *w, int *h) {
    (void)display;
    for (int i = 0; i < test_geometry_count; i++) {
        if (test_geometries[i].id == window) {
            *x = test_geometries[i].x;
            *y = test_geometries[i].y;
            *w = test_geometries[i].w;
            *h = test_geometries[i].h;
            return 1;
        }
    }
    return 0;
}

void save_config(const CofiConfig *config) { (void)config; }
void show_slot_overlays(AppData *app) { (void)app; }

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

Atom XInternAtom(Display *display, const char *atom_name, Bool only_if_exists) {
    (void)display; (void)atom_name; (void)only_if_exists;
    return None;
}

int XGetWindowProperty(Display *display, Window window, Atom property,
                       long long_offset, long long_length, Bool del, Atom req_type,
                       Atom *actual_type_return, int *actual_format_return,
                       unsigned long *nitems_return, unsigned long *bytes_after_return,
                       unsigned char **prop_return) {
    (void)display; (void)window; (void)property;
    (void)long_offset; (void)long_length; (void)del; (void)req_type;

    if (prop_return && nitems_return && test_stack_count > 0) {
        Window *buf = malloc(test_stack_count * sizeof(Window));
        memcpy(buf, test_stack, test_stack_count * sizeof(Window));
        *prop_return = (unsigned char *)buf;
        *nitems_return = test_stack_count;
        if (actual_type_return) *actual_type_return = 33; /* XA_WINDOW */
        if (actual_format_return) *actual_format_return = 32;
        if (bytes_after_return) *bytes_after_return = 0;
        return Success;
    }

    if (actual_type_return) *actual_type_return = None;
    if (actual_format_return) *actual_format_return = 0;
    if (nitems_return) *nitems_return = 0;
    if (bytes_after_return) *bytes_after_return = 0;
    if (prop_return) *prop_return = NULL;
    return BadAtom;
}

int XFree(void *data) { free(data); return 0; }

#undef DefaultRootWindow
#define DefaultRootWindow(dpy) ((Window)0)

#include "../src/workspace_slots.c"

/* ==================================================================
 * Test 1: No occlusion — all windows visible, none overlapping.
 *         All should get slots.
 * ================================================================== */
static void test_no_occlusion_all_get_slots(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.window_count = 4;

    reset_test_state();

    for (int i = 0; i < 4; i++) {
        Window id = (Window)(0x100 + i);
        app.windows[i].id = id;
        app.windows[i].desktop = 0;
        strcpy(app.windows[i].type, "Normal");
        add_geometry(id, i * 200, 0, 180, 100);
    }
    Window stack[] = { 0x100, 0x101, 0x102, 0x103 };
    set_stack(stack, 4);

    assign_workspace_slots(&app);

    ASSERT_TRUE("no occlusion: 4 slots", app.workspace_slots.count == 4);
    ASSERT_TRUE("no occlusion: has 0x100", slot_contains(&app.workspace_slots, 0x100));
    ASSERT_TRUE("no occlusion: has 0x103", slot_contains(&app.workspace_slots, 0x103));
}

/* ==================================================================
 * Test 2: Single maximized window fully covers a background window.
 *         Background window should be EXCLUDED (zero visible pixels).
 * ================================================================== */
static void test_fully_covered_by_single_window_is_excluded(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.window_count = 2;

    reset_test_state();

    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 1920, 1080);

    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 0, 1920, 1080);

    Window stack[] = { 0xA, 0xB };
    set_stack(stack, 2);

    assign_workspace_slots(&app);

    ASSERT_TRUE("full cover single: 1 slot", app.workspace_slots.count == 1);
    ASSERT_TRUE("full cover single: top survives", slot_contains(&app.workspace_slots, 0xB));
    ASSERT_TRUE("full cover single: bottom excluded", !slot_contains(&app.workspace_slots, 0xA));
}

/* ==================================================================
 * Test 3: Maximized window covers most of background, but a visible
 *         strip remains on the left. Background should SURVIVE.
 * ================================================================== */
static void test_partially_visible_strip_survives(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.window_count = 2;

    reset_test_state();

    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 1920, 1080);

    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 40, 0, 1880, 1080);

    Window stack[] = { 0xA, 0xB };
    set_stack(stack, 2);

    assign_workspace_slots(&app);

    ASSERT_TRUE("partial strip: 2 slots", app.workspace_slots.count == 2);
    ASSERT_TRUE("partial strip: A survives", slot_contains(&app.workspace_slots, 0xA));
    ASSERT_TRUE("partial strip: B survives", slot_contains(&app.workspace_slots, 0xB));
}

/* ==================================================================
 * Test 4: Real "Side Project" workspace — dual monitor, maximized
 *         layers with realistic geometry.
 *
 * Left monitor (0..3840):
 *   [A] Editor:     x=960,  y=0,  w=2860, h=2112  (100% within Slack)
 *   [B] Slack:      x=0,    y=0,  w=3840, h=2112  (maximized, on top)
 *   Editor is fully covered by Slack → EXCLUDED (0% visible)
 *
 * Right monitor (3840..7680):
 *   [C] JIRA:       x=4800, y=-22, w=2880, h=2156
 *   [D] Figma:      x=4820, y=-22, w=2860, h=2156  (on top)
 *   JIRA has 20px visible strip (x=4800..4820) = 0.69% of area
 *   → EXCLUDED (below 2% threshold: frame edge, not content)
 *
 * Expected: B (left top), D (right top) = 2 slots.
 *           A and C excluded (zero or sub-threshold visible area).
 * ================================================================== */
static void test_real_side_project_workspace(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_COLUMN_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.window_count = 4;

    reset_test_state();

    /* Left: Editor (fully behind Slack) */
    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 960, 0, 2860, 2112);

    /* Left: Slack (maximized, on top) */
    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 0, 3840, 2112);

    /* Right: JIRA (behind Figma, 20px strip visible) */
    app.windows[2].id = 0xC;
    app.windows[2].desktop = 0;
    strcpy(app.windows[2].type, "Normal");
    add_geometry(0xC, 4800, -22, 2880, 2156);

    /* Right: Figma (on top) */
    app.windows[3].id = 0xD;
    app.windows[3].desktop = 0;
    strcpy(app.windows[3].type, "Normal");
    add_geometry(0xD, 4820, -22, 2860, 2156);

    /* Stack: bottom → top: A(Editor), C(JIRA), B(Slack), D(Figma) */
    Window stack[] = { 0xA, 0xC, 0xB, 0xD };
    set_stack(stack, 4);

    assign_workspace_slots(&app);

    ASSERT_TRUE("side project: 2 slots", app.workspace_slots.count == 2);
    ASSERT_TRUE("side project: Slack included", slot_contains(&app.workspace_slots, 0xB));
    ASSERT_TRUE("side project: Figma included", slot_contains(&app.workspace_slots, 0xD));
    ASSERT_TRUE("side project: JIRA excluded (0.69% visible)", !slot_contains(&app.workspace_slots, 0xC));
    ASSERT_TRUE("side project: Editor excluded (0% visible)", !slot_contains(&app.workspace_slots, 0xA));
}

/* ==================================================================
 * Test 5: Multiple windows TOGETHER fully cover a background window,
 *         but none individually covers it fully.
 *         A should be EXCLUDED (zero visible pixels).
 * ================================================================== */
static void test_fully_covered_by_multiple_windows_is_excluded(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.window_count = 3;

    reset_test_state();

    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 1920, 1080);

    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 0, 960, 1080);

    app.windows[2].id = 0xC;
    app.windows[2].desktop = 0;
    strcpy(app.windows[2].type, "Normal");
    add_geometry(0xC, 960, 0, 960, 1080);

    Window stack[] = { 0xA, 0xB, 0xC };
    set_stack(stack, 3);

    assign_workspace_slots(&app);

    ASSERT_TRUE("multi-cover: 2 slots", app.workspace_slots.count == 2);
    ASSERT_TRUE("multi-cover: B included", slot_contains(&app.workspace_slots, 0xB));
    ASSERT_TRUE("multi-cover: C included", slot_contains(&app.workspace_slots, 0xC));
    ASSERT_TRUE("multi-cover: A excluded", !slot_contains(&app.workspace_slots, 0xA));
}

/* ==================================================================
 * Test 6: Multiple windows cover most but NOT all — visible strip.
 *         A should SURVIVE.
 * ================================================================== */
static void test_mostly_covered_but_visible_strip_survives(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.window_count = 3;

    reset_test_state();

    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 1920, 1080);

    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 0, 960, 1080);

    app.windows[2].id = 0xC;
    app.windows[2].desktop = 0;
    strcpy(app.windows[2].type, "Normal");
    add_geometry(0xC, 960, 0, 900, 1080);

    Window stack[] = { 0xA, 0xB, 0xC };
    set_stack(stack, 3);

    assign_workspace_slots(&app);

    ASSERT_TRUE("visible strip: 3 slots", app.workspace_slots.count == 3);
    ASSERT_TRUE("visible strip: A survives", slot_contains(&app.workspace_slots, 0xA));
}

/* ==================================================================
 * Test 7: Overlapping occluders that together cover EVERYTHING.
 *         No double-counting should cause false inclusion.
 *         A should be EXCLUDED.
 * ================================================================== */
static void test_overlapping_occluders_cover_fully(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.window_count = 3;

    reset_test_state();

    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 1000, 1000);

    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 0, 800, 1000);

    app.windows[2].id = 0xC;
    app.windows[2].desktop = 0;
    strcpy(app.windows[2].type, "Normal");
    add_geometry(0xC, 200, 0, 800, 1000);

    Window stack[] = { 0xA, 0xB, 0xC };
    set_stack(stack, 3);

    assign_workspace_slots(&app);

    ASSERT_TRUE("overlap full: 2 slots", app.workspace_slots.count == 2);
    ASSERT_TRUE("overlap full: A excluded", !slot_contains(&app.workspace_slots, 0xA));
}

/* ==================================================================
 * Test 8: Overlapping occluders with a visible gap.
 *         A should SURVIVE (gap at x=900..1000).
 * ================================================================== */
static void test_overlapping_occluders_with_gap_survives(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.window_count = 3;

    reset_test_state();

    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 1000, 1000);

    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 0, 600, 1000);

    app.windows[2].id = 0xC;
    app.windows[2].desktop = 0;
    strcpy(app.windows[2].type, "Normal");
    add_geometry(0xC, 400, 0, 500, 1000);

    Window stack[] = { 0xA, 0xB, 0xC };
    set_stack(stack, 3);

    assign_workspace_slots(&app);

    ASSERT_TRUE("overlap gap: 3 slots", app.workspace_slots.count == 3);
    ASSERT_TRUE("overlap gap: A survives", slot_contains(&app.workspace_slots, 0xA));
}

/* ==================================================================
 * Test 9: 1-pixel visible strip (0.05% of area).
 *         Below any reasonable threshold — frame debris, not content.
 *         A should be EXCLUDED.
 * ================================================================== */
static void test_one_pixel_visible_is_excluded(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.window_count = 2;

    reset_test_state();

    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 1920, 1080);

    /* B covers all but 1px on the right */
    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 0, 1919, 1080);

    Window stack[] = { 0xA, 0xB };
    set_stack(stack, 2);

    assign_workspace_slots(&app);

    ASSERT_TRUE("1px strip: 1 slot", app.workspace_slots.count == 1);
    ASSERT_TRUE("1px strip: A excluded", !slot_contains(&app.workspace_slots, 0xA));
}

/* ==================================================================
 * Test 10: Vertical visible strip (top, 30px = 2.78% of area).
 *          Above the 2% threshold — enough to show real content.
 *          A should SURVIVE.
 * ================================================================== */
static void test_vertical_strip_top_survives(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.window_count = 2;

    reset_test_state();

    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 1920, 1080);

    /* B covers bottom portion, leaves 30px visible at top */
    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 30, 1920, 1050);

    Window stack[] = { 0xA, 0xB };
    set_stack(stack, 2);

    assign_workspace_slots(&app);

    ASSERT_TRUE("vert strip top: 2 slots", app.workspace_slots.count == 2);
    ASSERT_TRUE("vert strip top: A survives", slot_contains(&app.workspace_slots, 0xA));
}

/* ==================================================================
 * Test 11: Stacking order sanity check.
 *          A and B are identical rects. A is below B in the stack.
 *          A should be EXCLUDED (fully covered). B should survive.
 * ================================================================== */
static void test_below_in_stack_does_not_occlude(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.window_count = 2;

    reset_test_state();

    /* A is same size as B, but A is BELOW in stack */
    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 1920, 1080);

    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 0, 1920, 1080);

    /* B is on TOP — A should be excluded, not B */
    Window stack[] = { 0xA, 0xB };
    set_stack(stack, 2);

    assign_workspace_slots(&app);

    ASSERT_TRUE("stack order: 1 slot", app.workspace_slots.count == 1);
    ASSERT_TRUE("stack order: B (top) included", slot_contains(&app.workspace_slots, 0xB));
    ASSERT_TRUE("stack order: A (bottom) excluded", !slot_contains(&app.workspace_slots, 0xA));
}

/* ==================================================================
 * Test 12: Exactly 2% visible area — the boundary.
 *          Policy: >= threshold survives. This test locks that in.
 *          1000x1000 window, 20px strip = exactly 2.0%.
 *          A should SURVIVE (>= threshold).
 * ================================================================== */
static void test_exactly_at_threshold_survives(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.window_count = 2;

    reset_test_state();

    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 1000, 1000);

    /* B covers all but 20px on the right = exactly 2% visible */
    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 0, 980, 1000);

    Window stack[] = { 0xA, 0xB };
    set_stack(stack, 2);

    assign_workspace_slots(&app);

    ASSERT_TRUE("boundary: 2 slots", app.workspace_slots.count == 2);
    ASSERT_TRUE("boundary: A survives (exactly 2%)", slot_contains(&app.workspace_slots, 0xA));
}

/* ==================================================================
 * Test 13: Window not in stacking list at all.
 *          Should survive (treated as non-occluded).
 * ================================================================== */
static void test_window_not_in_stack_survives(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.window_count = 2;

    reset_test_state();

    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 1920, 1080);

    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 0, 1920, 1080);

    /* Stack only contains B — A is not in the stacking list */
    Window stack[] = { 0xB };
    set_stack(stack, 1);

    assign_workspace_slots(&app);

    ASSERT_TRUE("no stack entry: 2 slots", app.workspace_slots.count == 2);
    ASSERT_TRUE("no stack entry: A survives", slot_contains(&app.workspace_slots, 0xA));
}

int main(void) {
    printf("Workspace slot occlusion behavioral tests\n");
    printf("==========================================\n\n");

    test_no_occlusion_all_get_slots();
    test_fully_covered_by_single_window_is_excluded();
    test_partially_visible_strip_survives();
    test_real_side_project_workspace();
    test_fully_covered_by_multiple_windows_is_excluded();
    test_mostly_covered_but_visible_strip_survives();
    test_overlapping_occluders_cover_fully();
    test_overlapping_occluders_with_gap_survives();
    test_one_pixel_visible_is_excluded();
    test_vertical_strip_top_survives();
    test_below_in_stack_does_not_occlude();
    test_exactly_at_threshold_survives();
    test_window_not_in_stack_survives();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
