#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/app/app_data.h"
#include "geom/geom_rule_sync.h"
#include "geom/window_geometry_matching.h"
#include "matching/match_entry.h"
#include "matching/match_entry_config.h"
#include "rules/rules_config.h"
#include "x11/frame_extents.h"
#include "x11/x11_utils.h"

static int tests_passed = 0;
static int tests_failed = 0;
static int g_geom_x = 11;
static int g_geom_y = 22;
static int g_geom_w = 333;
static int g_geom_h = 444;
static int g_geom_desktop = 5;
static int g_move_calls = 0;
static Window g_last_moved_window = 0;
static int g_last_move_x = 0;
static int g_last_move_y = 0;
static int g_last_move_w = 0;
static int g_last_move_h = 0;

WindowInfo *get_selected_window(AppData *app) { (void)app; return NULL; }
gboolean get_window_geometry(Display *display, Window window, int *x, int *y, int *width, int *height) {
    (void)display;
    (void)window;
    if (x) *x = g_geom_x;
    if (y) *y = g_geom_y;
    if (width) *width = g_geom_w;
    if (height) *height = g_geom_h;
    return TRUE;
}
int get_window_desktop(Display *display, Window window) { (void)display; (void)window; return g_geom_desktop; }
gboolean window_is_fullscreen(Display *display, Window window) { (void)display; (void)window; return FALSE; }
gboolean window_is_maximized_horizontal(Display *display, Window window) { (void)display; (void)window; return FALSE; }
gboolean window_is_maximized_vertical(Display *display, Window window) { (void)display; (void)window; return FALSE; }
void set_window_fullscreen(Display *display, Window window, WindowStateAction action) {
    (void)display; (void)window; (void)action;
}
void set_window_maximized_horizontal(Display *display, Window window, WindowStateAction action) {
    (void)display; (void)window; (void)action;
}
void set_window_maximized_vertical(Display *display, Window window, WindowStateAction action) {
    (void)display; (void)window; (void)action;
}
void unmaximize_and_settle(Display *display, Window window_id) {
    (void)display;
    (void)window_id;
}
int get_current_desktop(Display *display) { (void)display; return 0; }
void switch_to_desktop(Display *display, int desktop) { (void)display; (void)desktop; }
void move_window_to_desktop(Display *display, Window window, int desktop_index) {
    (void)display; (void)window; (void)desktop_index;
}
int get_frame_extents(Display *display, Window window, FrameExtents *extents) {
    (void)display; (void)window;
    if (extents) memset(extents, 0, sizeof(*extents));
    return 0;
}
void request_frame_extents(Display *display, Window window) { (void)display; (void)window; }
void xmove_resize_frame_aware(Display *display, Window window,
                              int frame_x, int frame_y, int width, int height) {
    (void)display;
    g_move_calls++;
    g_last_moved_window = window;
    g_last_move_x = frame_x;
    g_last_move_y = frame_y;
    g_last_move_w = width;
    g_last_move_h = height;
}
int XFlush(Display *display) { (void)display; return 0; }
int matching_run_gc(AppData *app) { (void)app; return 0; }

#define ASSERT_TRUE(desc, cond) do { \
    if (!(cond)) { \
        printf("FAIL: %s\n", (desc)); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } \
} while (0)

#define ASSERT_INT(desc, expected, actual) do { \
    int _expected = (expected); \
    int _actual = (actual); \
    if (_expected != _actual) { \
        printf("FAIL: %s - expected %d, got %d\n", (desc), _expected, _actual); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } \
} while (0)

static void set_test_home(void) {
    char tmpdir[] = "/tmp/cofi_geom_rule_sync_XXXXXX";
    char *dir = mkdtemp(tmpdir);
    if (!dir) {
        return;
    }
    setenv("HOME", dir, 1);
    char path[512];
    snprintf(path, sizeof(path), "%s/.config", dir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi", dir);
    mkdir(path, 0755);
}

static void init_test_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    match_entry_manager_init(&app->matching);
    layout_store_init(&app->layouts);
    init_rules_config(&app->rules_config);
}

