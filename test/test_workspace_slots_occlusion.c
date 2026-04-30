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
 * configurable threshold (default 5%). Windows with >= threshold
 * visible area survive. This filters out frame edges, debris,
 * and windows mostly dragged off-screen.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "../src/app_data.h"
#include "../src/workarea.h"

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
static Window test_hidden[MAX_WINDOWS];
static int test_hidden_count = 0;
static Window test_below[MAX_WINDOWS];
static int test_below_count = 0;

typedef struct {
    Window id;
    int left, right, top, bottom;
} TestFrameExtents;

typedef struct {
    int x, y, w, h;
} TestMonitor;

static TestFrameExtents test_fe_table[MAX_WINDOWS];
static int test_fe_count = 0;
static TestMonitor test_monitors[MAX_WINDOWS];
static int test_monitor_count = 0;
static WorkArea test_work_area = {0};
static int test_have_work_area = 0;

static void reset_test_state(void) {
    memset(test_geometries, 0, sizeof(test_geometries));
    test_geometry_count = 0;
    test_current_desktop = 0;
    memset(test_stack, 0, sizeof(test_stack));
    test_stack_count = 0;
    memset(test_hidden, 0, sizeof(test_hidden));
    test_hidden_count = 0;
    memset(test_below, 0, sizeof(test_below));
    test_below_count = 0;
    memset(test_fe_table, 0, sizeof(test_fe_table));
    test_fe_count = 0;
    memset(test_monitors, 0, sizeof(test_monitors));
    test_monitor_count = 0;
    test_work_area = (WorkArea){0};
    test_have_work_area = 0;
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

static void add_hidden(Window id) {
    test_hidden[test_hidden_count++] = id;
}

static void add_below(Window id) {
    test_below[test_below_count++] = id;
}

static void add_frame_extents(Window id, int left, int top, int right, int bottom) {
    test_fe_table[test_fe_count].id     = id;
    test_fe_table[test_fe_count].left   = left;
    test_fe_table[test_fe_count].right  = right;
    test_fe_table[test_fe_count].top    = top;
    test_fe_table[test_fe_count].bottom = bottom;
    test_fe_count++;
}

static void add_monitor(int x, int y, int w, int h,
                        int wx, int wy, int ww, int wh) {
    test_monitors[test_monitor_count++] = (TestMonitor){ x, y, w, h };
    test_work_area = (WorkArea){ wx, wy, ww, wh };
    test_have_work_area = 1;
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
    if (strcmp(state_name, "_NET_WM_STATE_HIDDEN") == 0) {
        for (int i = 0; i < test_hidden_count; i++) {
            if (test_hidden[i] == window) return 1;
        }
        return 0;
    }
    if (strcmp(state_name, "_NET_WM_STATE_BELOW") == 0) {
        for (int i = 0; i < test_below_count; i++) {
            if (test_below[i] == window) return 1;
        }
        return 0;
    }
    if (strcmp(state_name, "_NET_WM_STATE_SHADED") == 0) return 0;
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

int get_current_work_area(Display *display, WorkArea *work_area) {
    (void)display;
    if (!test_have_work_area) return 0;
    if (work_area) *work_area = test_work_area;
    return 1;
}

int XRRQueryExtension(Display *display, int *event_base_return, int *error_base_return) {
    (void)display;
    if (event_base_return) *event_base_return = 0;
    if (error_base_return) *error_base_return = 0;
    return test_monitor_count > 0;
}

XRRScreenResources *XRRGetScreenResources(Display *display, Window window) {
    (void)display;
    (void)window;
    if (test_monitor_count <= 0) return NULL;

    XRRScreenResources *resources = calloc(1, sizeof(*resources));
    resources->noutput = test_monitor_count;
    resources->outputs = calloc(test_monitor_count, sizeof(RROutput));
    for (int i = 0; i < test_monitor_count; i++) {
        resources->outputs[i] = (RROutput)(i + 1);
    }
    return resources;
}

XRROutputInfo *XRRGetOutputInfo(Display *display, XRRScreenResources *resources, RROutput output) {
    (void)display;
    (void)resources;
    int index = (int)output - 1;
    if (index < 0 || index >= test_monitor_count) return NULL;

    XRROutputInfo *info = calloc(1, sizeof(*info));
    info->connection = RR_Connected;
    info->crtc = (RRCrtc)output;
    return info;
}

XRRCrtcInfo *XRRGetCrtcInfo(Display *display, XRRScreenResources *resources, RRCrtc crtc) {
    (void)display;
    (void)resources;
    int index = (int)crtc - 1;
    if (index < 0 || index >= test_monitor_count) return NULL;

    XRRCrtcInfo *info = calloc(1, sizeof(*info));
    info->x = test_monitors[index].x;
    info->y = test_monitors[index].y;
    info->width = test_monitors[index].w;
    info->height = test_monitors[index].h;
    return info;
}

void XRRFreeScreenResources(XRRScreenResources *resources) {
    if (!resources) return;
    free(resources->outputs);
    free(resources);
}

void XRRFreeOutputInfo(XRROutputInfo *output_info) { free(output_info); }
void XRRFreeCrtcInfo(XRRCrtcInfo *crtc_info) { free(crtc_info); }

#undef DefaultRootWindow
#define DefaultRootWindow(dpy) ((Window)0)

/* Strong definition overrides the weak extern in workspace_slots.c.
 * Returns frame extents from the test table, or 0 (failure) if not found.
 * Windows not in the table get zero insets → content_rect = outer_rect,
 * preserving all existing test behavior unchanged. */
#include "../src/frame_extents.h"
int get_frame_extents(Display *display, Window window, FrameExtents *extents) {
    (void)display;
    for (int i = 0; i < test_fe_count; i++) {
        if (test_fe_table[i].id == window) {
            extents->left   = test_fe_table[i].left;
            extents->right  = test_fe_table[i].right;
            extents->top    = test_fe_table[i].top;
            extents->bottom = test_fe_table[i].bottom;
            return 1;
        }
    }
    return 0;
}

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
    app.config.slot_occlusion_threshold_pct = 5;
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
    app.config.slot_occlusion_threshold_pct = 5;
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
 *         strip remains on the left (100px = 5.2% visible).
 *         Background should SURVIVE.
 * ================================================================== */
static void test_partially_visible_strip_survives(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 2;

    reset_test_state();

    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 1920, 1080);

    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 100, 0, 1820, 1080);

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
    app.config.slot_occlusion_threshold_pct = 5;
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
    app.config.slot_occlusion_threshold_pct = 5;
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
 * Test 6: Multiple windows cover most but NOT all — 120px visible strip
 *         (6.25% visible). A should SURVIVE.
 * ================================================================== */
static void test_mostly_covered_but_visible_strip_survives(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
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
    add_geometry(0xC, 960, 0, 840, 1080);

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
    app.config.slot_occlusion_threshold_pct = 5;
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
    app.config.slot_occlusion_threshold_pct = 5;
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
    app.config.slot_occlusion_threshold_pct = 5;
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
 * Test 10: Vertical visible strip (top, 60px = 5.56% of area).
 *          Above the 5% threshold — enough to show real content.
 *          A should SURVIVE.
 * ================================================================== */
static void test_vertical_strip_top_survives(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 2;

    reset_test_state();

    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 1920, 1080);

    /* B covers bottom portion, leaves 60px visible at top */
    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 60, 1920, 1020);

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
    app.config.slot_occlusion_threshold_pct = 5;
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
 * Test 12: Exactly 5% visible area — the boundary.
 *          Policy: >= threshold survives. This test locks that in.
 *          1000x1000 window, 50px strip = exactly 5.0%.
 *          A should SURVIVE (>= threshold).
 * ================================================================== */
static void test_exactly_at_threshold_survives(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 2;

    reset_test_state();

    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 1000, 1000);

    /* B covers all but 50px on the right = exactly 5% visible */
    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 0, 950, 1000);

    Window stack[] = { 0xA, 0xB };
    set_stack(stack, 2);

    assign_workspace_slots(&app);

    ASSERT_TRUE("boundary: 2 slots", app.workspace_slots.count == 2);
    ASSERT_TRUE("boundary: A survives (exactly 5%)", slot_contains(&app.workspace_slots, 0xA));
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
    app.config.slot_occlusion_threshold_pct = 5;
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

