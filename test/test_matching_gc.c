#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "core/app/app_data.h"
#include "x11/frame_extents.h"
#include "harpoon/harpoon.h"
#include "geom/layout_store.h"
#include "matching/match_entry.h"
#include "matching/match_entry_config.h"
#include "core/app/matching_gc.h"
#include "geom/window_geometry_matching.h"
#include "names/names_store.h"
#include "x11/x11_utils.h"
#include "core/utils/utils.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(name, cond) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
        printf("PASS: %s\n", name); \
    } else { \
        printf("FAIL: %s (line %d)\n", name, __LINE__); \
    } \
} while (0)

gboolean get_window_geometry(Display *display, Window window, int *x, int *y, int *width, int *height) {
    (void)display;
    (void)window;
    if (x) *x = 0;
    if (y) *y = 0;
    if (width) *width = 100;
    if (height) *height = 100;
    return TRUE;
}

int get_window_desktop(Display *display, Window window) {
    (void)display;
    (void)window;
    return 0;
}

void move_window_to_desktop(Display *display, Window window, int desktop_index) {
    (void)display;
    (void)window;
    (void)desktop_index;
}

gboolean get_window_state(Display *display, Window window, const char *state_atom_name) {
    (void)display;
    (void)window;
    (void)state_atom_name;
    return FALSE;
}

gboolean window_is_fullscreen(Display *display, Window window) {
    (void)display;
    (void)window;
    return FALSE;
}

gboolean window_is_maximized_horizontal(Display *display, Window window) {
    (void)display;
    (void)window;
    return FALSE;
}

gboolean window_is_maximized_vertical(Display *display, Window window) {
    (void)display;
    (void)window;
    return FALSE;
}

void set_window_fullscreen(Display *display, Window window, WindowStateAction action) {
    (void)display;
    (void)window;
    (void)action;
}

void set_window_maximized_horizontal(Display *display, Window window, WindowStateAction action) {
    (void)display;
    (void)window;
    (void)action;
}

void set_window_maximized_vertical(Display *display, Window window, WindowStateAction action) {
    (void)display;
    (void)window;
    (void)action;
}

int get_current_desktop(Display *display) { (void)display; return 0; }
void switch_to_desktop(Display *display, int desktop) { (void)display; (void)desktop; }
int get_frame_extents(Display *display, Window window, FrameExtents *extents) {
    (void)display; (void)window;
    if (extents) memset(extents, 0, sizeof(*extents));
    return 0;
}
void request_frame_extents(Display *display, Window window) { (void)display; (void)window; }
void xmove_resize_frame_aware(Display *display, Window window,
                               int frame_x, int frame_y, int width, int height) {
    (void)display; (void)window; (void)frame_x; (void)frame_y; (void)width; (void)height;
}

static void set_test_home(const char *suffix) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/cofi-matching-gc-%s-%ld", suffix, (long)getpid());
    mkdir(path, 0755);
    setenv("HOME", path, 1);
}

static WindowInfo make_window(Window id,
                              const char *title,
                              const char *class_name,
                              const char *instance,
                              const char *type) {
    WindowInfo w = {0};
    w.id = id;
    safe_string_copy(w.title, title, MAX_TITLE_LEN);
    safe_string_copy(w.class_name, class_name, MAX_CLASS_LEN);
    safe_string_copy(w.instance, instance, MAX_CLASS_LEN);
    safe_string_copy(w.type, type, sizeof(w.type));
    return w;
}

static void init_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    match_entry_manager_init(&app->matching);
    init_harpoon_manager(&app->harpoon);
    layout_store_init(&app->layouts);
    names_store_init(&app->names);
    app->harpoon.matching = &app->matching;
    app->harpoon.windows = app->windows;
    app->harpoon.window_count = &app->window_count;
}

static int find_match_id_by_window(const AppData *app, Window id) {
    int idx = match_entry_find_index_by_window(&app->matching, id);
    return idx >= 0 ? app->matching.entries[idx].match_id : -1;
}