static void add_layout_with_pattern(AppData *app, int match_id, const char *pattern, bool disabled) {
    int idx = app->matching.count++;
    app->matching.entries[idx].match_id = match_id;
    snprintf(app->matching.entries[idx].class_name,
             sizeof(app->matching.entries[idx].class_name), "Class%d", match_id);
    snprintf(app->matching.entries[idx].instance,
             sizeof(app->matching.entries[idx].instance), "inst%d", match_id);
    g_strlcpy(app->matching.entries[idx].type, "Normal",
              sizeof(app->matching.entries[idx].type));
    g_strlcpy(app->matching.entries[idx].original_title, pattern,
              sizeof(app->matching.entries[idx].original_title));
    app->matching.entries[idx].assigned = 1;
    if (app->matching.next_match_id <= match_id) {
        app->matching.next_match_id = match_id + 1;
    }
    app->layouts.records[app->layouts.count++] = (LayoutRecord){
        .match_id = match_id,
        .x = 10,
        .y = 20,
        .width = 300,
        .height = 200,
        .desktop = 1,
        .restore_desktop = true,
        .disabled = disabled
    };
}

static int count_geom_rules(const RulesConfig *config, const char *pattern) {
    int count = 0;
    for (int i = 0; i < config->count; i++) {
        if (strcmp(config->rules[i].tag, "geom") == 0 &&
            strcmp(config->rules[i].pattern, pattern) == 0 &&
            rule_commands_contain_segment(config->rules[i].commands, "rl")) {
            count++;
        }
    }
    return count;
}

static int count_geom_rules_by_match_id(const RulesConfig *config, int match_id) {
    int count = 0;
    for (int i = 0; i < config->count; i++) {
        if (strcmp(config->rules[i].tag, "geom") == 0 &&
            config->rules[i].match_id == match_id &&
            rule_commands_contain_segment(config->rules[i].commands, "rl")) {
            count++;
        }
    }
    return count;
}

static int find_geom_rule_index_by_pattern(const RulesConfig *config, const char *pattern) {
    for (int i = 0; i < config->count; i++) {
        if (strcmp(config->rules[i].tag, "geom") == 0 &&
            strcmp(config->rules[i].pattern, pattern) == 0 &&
            rule_commands_contain_segment(config->rules[i].commands, "rl")) {
            return i;
        }
    }
    return -1;
}

static WindowInfo make_window(Window id, const char *title, const char *class_name,
                              const char *instance, const char *type) {
    WindowInfo window = {0};
    window.id = id;
    g_strlcpy(window.title, title, sizeof(window.title));
    g_strlcpy(window.class_name, class_name, sizeof(window.class_name));
    g_strlcpy(window.instance, instance, sizeof(window.instance));
    g_strlcpy(window.type, type, sizeof(window.type));
    return window;
}

static void test_refcount_create_and_delete(void) {
    AppData app;
    init_test_app(&app);
    add_layout_with_pattern(&app, 1, "My App", false);
    add_layout_with_pattern(&app, 2, "My App", false);

    geom_rule_sync_for_pattern(&app, "My App");
    ASSERT_TRUE("two enabled layouts create one tagged rule each",
                count_geom_rules(&app.rules_config, "My App") == 2);

    app.layouts.records[0].disabled = true;
    geom_rule_sync_for_pattern(&app, "My App");
    ASSERT_TRUE("disabling one removes only that layout rule",
                count_geom_rules(&app.rules_config, "My App") == 1);

    app.layouts.records[1].disabled = true;
    geom_rule_sync_for_pattern(&app, "My App");
    ASSERT_TRUE("disabling both deletes tagged rule",
                count_geom_rules(&app.rules_config, "My App") == 0);
}