/* ==================================================================
 * Test 14: Thin decoration strip with area > 5% but height < 8px.
 *          Must be excluded by min-dimension check.
 * ================================================================== */
static void test_thin_decoration_strip_excluded_by_dimension(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 2;

    reset_test_state();

    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 3840, 55);

    /* B covers all but a 3px strip at the bottom of A */
    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 0, 3840, 52);

    Window stack[] = { 0xA, 0xB };
    set_stack(stack, 2);

    assign_workspace_slots(&app);

    ASSERT_TRUE("thin dim strip: 1 slot", app.workspace_slots.count == 1);
    ASSERT_TRUE("thin dim strip: A excluded", !slot_contains(&app.workspace_slots, 0xA));
    ASSERT_TRUE("thin dim strip: B survives", slot_contains(&app.workspace_slots, 0xB));
}

/* ==================================================================
 * Test 16: Dimension boundary - exactly 8px in smallest dim survives.
 *          Visible fragment 8px tall, area well above threshold.
 * ================================================================== */
static void test_dimension_boundary_exactly_8_survives(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 2;

    reset_test_state();

    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 100, 100);

    /* B covers top 92px, leaves a 100x8 bottom fragment (exactly MIN_VISIBLE_DIM_PX) */
    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 0, 100, 92);

    Window stack[] = { 0xA, 0xB };
    set_stack(stack, 2);

    assign_workspace_slots(&app);

    ASSERT_TRUE("dim boundary 8: 2 slots", app.workspace_slots.count == 2);
    ASSERT_TRUE("dim boundary 8: A survives", slot_contains(&app.workspace_slots, 0xA));
    ASSERT_TRUE("dim boundary 8: B survives", slot_contains(&app.workspace_slots, 0xB));
}