static void test_clear_layout_removes_record_and_gcs_orphaned_entry(void) {
    set_test_home("clear-layout");

    AppData app;
    init_app(&app);

    app.window_count = 1;
    app.windows[0] = make_window(0x501, "Clear Layout", "Kitty", "kitty", "Normal");
    int match_id = matching_create_entry(&app.matching, &app.windows[0]);
    ASSERT_TRUE("captured match entry for clear test", match_id > 0);
    ASSERT_TRUE("seeded layout record", layout_store_set(&app.layouts, match_id, 1, 2, 300, 200, 4,
                                                         false, false, false, true, false));
    save_match_entries(&app.matching);
    ASSERT_TRUE("initial matching persisted", 1);
    ASSERT_TRUE("persisted initial layouts", layout_store_save(&app.layouts));

    ASSERT_TRUE("clear-layout helper succeeds",
                clear_window_geometry_for_window(&app, &app.windows[0]) == TRUE);
    ASSERT_TRUE("layout record removed in memory", app.layouts.count == 0);
    ASSERT_TRUE("orphaned matching entry removed by gc", app.matching.count == 0);

    MatchEntryManager loaded_matching;
    LayoutStore loaded_layouts;
    load_match_entries(&loaded_matching);
    layout_store_init(&loaded_layouts);
    layout_store_load(&loaded_layouts);
    ASSERT_TRUE("matching.json reloaded empty after clear", loaded_matching.count == 0);
    ASSERT_TRUE("layouts.json reloaded empty after clear", loaded_layouts.count == 0);
}

static void test_matching_gc_composes_label_harpoon_and_layout_consumers(void) {
    set_test_home("compose");

    AppData app;
    init_app(&app);

    app.window_count = 5;
    app.windows[0] = make_window(0x601, "Label", "Kitty", "kitty-a", "Normal");
    app.windows[1] = make_window(0x602, "Harpoon", "Kitty", "kitty-b", "Normal");
    app.windows[2] = make_window(0x603, "Layout", "Kitty", "kitty-c", "Normal");
    app.windows[3] = make_window(0x604, "Bare", "Kitty", "kitty-d", "Normal");
    app.windows[4] = make_window(0x605, "Rules", "Kitty", "kitty-e", "Normal");

    int named_match_id = matching_create_entry(&app.matching, &app.windows[0]);
    ASSERT_TRUE("named entry captured", named_match_id > 0);
    ASSERT_TRUE("name root stored",
                names_store_set(&app.names, named_match_id, "named") == true);
    assign_window_to_slot(&app.harpoon, 1, &app.windows[1]);
    int layout_match_id = matching_create_entry(&app.matching, &app.windows[2]);
    int bare_match_id = matching_create_entry(&app.matching, &app.windows[3]);
    int rules_match_id = matching_create_entry(&app.matching, &app.windows[4]);

    ASSERT_TRUE("layout-only entry captured", layout_match_id > 0);
    ASSERT_TRUE("bare entry captured", bare_match_id > 0);
    ASSERT_TRUE("rules-only entry captured", rules_match_id > 0);
    ASSERT_TRUE("layout-only record stored", layout_store_set(&app.layouts, layout_match_id, 7, 8, 640, 480, 2,
                                                              false, false, false, true, false));
    app.rules_config.count = 1;
    app.rules_config.rules[0].match_id = rules_match_id;
    safe_string_copy(app.rules_config.rules[0].pattern, "Rules", MAX_PATTERN_LEN);
    safe_string_copy(app.rules_config.rules[0].commands, "rl", MAX_COMMANDS_LEN);

    ASSERT_TRUE("five matching entries created", app.matching.count == 5);

    ASSERT_TRUE("gc removes only bare entry", matching_run_gc(&app) == 1);
    ASSERT_TRUE("four referenced entries remain", app.matching.count == 4);
    ASSERT_TRUE("named entry survives", match_entry_find_index_by_match_id(&app.matching, named_match_id) >= 0);
    ASSERT_TRUE("name record survives",
                strcmp(names_store_get_by_match_id(&app.names, named_match_id), "named") == 0);
    ASSERT_TRUE("harpoon entry survives", find_match_id_by_window(&app, 0x602) > 0);
    ASSERT_TRUE("layout-only entry survives", match_entry_find_index_by_match_id(&app.matching, layout_match_id) >= 0);
    ASSERT_TRUE("rules-only entry survives", match_entry_find_index_by_match_id(&app.matching, rules_match_id) >= 0);
    ASSERT_TRUE("bare entry removed", match_entry_find_index_by_match_id(&app.matching, bare_match_id) < 0);
}

int main(void) {
    printf("Matching GC tests\n");
    printf("=================\n\n");

    test_clear_layout_removes_record_and_gcs_orphaned_entry();
    test_matching_gc_composes_label_harpoon_and_layout_consumers();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