static void test_delete_layout_refcount_behavior(void) {
    AppData app;
    init_test_app(&app);
    add_layout_with_pattern(&app, 1, "My App", false);
    add_layout_with_pattern(&app, 2, "My App", false);
    geom_rule_sync_for_pattern(&app, "My App");

    app.layouts.records[0] = app.layouts.records[1];
    app.layouts.count = 1;
    geom_rule_sync_for_pattern(&app, "My App");
    ASSERT_TRUE("deleting one of two keeps remaining layout rule",
                count_geom_rules(&app.rules_config, "My App") == 1);

    app.layouts.count = 0;
    geom_rule_sync_for_pattern(&app, "My App");
    ASSERT_TRUE("deleting sole layout removes tagged rule",
                count_geom_rules(&app.rules_config, "My App") == 0);
}

static void test_disable_enable_roundtrip(void) {
    AppData app;
    init_test_app(&app);
    add_layout_with_pattern(&app, 1, "Editor", false);

    geom_rule_sync_for_pattern(&app, "Editor");
    ASSERT_TRUE("enabled has tagged rule",
                count_geom_rules(&app.rules_config, "Editor") == 1);

    app.layouts.records[0].disabled = true;
    geom_rule_sync_for_pattern(&app, "Editor");
    ASSERT_TRUE("disabled deletes tagged rule",
                count_geom_rules(&app.rules_config, "Editor") == 0);

    app.layouts.records[0].disabled = false;
    geom_rule_sync_for_pattern(&app, "Editor");
    ASSERT_TRUE("re-enabled recreates tagged rule",
                count_geom_rules(&app.rules_config, "Editor") == 1);
}

static void test_untagged_rule_untouched(void) {
    AppData app;
    init_test_app(&app);
    add_layout_with_pattern(&app, 1, "Browser", false);
    add_rule(&app.rules_config, "Browser", "rl");

    geom_rule_sync_for_pattern(&app, "Browser");
    ASSERT_TRUE("untagged rl rule remains", app.rules_config.count == 2);
    ASSERT_TRUE("sync still manages tagged rule independently",
                count_geom_rules(&app.rules_config, "Browser") == 1);

    app.layouts.records[0].disabled = true;
    geom_rule_sync_for_pattern(&app, "Browser");
    ASSERT_TRUE("disabled layout removes only tagged geom rule",
                count_geom_rules(&app.rules_config, "Browser") == 0);
    ASSERT_TRUE("untagged user rl rule survives disabled-layout sync",
                app.rules_config.count == 1 &&
                app.rules_config.rules[0].tag[0] == '\0' &&
                rule_commands_contain_segment(app.rules_config.rules[0].commands, "rl"));
}