/* ==================================================================
 * Test 17: Dimension boundary - 7px in smallest dim is excluded.
 *          Visible fragment 7px tall, area passes but dimension fails.
 * ================================================================== */
static void test_dimension_boundary_7_excluded(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 2;

    reset_test_state();

    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 100, 100);

    /* B covers top 93px, leaves a 100x7 bottom fragment (just under MIN_VISIBLE_DIM_PX) */
    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 0, 100, 93);

    Window stack[] = { 0xA, 0xB };
    set_stack(stack, 2);

    assign_workspace_slots(&app);

    ASSERT_TRUE("dim boundary 7: 1 slot", app.workspace_slots.count == 1);
    ASSERT_TRUE("dim boundary 7: A excluded", !slot_contains(&app.workspace_slots, 0xA));
    ASSERT_TRUE("dim boundary 7: B survives", slot_contains(&app.workspace_slots, 0xB));
}
/* ==================================================================
 * Test 15: Small but chunky visible fragment (100x10) should survive
 *          when area and min dimensions both pass.
 * ================================================================== */
static void test_small_chunky_fragment_survives(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 2;

    reset_test_state();

    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 0, 0, 100, 100);

    /* B covers top 90px, leaves a 100x10 bottom fragment */
    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 0, 100, 90);

    Window stack[] = { 0xA, 0xB };
    set_stack(stack, 2);

    assign_workspace_slots(&app);

    ASSERT_TRUE("chunky fragment: 2 slots", app.workspace_slots.count == 2);
    ASSERT_TRUE("chunky fragment: A survives", slot_contains(&app.workspace_slots, 0xA));
    ASSERT_TRUE("chunky fragment: B survives", slot_contains(&app.workspace_slots, 0xB));
}

