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

typedef struct {
    Window id;
    int x;
    int y;
    int w;
    int h;
} TestGeometry;

static TestGeometry test_geometries[MAX_WINDOWS];
static int test_geometry_count = 0;
static int test_current_desktop = 0;

static void reset_test_geometry(void) {
    memset(test_geometries, 0, sizeof(test_geometries));
    test_geometry_count = 0;
}

static void add_geometry(Window id, int x, int y, int w, int h) {
    test_geometries[test_geometry_count].id = id;
    test_geometries[test_geometry_count].x = x;
    test_geometries[test_geometry_count].y = y;
    test_geometries[test_geometry_count].w = w;
    test_geometries[test_geometry_count].h = h;
    test_geometry_count++;
}

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

void save_config(const CofiConfig *config) {
    (void)config;
}

void show_slot_overlays(AppData *app) {
    (void)app;
}

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

Atom XInternAtom(Display *display, const char *atom_name, Bool only_if_exists) {
    (void)display;
    (void)atom_name;
    (void)only_if_exists;
    return None;
}

int XGetWindowProperty(Display *display, Window window, Atom property,
                       long long_offset, long long_length, Bool del, Atom req_type,
                       Atom *actual_type_return, int *actual_format_return,
                       unsigned long *nitems_return, unsigned long *bytes_after_return,
                       unsigned char **prop_return) {
    (void)display;
    (void)window;
    (void)property;
    (void)long_offset;
    (void)long_length;
    (void)del;
    (void)req_type;
    if (actual_type_return) *actual_type_return = None;
    if (actual_format_return) *actual_format_return = 0;
    if (nitems_return) *nitems_return = 0;
    if (bytes_after_return) *bytes_after_return = 0;
    if (prop_return) *prop_return = NULL;
    return BadAtom;
}

int XFree(void *data) {
    (void)data;
    return 0;
}

#undef DefaultRootWindow
#define DefaultRootWindow(dpy) ((Window)0)

#include "../src/workspace_slots.c"

static void test_assign_workspace_slots_keeps_behavior_for_small_sets(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.window_count = 5;

    reset_test_geometry();

    for (int i = 0; i < 5; i++) {
        Window id = (Window)(0x100 + i);
        app.windows[i].id = id;
        app.windows[i].desktop = 0;
        strcpy(app.windows[i].type, "Normal");
        add_geometry(id, i * 100, 0, 90, 90);
    }

    assign_workspace_slots(&app);

    ASSERT_TRUE("small set assigns 5 slots", app.workspace_slots.count == 5);
    ASSERT_TRUE("slot 1 is first window", get_workspace_slot_window(&app.workspace_slots, 1) == 0x100);
    ASSERT_TRUE("slot 5 is fifth window", get_workspace_slot_window(&app.workspace_slots, 5) == 0x104);
}

static void test_assign_workspace_slots_considers_all_candidates_before_capping(void) {
    AppData app = {0};
    init_workspace_slots(&app.workspace_slots);
    app.config.slot_sort_order = SLOT_SORT_ROW_FIRST;
    app.config.digit_slot_mode = DIGIT_MODE_DEFAULT;
    app.window_count = 11;

    reset_test_geometry();

    app.windows[0].id = 0x201;
    app.windows[0].desktop = -1;
    strcpy(app.windows[0].type, "Normal");
    add_geometry(0x201, 9000, 0, 100, 100);

    for (int i = 1; i <= 9; i++) {
        Window id = (Window)(0x201 + i);
        app.windows[i].id = id;
        app.windows[i].desktop = 0;
        strcpy(app.windows[i].type, "Normal");
        add_geometry(id, (i - 1) * 100, 0, 100, 100);
    }

    app.windows[10].id = 0x20c;
    app.windows[10].desktop = 0;
    strcpy(app.windows[10].type, "Normal");
    add_geometry(0x20c, 50, 0, 100, 100);

    assign_workspace_slots(&app);

    ASSERT_TRUE("slot count capped at MAX_WORKSPACE_SLOTS", app.workspace_slots.count == MAX_WORKSPACE_SLOTS);
    ASSERT_TRUE("late candidate is included after full collection", get_workspace_slot_window(&app.workspace_slots, 2) == 0x20c);
    ASSERT_TRUE("far-right sticky window is excluded from top 9", get_workspace_slot_window(&app.workspace_slots, 9) != 0x201);
}

int main(void) {
    printf("Workspace slot candidate cap tests\n");
    printf("==================================\n\n");

    test_assign_workspace_slots_keeps_behavior_for_small_sets();
    test_assign_workspace_slots_considers_all_candidates_before_capping();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