static void test_remove_geom_rule_by_match_id_is_selective(void) {
    AppData app;
    init_test_app(&app);
    add_rule(&app.rules_config, "Target", "rl");
    app.rules_config.rules[0].match_id = 401;
    g_strlcpy(app.rules_config.rules[0].tag, "geom",
              sizeof(app.rules_config.rules[0].tag));
    add_rule(&app.rules_config, "User Target", "rl");
    app.rules_config.rules[1].match_id = 401;
    add_rule(&app.rules_config, "Other Geom", "rl");
    app.rules_config.rules[2].match_id = 402;
    g_strlcpy(app.rules_config.rules[2].tag, "geom",
              sizeof(app.rules_config.rules[2].tag));
    add_rule(&app.rules_config, "Non Restore", "noop");
    app.rules_config.rules[3].match_id = 401;
    g_strlcpy(app.rules_config.rules[3].tag, "geom",
              sizeof(app.rules_config.rules[3].tag));

    ASSERT_INT("remove geom rule returns removed", 1,
               geom_rule_remove_for_match_id(&app, 401));
    ASSERT_INT("target tagged geom rule removed", 0,
               count_geom_rules_by_match_id(&app.rules_config, 401));
    ASSERT_TRUE("untagged user rl rule remains",
                app.rules_config.count == 3 &&
                app.rules_config.rules[0].match_id == 401 &&
                app.rules_config.rules[0].tag[0] == '\0' &&
                rule_commands_contain_segment(app.rules_config.rules[0].commands, "rl"));
    ASSERT_INT("other tagged geom rule remains", 1,
               count_geom_rules_by_match_id(&app.rules_config, 402));
    ASSERT_TRUE("non-restore geom-tagged rule remains",
                app.rules_config.rules[2].match_id == 401 &&
                strcmp(app.rules_config.rules[2].tag, "geom") == 0 &&
                !rule_commands_contain_segment(app.rules_config.rules[2].commands, "rl"));

    RulesConfig saved_rules;
    load_rules_config(&saved_rules, NULL);
    ASSERT_INT("removed target rule stays removed on disk", 0,
               count_geom_rules_by_match_id(&saved_rules, 401));
    ASSERT_INT("other geom rule persisted", 1,
               count_geom_rules_by_match_id(&saved_rules, 402));

    int count_after_remove = app.rules_config.count;
    ASSERT_INT("remove absent geom rule no-ops", 0,
               geom_rule_remove_for_match_id(&app, 999));
    ASSERT_INT("absent remove keeps rule count", count_after_remove,
               app.rules_config.count);
}

static void test_startup_heal_create_and_delete(void) {
    AppData app;
    init_test_app(&app);
    add_layout_with_pattern(&app, 1, "alpha", false);
    add_layout_with_pattern(&app, 2, "beta", true);
    add_rule(&app.rules_config, "orphan", "rl");
    g_strlcpy(app.rules_config.rules[0].tag, "geom", sizeof(app.rules_config.rules[0].tag));

    geom_rule_sync_all_layout_patterns(&app);

    ASSERT_TRUE("startup creates missing tagged rule for enabled layout",
                count_geom_rules(&app.rules_config, "alpha") == 1);
    ASSERT_TRUE("startup keeps no tagged rule for fully-disabled pattern",
                count_geom_rules(&app.rules_config, "beta") == 0);
    ASSERT_TRUE("startup removes orphan tagged rule",
                count_geom_rules(&app.rules_config, "orphan") == 0);
}

static void test_startup_sweeps_stale_rules_before_creating(void) {
    AppData app;
    init_test_app(&app);
    add_layout_with_pattern(&app, 42, "Fresh", false);

    for (int i = 0; i < MAX_RULES - 1; i++) {
        char pattern[32];
        snprintf(pattern, sizeof(pattern), "user-%d", i);
        add_rule(&app.rules_config, pattern, "noop");
    }
    add_rule(&app.rules_config, "stale", "rl");
    app.rules_config.rules[MAX_RULES - 1].match_id = 999;
    g_strlcpy(app.rules_config.rules[MAX_RULES - 1].tag, "geom",
              sizeof(app.rules_config.rules[MAX_RULES - 1].tag));

    geom_rule_sync_all_layout_patterns(&app);

    ASSERT_TRUE("startup keeps rules at capacity after sweep/create",
                app.rules_config.count == MAX_RULES);
    ASSERT_TRUE("startup frees stale slot before creating anchored geom rule",
                count_geom_rules(&app.rules_config, "Fresh") == 1);
    ASSERT_TRUE("startup removes stale geom rule in same pass",
                count_geom_rules(&app.rules_config, "stale") == 0);
}

static void test_wildcard_pattern_verbatim(void) {
    AppData app;
    init_test_app(&app);
    add_layout_with_pattern(&app, 1, "*foo*", false);
    geom_rule_sync_for_pattern(&app, "*foo*");

    ASSERT_TRUE("wildcard pattern stored verbatim",
                strcmp(app.rules_config.rules[0].pattern, "*foo*") == 0);
}