/* ==================================================================
 * Test 18: Real SP4 workspace geometry (desktop 7 snapshot).
 *          Four terminals are tiled below a maximized Chrome on the
 *          right monitor; hidden Kraken window must be ignored.
 *          Expected survivors: Chrome-L and Chrome-R only.
 * ================================================================== */
static void test_real_sp4_workspace(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_COLUMN_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 7;

    reset_test_state();

    /* 0x4106885 Term-D (bottom-right quarter) */
    app.windows[0].id = 0x4106885;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0x4106885, 5780, 1114, 1880, 978);

    /* 0x400d858 Term-B (bottom-left quarter) */
    app.windows[1].id = 0x400d858;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0x400d858, 3860, 1114, 1880, 978);

    /* 0x4c003a9 Kraken (hidden, should be ignored) */
    app.windows[2].id = 0x4c003a9;
    app.windows[2].desktop = 0;
    strcpy(app.windows[2].type, "Normal");
    add_geometry(0x4c003a9, 3840, 0, 3840, 2112);
    add_hidden(0x4c003a9);

    /* 0x412b6b0 Term-C (top-right quarter) */
    app.windows[3].id = 0x412b6b0;
    app.windows[3].desktop = 0;
    strcpy(app.windows[3].type, "Normal");
    add_geometry(0x412b6b0, 5780, 58, 1880, 978);

    /* 0x411a4bc Chrome-L (left monitor maximized) */
    app.windows[4].id = 0x411a4bc;
    app.windows[4].desktop = 0;
    strcpy(app.windows[4].type, "Normal");
    add_geometry(0x411a4bc, 0, 6, 3840, 2104);

    /* 0x400d721 Term-A (top-left quarter) */
    app.windows[5].id = 0x400d721;
    app.windows[5].desktop = 0;
    strcpy(app.windows[5].type, "Normal");
    add_geometry(0x400d721, 3860, 58, 1880, 978);

    /* 0x4c001fc Chrome-R (right monitor maximized, top of stack) */
    app.windows[6].id = 0x4c001fc;
    app.windows[6].desktop = 0;
    strcpy(app.windows[6].type, "Normal");
    add_geometry(0x4c001fc, 3840, 0, 3840, 2112);

    Window stack[] = {
        0x4106885, 0x400d858, 0x4c003a9, 0x412b6b0, 0x411a4bc, 0x400d721, 0x4c001fc
    };
    set_stack(stack, 7);

    assign_workspace_slots(&app);

    ASSERT_TRUE("sp4: 2 slots", app.workspace_slots.count == 2);
    ASSERT_TRUE("sp4: Chrome-L included", slot_contains(&app.workspace_slots, 0x411a4bc));
    ASSERT_TRUE("sp4: Chrome-R included", slot_contains(&app.workspace_slots, 0x4c001fc));

    ASSERT_TRUE("sp4: Term-A excluded", !slot_contains(&app.workspace_slots, 0x400d721));
    ASSERT_TRUE("sp4: Term-B excluded", !slot_contains(&app.workspace_slots, 0x400d858));
    ASSERT_TRUE("sp4: Term-C excluded", !slot_contains(&app.workspace_slots, 0x412b6b0));
    ASSERT_TRUE("sp4: Term-D excluded", !slot_contains(&app.workspace_slots, 0x4106885));
}

/* ==================================================================
 * Test 21: Workspace 1 real geometry — Thunderbird maximized behind
 *          two tiled terminals.
 *
 *  Monitor: 3840×2112
 *  Thunderbird: (0,0) 3840×2112 — maximized, bottom of stack
 *  Left terminal: outer (20,58) 1880×2034, frame {left=20,top=58,right=20,bot=20}
 *  Right terminal: outer (1940,58) 1880×2034, same frame extents
 *  Bottom dock: sticky, type Dock, (0,2112) 3840×48 — should not get a slot
 *
 *  Thunderbird has ~5.7% total area visible (thin strips at top, sides,
 *  center gap, bottom) but the largest single fragment is 1880×58 =
 *  109,040 px = 1.34% of 8,110,080 — well below the 5% threshold.
 *  "fragmented edge visibility" → Thunderbird is EXCLUDED.
 *
 *  Terminals: unoccluded (dock doesn't overlap them), full content
 *  area visible → both SURVIVE.
 * ================================================================== */