static void test_save_geometry_reuses_layout_match_id_for_geom_rule(void) {
    AppData app;
    init_test_app(&app);
    app.display = (Display *)0x1;
    app.windows[0] = make_window(0x701, "Geom Single", "ClassGeom", "instGeom", "Normal");
    app.window_count = 1;

    int initial_count = app.matching.count;
    ASSERT_INT("geometry save succeeds", TRUE,
               save_window_geometry_for_window(&app, &app.windows[0]));
    ASSERT_INT("single geometry save creates exactly one match entry",
               initial_count + 1, app.matching.count);
    ASSERT_INT("single geometry save creates one layout", 1, app.layouts.count);
    ASSERT_INT("single geometry save creates one geom rl rule",
               1, count_geom_rules(&app.rules_config, "Geom Single"));

    int rule_idx = find_geom_rule_index_by_pattern(&app.rules_config, "Geom Single");
    ASSERT_TRUE("geom rl rule found", rule_idx >= 0);
    if (rule_idx < 0 || app.layouts.count == 0 || app.matching.count == 0) {
        return;
    }

    int layout_match_id = app.layouts.records[0].match_id;
    int rule_match_id = app.rules_config.rules[rule_idx].match_id;
    ASSERT_INT("geom rl rule references layout match_id", layout_match_id, rule_match_id);
    ASSERT_INT("layout match_id is the only entry match_id",
               app.matching.entries[initial_count].match_id, layout_match_id);
    ASSERT_TRUE("single entry retains class anchor",
                app.matching.entries[initial_count].class_name[0] != '\0');
    ASSERT_TRUE("single entry retains instance anchor",
                app.matching.entries[initial_count].instance[0] != '\0');
    ASSERT_TRUE("single entry retains type anchor",
                app.matching.entries[initial_count].type[0] != '\0');
}

static void test_save_geometry_ignores_stale_bound_identity(void) {
    AppData app;
    init_test_app(&app);
    app.display = (Display *)0x1;
    app.windows[0] = make_window(0x801, "Terminal", "Alacritty", "alacritty", "Normal");
    app.window_count = 1;

    int terminal_match_id = matching_create_entry(&app.matching, &app.windows[0]);
    ASSERT_INT("terminal identity created", 1, terminal_match_id > 0);
    ASSERT_INT("terminal layout stored", TRUE,
               layout_store_set(&app.layouts, terminal_match_id, 1, 2, 300, 200, 0,
                                false, false, false, true, false));

    g_strlcpy(app.windows[0].title, "invoicing",
              sizeof(app.windows[0].title));
    g_geom_x = 51;
    g_geom_y = 62;
    g_geom_w = 700;
    g_geom_h = 500;

    ASSERT_INT("save with changed title succeeds", TRUE,
               save_window_geometry_for_window(&app, &app.windows[0]));
    ASSERT_INT("save creates a current-title identity instead of reusing stale binding",
               2, app.matching.count);
    ASSERT_INT("save keeps old layout and adds current-title layout", 2, app.layouts.count);

    const LayoutRecord *terminal_layout = layout_store_get(&app.layouts, terminal_match_id);
    ASSERT_TRUE("terminal layout still exists", terminal_layout != NULL);
    if (terminal_layout) {
        ASSERT_INT("terminal layout x unchanged", 1, terminal_layout->x);
        ASSERT_INT("terminal layout y unchanged", 2, terminal_layout->y);
        ASSERT_INT("terminal layout width unchanged", 300, terminal_layout->width);
        ASSERT_INT("terminal layout height unchanged", 200, terminal_layout->height);
    }

    int current_idx = -1;
    for (int i = 0; i < app.matching.count; i++) {
        if (strcmp(app.matching.entries[i].original_title, "invoicing") == 0) {
            current_idx = i;
            break;
        }
    }
    ASSERT_TRUE("current-title identity created", current_idx >= 0);
    if (current_idx < 0) {
        return;
    }

    int current_match_id = app.matching.entries[current_idx].match_id;
    const LayoutRecord *current_layout = layout_store_get(&app.layouts, current_match_id);
    ASSERT_TRUE("current-title layout exists", current_layout != NULL);
    if (current_layout) {
        ASSERT_INT("current-title layout x captured", g_geom_x, current_layout->x);
        ASSERT_INT("current-title layout y captured", g_geom_y, current_layout->y);
        ASSERT_INT("current-title layout width captured", g_geom_w, current_layout->width);
        ASSERT_INT("current-title layout height captured", g_geom_h, current_layout->height);
    }
}

static void test_restore_prefers_current_title_layout_over_stale_binding(void) {
    AppData app;
    init_test_app(&app);
    app.display = (Display *)0x1;
    app.windows[0] = make_window(0x901, "Stream - Video", "Google-chrome",
                                 "google-chrome", "Normal");
    app.window_count = 1;

    app.matching.count = 2;
    app.matching.next_match_id = 203;
    app.matching.entries[0] = (MatchEntry){
        .match_id = 201,
        .bound_x11_id = 0x901,
        .assigned = 1
    };
    g_strlcpy(app.matching.entries[0].original_title, "New Tab - Google Chrome",
              sizeof(app.matching.entries[0].original_title));
    g_strlcpy(app.matching.entries[0].class_name, "Google-chrome",
              sizeof(app.matching.entries[0].class_name));
    g_strlcpy(app.matching.entries[0].instance, "google-chrome",
              sizeof(app.matching.entries[0].instance));
    g_strlcpy(app.matching.entries[0].type, "Normal",
              sizeof(app.matching.entries[0].type));

    app.matching.entries[1] = (MatchEntry){
        .match_id = 202,
        .bound_x11_id = 0,
        .assigned = 0
    };
    g_strlcpy(app.matching.entries[1].original_title, "Stream*",
              sizeof(app.matching.entries[1].original_title));
    g_strlcpy(app.matching.entries[1].class_name, "Google-chrome",
              sizeof(app.matching.entries[1].class_name));
    g_strlcpy(app.matching.entries[1].instance, "google-chrome",
              sizeof(app.matching.entries[1].instance));
    g_strlcpy(app.matching.entries[1].type, "Normal",
              sizeof(app.matching.entries[1].type));

    ASSERT_INT("stream layout stored", TRUE,
               layout_store_set(&app.layouts, 202, 77, 88, 999, 777, 5,
                                false, false, false, true, false));

    g_move_calls = 0;
    ASSERT_INT("restore succeeds", TRUE,
               restore_window_geometry_for_window(&app, &app.windows[0]));
    ASSERT_INT("restore applies matching current-title layout once", 1, g_move_calls);
    ASSERT_INT("restore targets current window", (int)app.windows[0].id,
               (int)g_last_moved_window);
    ASSERT_INT("restore uses stream layout x", 77, g_last_move_x);
    ASSERT_INT("restore uses stream layout y", 88, g_last_move_y);
    ASSERT_INT("restore uses stream layout width", 999, g_last_move_w);
    ASSERT_INT("restore uses stream layout height", 777, g_last_move_h);
    ASSERT_INT("stream entry rebound to current window", (int)app.windows[0].id,
               (int)app.matching.entries[1].bound_x11_id);
    ASSERT_INT("stream entry assigned", 1, app.matching.entries[1].assigned);
}