static void test_workspace1_tiled_terminals_thunderbird(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 4;

    reset_test_state();

    /* Thunderbird: maximized, no frame extents (WM removes decorations) */
    app.windows[0].id = 0xAA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xAA, 0, 0, 3840, 2112);

    /* Left terminal: outer rect starts at (20,58), frame top=58 */
    app.windows[1].id = 0xBB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xBB, 20, 58, 1880, 2034);
    add_frame_extents(0xBB, 20, 58, 20, 20);  /* left, top, right, bottom */

    /* Right terminal: same geometry, right half of screen */
    app.windows[2].id = 0xCC;
    app.windows[2].desktop = 0;
    strcpy(app.windows[2].type, "Normal");
    add_geometry(0xCC, 1940, 58, 1880, 2034);
    add_frame_extents(0xCC, 20, 58, 20, 20);

    /* Bottom dock: sticky, Dock type — must NOT receive a digit slot */
    app.windows[3].id = 0xDD;
    app.windows[3].desktop = -1;
    strcpy(app.windows[3].type, "Dock");
    add_geometry(0xDD, 0, 2112, 3840, 48);

    /* Thunderbird bottom of stack; terminals above it; dock topmost */
    Window stack[] = { 0xAA, 0xBB, 0xCC, 0xDD };
    set_stack(stack, 4);

    assign_workspace_slots(&app);

    /* Two slots: one per terminal */
    ASSERT_TRUE("ws1: 2 slots",                       app.workspace_slots.count == 2);
    ASSERT_TRUE("ws1: left terminal included",         slot_contains(&app.workspace_slots, 0xBB));
    ASSERT_TRUE("ws1: right terminal included",        slot_contains(&app.workspace_slots, 0xCC));
    ASSERT_TRUE("ws1: Thunderbird excluded (fragmented)", !slot_contains(&app.workspace_slots, 0xAA));
    ASSERT_TRUE("ws1: dock excluded (not Normal type)",   !slot_contains(&app.workspace_slots, 0xDD));
}

/* ==================================================================
 * Test 19: Frame extents — decoration strip excluded when content
 *          is fully behind an occluder.
 *
 *          Reproduces the SP4 bug: terminal has outer y=-20 (WM frame),
 *          Chrome starts at y=0 and covers the full content area.
 *          The 20px visible strip is the titlebar decoration — not
 *          real content. After clipping to the content rect it clips
 *          to zero area → window is EXCLUDED.
 *
 *          A: outer (100,-20) size 1920×1120, frame.top=20
 *             content rect = (100,0)-(2020,1100) = 1920×1100
 *          B: outer (0,0) size 3840×1100, ON TOP, covers all of A's content.
 *          Expected: B survives, A excluded.
 * ================================================================== */
static void test_frame_extents_titlebar_strip_excluded(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 2;

    reset_test_state();

    /* A: terminal with 20px WM titlebar above content */
    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 100, -20, 1920, 1120);  /* outer: y=-20, h includes titlebar */
    add_frame_extents(0xA, 0, 20, 0, 0);      /* top inset = 20px */

    /* B: Chrome, starts at y=0, fully covers A's content rect */
    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 0, 0, 3840, 1100);

    /* B is on top */
    Window stack[] = { 0xA, 0xB };
    set_stack(stack, 2);

    assign_workspace_slots(&app);

    ASSERT_TRUE("fe strip: 1 slot", app.workspace_slots.count == 1);
    ASSERT_TRUE("fe strip: B (top) survives", slot_contains(&app.workspace_slots, 0xB));
    ASSERT_TRUE("fe strip: A (titlebar only) excluded", !slot_contains(&app.workspace_slots, 0xA));
}

/* ==================================================================
 * Test 20: Frame extents — partial content visible despite frame offset.
 *
 *          Same terminal frame offset (outer y=-20, fe_top=20), but
 *          the occluder only covers the LEFT half of the content rect.
 *          Right half remains visible: 480×1100 / 960×1100 = 50%.
 *          A SURVIVES.
 *
 *          A: outer (100,-20) size 960×1120, frame.top=20
 *             content rect = (100,0)-(1060,1100) = 960×1100
 *          B: outer (100,0) size 480×1100, ON TOP, covers left half.
 *          Expected: both survive (A at 50%, B unoccluded).
 * ================================================================== */
static void test_frame_extents_partial_content_survives(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 2;

    reset_test_state();

    /* A: terminal, fe_top=20, only left half covered by B */
    app.windows[0].id = 0xA;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA, 100, -20, 960, 1120);
    add_frame_extents(0xA, 0, 20, 0, 0);

    /* B: covers exactly the left half of A's content rect */
    app.windows[1].id = 0xB;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB, 100, 0, 480, 1100);

    /* B on top */
    Window stack[] = { 0xA, 0xB };
    set_stack(stack, 2);

    assign_workspace_slots(&app);

    /* A has 50% content visible — well above 5% threshold */
    ASSERT_TRUE("fe partial: 2 slots", app.workspace_slots.count == 2);
    ASSERT_TRUE("fe partial: A survives (50% content)", slot_contains(&app.workspace_slots, 0xA));
    ASSERT_TRUE("fe partial: B survives", slot_contains(&app.workspace_slots, 0xB));
}

/* ==================================================================
 * Test 21: Workspace 1 left monitor snapshot.
 *          Thunderbird is maximized behind two tiled terminals.
 *          Geometric leftovers (top strip, side strips, center gap)
 *          are not meaningfully reachable by eye and should not earn
 *          a digit slot. Only the front terminals should survive.
 * ================================================================== */
static void test_workspace1_thunderbird_behind_two_terminals_is_excluded(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_COLUMN_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 3;

    reset_test_state();

    app.windows[0].id = 0xA1;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA1, 0, 0, 3840, 2112);

    app.windows[1].id = 0xB1;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB1, 20, 58, 1880, 2034);
    add_frame_extents(0xB1, 20, 20, 58, 20);

    app.windows[2].id = 0xC1;
    app.windows[2].desktop = 0;
    strcpy(app.windows[2].type, "Normal");
    add_geometry(0xC1, 1940, 58, 1880, 2034);
    add_frame_extents(0xC1, 20, 20, 58, 20);

    /* Thunderbird below, terminals above */
    Window stack[] = { 0xA1, 0xC1, 0xB1 };
    set_stack(stack, 3);

    assign_workspace_slots(&app);

    ASSERT_TRUE("ws1 tb: 2 slots", app.workspace_slots.count == 2);
    ASSERT_TRUE("ws1 tb: left terminal survives", slot_contains(&app.workspace_slots, 0xB1));
    ASSERT_TRUE("ws1 tb: right terminal survives", slot_contains(&app.workspace_slots, 0xC1));
    ASSERT_TRUE("ws1 tb: thunderbird excluded", !slot_contains(&app.workspace_slots, 0xA1));
}

static void test_no_monitor_clips_keeps_full_visibility(void) {
    WindowPosition win = {
        .id = 0x1, .x = 10, .y = -30, .w = 200, .h = 100,
        .frame = {0}
    };
    int overlay_x = 0, overlay_y = 0, largest_w = 0, largest_h = 0;
    double largest_fraction = 0.0;
    double visible_fraction = compute_visible_fraction_and_overlay_center_for_clips(
        &win, -1, &win, 1, NULL, 0, NULL, 0,
        &overlay_x, &overlay_y, &largest_w, &largest_h, &largest_fraction);

    ASSERT_TRUE("no clip fallback: full visible", visible_fraction == 1.0);
    ASSERT_TRUE("no clip fallback: full fragment", largest_fraction == 1.0);
    ASSERT_TRUE("no clip fallback: center kept", overlay_x == 110 && overlay_y == 20);
    ASSERT_TRUE("no clip fallback: size kept", largest_w == 200 && largest_h == 100);
}