static void test_clear_geometry_removes_tagged_geom_rule_and_entry(void) {
    AppData app;
    init_test_app(&app);
    add_layout_with_pattern(&app, 301, "Clear Me", false);
    app.matching.entries[0].bound_x11_id = 0xA01;
    app.window_count = 1;
    app.windows[0] = make_window(0xA01, "Clear Me", "Class301", "inst301", "Normal");

    ASSERT_INT("initial geom rule created", 1,
               geom_rule_sync_for_layout(&app, 301));
    ASSERT_INT("tagged geom rule initially exists", 1,
               count_geom_rules_by_match_id(&app.rules_config, 301));

    ASSERT_INT("clear geometry succeeds", TRUE,
               clear_window_geometry_for_window(&app, &app.windows[0]));
    ASSERT_TRUE("clear geometry removes layout",
                layout_store_get(&app.layouts, 301) == NULL);
    ASSERT_INT("clear geometry removes tagged geom rule", 0,
               count_geom_rules_by_match_id(&app.rules_config, 301));
    ASSERT_INT("clear geometry deletes owned match entry", -1,
               match_entry_find_index_by_match_id(&app.matching, 301));

    LayoutStore saved_layouts;
    layout_store_init(&saved_layouts);
    layout_store_load(&saved_layouts);
    ASSERT_TRUE("cleared layout is persisted",
                layout_store_get(&saved_layouts, 301) == NULL);

    MatchEntryManager saved_matching;
    load_match_entries(&saved_matching);
    ASSERT_INT("deleted match entry is persisted", -1,
               match_entry_find_index_by_match_id(&saved_matching, 301));

    RulesConfig saved_rules;
    load_rules_config(&saved_rules, NULL);
    ASSERT_INT("removed tagged geom rule is persisted", 0,
               count_geom_rules_by_match_id(&saved_rules, 301));
}

static void test_sync_update_preserves_user_set_flags(void) {
    AppData app;
    init_test_app(&app);
    add_layout_with_pattern(&app, 401, "Editor", false);
    ASSERT_INT("initial sync creates rule", 1,
               geom_rule_sync_for_layout(&app, 401));

    int idx = find_geom_rule_index_by_pattern(&app.rules_config, "Editor");
    ASSERT_TRUE("rule found", idx >= 0);
    app.rules_config.rules[idx].once = false;
    app.rules_config.rules[idx].new_only = true;

    g_strlcpy(app.matching.entries[0].original_title, "Editor v2",
              sizeof(app.matching.entries[0].original_title));
    ASSERT_INT("rename triggers update branch", 1,
               geom_rule_sync_for_layout(&app, 401));

    int updated_idx = find_geom_rule_index_by_pattern(&app.rules_config, "Editor v2");
    ASSERT_TRUE("renamed rule found", updated_idx >= 0);
    ASSERT_TRUE("user-set once preserved across update",
                app.rules_config.rules[updated_idx].once == false);
    ASSERT_TRUE("user-set new_only preserved across update",
                app.rules_config.rules[updated_idx].new_only == true);
}

static void test_find_owning_rule(void) {
    AppData app;
    init_test_app(&app);
    add_layout_with_pattern(&app, 501, "Browser", false);
    geom_rule_sync_for_layout(&app, 501);

    Rule *rule = geom_find_owning_rule(&app, 501);
    ASSERT_TRUE("find_owning_rule returns rule for known match_id", rule != NULL);
    ASSERT_TRUE("find_owning_rule returns NULL for unknown match_id",
                geom_find_owning_rule(&app, 999) == NULL);
}

int main(void) {
    printf("Geom rule sync tests\n");
    printf("====================\n\n");

    set_test_home();
    test_refcount_create_and_delete();
    test_delete_layout_refcount_behavior();
    test_disable_enable_roundtrip();
    test_untagged_rule_untouched();
    test_remove_geom_rule_by_match_id_is_selective();
    test_startup_heal_create_and_delete();
    test_startup_sweeps_stale_rules_before_creating();
    test_wildcard_pattern_verbatim();
    test_save_geometry_reuses_layout_match_id_for_geom_rule();
    test_save_geometry_ignores_stale_bound_identity();
    test_restore_prefers_current_title_layout_over_stale_binding();
    test_clear_geometry_removes_tagged_geom_rule_and_entry();
    test_sync_update_preserves_user_set_flags();
    test_find_owning_rule();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