static void test_negative_y_without_occluder_is_excluded_by_monitor_clip(void) {
    AppData app = {0};
    app.display = (Display *)0x1;
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 1;

    reset_test_state();
    add_monitor(0, 0, 1000, 1000, 0, 0, 1000, 1000);

    app.windows[0].id = 0xA2;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA2, 0, -970, 1000, 1000);

    assign_workspace_slots(&app);

    ASSERT_TRUE("clip neg-y: excluded", app.workspace_slots.count == 0);
}

static void test_slight_negative_y_without_occluder_still_survives(void) {
    AppData app = {0};
    app.display = (Display *)0x1;
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 1;

    reset_test_state();
    add_monitor(0, 0, 1000, 1000, 0, 0, 1000, 1000);

    app.windows[0].id = 0xA3;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA3, 0, -20, 1000, 1000);

    assign_workspace_slots(&app);

    ASSERT_TRUE("clip slight neg-y: survives", app.workspace_slots.count == 1);
    ASSERT_TRUE("clip slight neg-y: window present", slot_contains(&app.workspace_slots, 0xA3));
}

static void test_missing_stack_entry_still_clipped(void) {
    AppData app = {0};
    app.display = (Display *)0x1;
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 2;

    reset_test_state();
    add_monitor(0, 0, 1000, 1000, 0, 0, 1000, 1000);

    app.windows[0].id = 0xA4;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA4, 0, -970, 1000, 1000);

    app.windows[1].id = 0xB4;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB4, 0, 0, 1000, 1000);

    Window stack[] = { 0xB4 };
    set_stack(stack, 1);

    assign_workspace_slots(&app);

    ASSERT_TRUE("clip missing stack: 1 slot", app.workspace_slots.count == 1);
    ASSERT_TRUE("clip missing stack: offscreen excluded", !slot_contains(&app.workspace_slots, 0xA4));
    ASSERT_TRUE("clip missing stack: onscreen survives", slot_contains(&app.workspace_slots, 0xB4));
}

static void test_below_window_fully_covered_by_normal_occluder_is_excluded(void) {
    AppData app = {0};
    app.display = (Display *)0x1;
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 2;

    reset_test_state();
    add_monitor(3000, 0, 1000, 1000, 3000, 0, 1000, 1000);

    app.windows[0].id = 0xB10;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xB10, 3100, 100, 400, 400);
    add_below(0xB10);

    app.windows[1].id = 0xB11;
    app.windows[1].desktop = 0;
    strcpy(app.windows[1].type, "Normal");
    add_geometry(0xB11, 3100, 100, 400, 400);

    Window stack[] = { 0xB10, 0xB11 };
    set_stack(stack, 2);

    assign_workspace_slots(&app);

    ASSERT_TRUE("below covered: 1 slot", app.workspace_slots.count == 1);
    ASSERT_TRUE("below covered: below window excluded", !slot_contains(&app.workspace_slots, 0xB10));
    ASSERT_TRUE("below covered: top occluder survives", slot_contains(&app.workspace_slots, 0xB11));
}

static void test_below_window_without_occluders_is_included(void) {
    AppData app = {0};
    app.display = (Display *)0x1;
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 1;

    reset_test_state();
    add_monitor(3000, 0, 1000, 1000, 3000, 0, 1000, 1000);

    app.windows[0].id = 0xB12;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xB12, 3100, 100, 400, 400);
    add_below(0xB12);

    assign_workspace_slots(&app);

    ASSERT_TRUE("below clear: 1 slot", app.workspace_slots.count == 1);
    ASSERT_TRUE("below clear: candidate survives", slot_contains(&app.workspace_slots, 0xB12));
}

static void test_dead_monitor_seam_pixels_do_not_count(void) {
    AppData app = {0};
    app.display = (Display *)0x1;
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.config.slot_occlusion_threshold_pct = 5;
    app.window_count = 1;

    reset_test_state();
    add_monitor(0, 0, 1000, 1000, 0, 0, 1000, 1000);
    add_monitor(1200, 0, 1000, 1000, 1200, 0, 1000, 1000);

    app.windows[0].id = 0xA5;
    app.windows[0].desktop = 0;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0xA5, 1020, 0, 160, 1000);

    assign_workspace_slots(&app);

    ASSERT_TRUE("clip seam gap: excluded", app.workspace_slots.count == 0);
}

static void test_overlay_center_comes_from_clipped_fragment(void) {
    WindowPosition win = {
        .id = 0xA6, .x = 0, .y = -300, .w = 100, .h = 400,
        .frame = {0}
    };
    Rect clip_rects[] = { { 0, 0, 100, 100 } };
    int overlay_x = 0, overlay_y = 0, largest_w = 0, largest_h = 0;
    double largest_fraction = 0.0;
    double visible_fraction = compute_visible_fraction_and_overlay_center_for_clips(
        &win, -1, &win, 1, NULL, 0, clip_rects, 1,
        &overlay_x, &overlay_y, &largest_w, &largest_h, &largest_fraction);

    ASSERT_TRUE("clip overlay: fraction clipped", visible_fraction == 0.25);
    ASSERT_TRUE("clip overlay: center in fragment", overlay_x == 50 && overlay_y == 50);
    ASSERT_TRUE("clip overlay: fragment dims clipped", largest_w == 100 && largest_h == 100);
    ASSERT_TRUE("clip overlay: fragment fraction clipped", largest_fraction == 0.25);
}

static void test_physical_dual_monitor_candidate_stays_visible(void) {
    WindowPosition win = {
        .id = 0xA7, .x = 3840, .y = 6, .w = 3840, .h = 2104,
        .frame = {0}
    };
    Rect clip_rects[] = {
        { 0, 0, 3840, 2160 },
        { 3840, 0, 7680, 2160 }
    };
    int overlay_x = 0, overlay_y = 0, largest_w = 0, largest_h = 0;
    double largest_fraction = 0.0;
    double visible_fraction = compute_visible_fraction_and_overlay_center_for_clips(
        &win, -1, &win, 1, NULL, 0, clip_rects, 2,
        &overlay_x, &overlay_y, &largest_w, &largest_h, &largest_fraction);

    ASSERT_TRUE("physical dual-monitor clip: >=95% visible", visible_fraction >= 0.95);
    ASSERT_TRUE("physical dual-monitor clip: full fragment", largest_fraction >= 0.95);
    ASSERT_TRUE("physical dual-monitor clip: overlay centered", overlay_x == 5760 && overlay_y == 1058);
    ASSERT_TRUE("physical dual-monitor clip: fragment dims", largest_w == 3840 && largest_h == 2104);
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
    test_thin_decoration_strip_excluded_by_dimension();
    test_small_chunky_fragment_survives();
    test_dimension_boundary_exactly_8_survives();
    test_dimension_boundary_7_excluded();
    test_real_sp4_workspace();
    test_workspace1_tiled_terminals_thunderbird();
    test_frame_extents_titlebar_strip_excluded();
    test_frame_extents_partial_content_survives();
    test_workspace1_thunderbird_behind_two_terminals_is_excluded();
    test_no_monitor_clips_keeps_full_visibility();
    test_negative_y_without_occluder_is_excluded_by_monitor_clip();
    test_slight_negative_y_without_occluder_still_survives();
    test_missing_stack_entry_still_clipped();
    test_below_window_fully_covered_by_normal_occluder_is_excluded();
    test_below_window_without_occluders_is_included();
    test_dead_monitor_seam_pixels_do_not_count();
    test_overlay_center_comes_from_clipped_fragment();
    test_physical_dual_monitor_candidate_stays_visible();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
