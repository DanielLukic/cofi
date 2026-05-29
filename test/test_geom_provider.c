#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/app/app_data.h"
#include "providers/cofi_tab_provider.h"
#include "commands/command_registry.h"
#include "geom/window_geometry_matching.h"
#include "x11/x11_utils.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(msg, cond) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("PASS: %s\n", msg); } \
    else { printf("FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

static int g_reset_selection_calls;
static int g_update_display_calls;
static int g_matching_run_gc_calls;
static int g_save_match_calls;
static int g_delete_by_match_id_calls;
static int g_last_deleted_match_id;
static int g_show_confirm_calls;
static void (*g_confirm_cb)(AppData *);
static int g_geom_rule_sync_calls;
static int g_last_synced_match_id;
static CofiTabProvider g_registered_provider;
static int g_set_state_calls;
static int g_move_calls;
static int g_move_desktop_calls;
static int g_switch_desktop_calls;
static int g_flush_calls;
static int g_show_pattern_overlay_calls;
static char g_last_pattern_context[128];
static char g_test_home[512];

static void set_test_home(void) {
    char tmpdir[] = "/tmp/cofi_geom_provider_XXXXXX";
    char *dir = mkdtemp(tmpdir);
    if (!dir) {
        return;
    }
    g_strlcpy(g_test_home, dir, sizeof(g_test_home));
    setenv("HOME", dir, 1);
    char path[512];
    snprintf(path, sizeof(path), "%s/.config", dir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi", dir);
    mkdir(path, 0755);
}

static void cleanup_test_home(void) {
    if (g_test_home[0] == '\0') {
        return;
    }
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_test_home);
    system(cmd);
}

void log_log(int level, const char *file, int line, const char *fmt, ...) {(void)level;(void)file;(void)line;(void)fmt;}
int has_match(const char *needle, const char *haystack) { return !needle || !needle[0] || (haystack && strstr(haystack, needle)); }
void reset_selection(AppData *app) { (void)app; g_reset_selection_calls++; }
void update_display(AppData *app) { (void)app; g_update_display_calls++; }
int matching_run_gc(AppData *app) { (void)app; g_matching_run_gc_calls++; return 1; }
void match_entry_delete_by_match_id(MatchEntryManager *manager, int match_id) {
    g_delete_by_match_id_calls++;
    g_last_deleted_match_id = match_id;
    int idx = -1;
    if (manager) {
        for (int i = 0; i < manager->count; i++) {
            if (manager->entries[i].match_id == match_id) {
                idx = i;
                break;
            }
        }
    }
    if (idx < 0) return;
    for (int i = idx; i < manager->count - 1; i++) {
        manager->entries[i] = manager->entries[i + 1];
    }
    memset(&manager->entries[manager->count - 1], 0, sizeof(manager->entries[0]));
    manager->count--;
}
int geom_rule_sync_for_layout(AppData *app, int match_id) {
    (void)app;
    g_geom_rule_sync_calls++;
    g_last_synced_match_id = match_id;
    return 1;
}
void show_confirm_overlay(AppData *app, const char *title, const char *info, void (*on_confirm)(AppData *)) { (void)app;(void)title;(void)info; g_show_confirm_calls++; g_confirm_cb = on_confirm; }
int selected_match_id_for_pattern_edit(AppData *app) {
    if (!app || app->filtered_geom_count <= 0) return 0;
    return app->layouts.records[app->filtered_geom[app->selection.provider_index]].match_id;
}
gboolean show_pattern_edit_overlay(AppData *app, int match_id, const char *context_line) {
    (void)app;
    if (match_id <= 0) return FALSE;
    g_show_pattern_overlay_calls++;
    g_strlcpy(g_last_pattern_context, context_line ? context_line : "",
              sizeof(g_last_pattern_context));
    return TRUE;
}
void exit_command_mode(AppData *app) { (void)app; }
void surface_tab(AppData *app, TabMode tab) { if (app) app->current_tab = tab; }
void cofi_init_provider_defaults(CofiTabProvider *p) { if (p) memset(p, 0, sizeof(*p)); }
int cofi_register_tab_provider(const CofiTabProvider *p) { g_registered_provider = *p; g_registered_provider.tab_mode = (TabMode)(TAB_COUNT + 7); return 0; }
const CofiTabProvider *cofi_get_provider(int provider_id) { return provider_id == 0 ? &g_registered_provider : NULL; }
int cofi_register_command(const CommandSpec *spec) { return spec ? 0 : -1; }
int match_entry_find_index_by_match_id(const MatchEntryManager *manager, int match_id) { if (!manager) return -1; for (int i = 0; i < manager->count; i++) if (manager->entries[i].match_id == match_id) return i; return -1; }
gboolean get_window_state(Display *display, Window window, const char *state_atom_name) { (void)display;(void)window;(void)state_atom_name; return FALSE; }
gboolean window_is_fullscreen(Display *display, Window window) { (void)display;(void)window; return FALSE; }
gboolean window_is_maximized_horizontal(Display *display, Window window) { (void)display;(void)window; return FALSE; }
gboolean window_is_maximized_vertical(Display *display, Window window) { (void)display;(void)window; return FALSE; }
int get_window_desktop(Display *display, Window window) { (void)display;(void)window; return 2; }
int get_current_desktop(Display *display) { (void)display; return 2; }
gboolean get_window_geometry(Display *display, Window window, int *x, int *y, int *w, int *h) { (void)display;(void)window; if (x) *x = 0; if (y) *y = 0; if (w) *w = 100; if (h) *h = 100; return TRUE; }
void set_window_fullscreen(Display *display, Window window, WindowStateAction action) { (void)display;(void)window;(void)action; g_set_state_calls++; }
void set_window_maximized_horizontal(Display *display, Window window, WindowStateAction action) { (void)display;(void)window;(void)action; g_set_state_calls++; }
void set_window_maximized_vertical(Display *display, Window window, WindowStateAction action) { (void)display;(void)window;(void)action; g_set_state_calls++; }
void xmove_resize_frame_aware(Display *display, Window window, int x, int y, int w, int h) { (void)display;(void)window;(void)x;(void)y;(void)w;(void)h; g_move_calls++; }
void move_window_to_desktop(Display *display, Window window, int desktop) { (void)display;(void)window;(void)desktop; g_move_desktop_calls++; }
void switch_to_desktop(Display *display, int desktop) { (void)display;(void)desktop; g_switch_desktop_calls++; }
int XFlush(Display *display) { (void)display; g_flush_calls++; return 0; }
void save_match_entries(const MatchEntryManager *manager) { (void)manager; g_save_match_calls++; }
int matching_create_entry(MatchEntryManager *manager, const WindowInfo *window) {
    (void)window;
    if (!manager) return -1;
    return manager->count > 0 ? manager->entries[manager->count - 1].match_id : -1;
}
bool match_entry_matches_window(const MatchEntry *entry, const WindowInfo *window) {
    if (!entry || !window) return false;
    return strcmp(entry->original_title, window->title) == 0;
}
int match_entry_find_index_by_window(const MatchEntryManager *manager, Window id) {
    if (!manager || id == 0) return -1;
    for (int i = 0; i < manager->count; i++) {
        if (manager->entries[i].bound_x11_id == id) return i;
    }
    return -1;
}
bool match_entry_reassign_live_windows(MatchEntryManager *m, WindowInfo *w, int c) { (void)m;(void)w;(void)c; return false; }

#include "geom/geom_provider.c"
#include "geom/geometry_planner.c"
#include "geom/window_geometry_matching.c"

static void reset_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    layout_store_init(&app->layouts);
    g_reset_selection_calls = 0;
    g_update_display_calls = 0;
    g_matching_run_gc_calls = 0;
    g_save_match_calls = 0;
    g_delete_by_match_id_calls = 0;
    g_last_deleted_match_id = 0;
    g_show_confirm_calls = 0;
    g_confirm_cb = NULL;
    g_geom_rule_sync_calls = 0;
    g_last_synced_match_id = 0;
    g_show_pattern_overlay_calls = 0;
    g_last_pattern_context[0] = '\0';
}

static void seed_layouts(AppData *app) {
    app->matching.count = 2;
    app->matching.entries[0].match_id = 11;
    app->matching.entries[0].assigned = 1;
    app->matching.entries[0].bound_x11_id = 0x111;
    g_strlcpy(app->matching.entries[0].original_title, "Alpha Title", sizeof(app->matching.entries[0].original_title));
    app->matching.entries[1].match_id = 22;
    app->matching.entries[1].assigned = 1;
    app->matching.entries[1].bound_x11_id = 0x222;
    g_strlcpy(app->matching.entries[1].original_title, "Beta Title", sizeof(app->matching.entries[1].original_title));
    app->window_count = 2;
    app->windows[0].id = 0x111;
    g_strlcpy(app->windows[0].class_name, "Firefox", sizeof(app->windows[0].class_name));
    app->windows[1].id = 0x222;
    g_strlcpy(app->windows[1].class_name, "Code", sizeof(app->windows[1].class_name));
    app->layouts.count = 2;
    app->layouts.records[0] = (LayoutRecord){.match_id=11,.x=10,.y=20,.width=800,.height=600,.desktop=3,.maximized_vert=true,.restore_desktop=true};
    app->layouts.records[1] = (LayoutRecord){.match_id=22,.x=30,.y=40,.width=640,.height=480,.desktop=4,.fullscreen=true,.disabled=true};
}

static void test_filter_and_row_format(void) {
    AppData app; CofiRowCells row;
    reset_app(&app); seed_layouts(&app);
    geom_on_query_changed(&app, "Alpha");
    ASSERT_TRUE("filter substring on pattern", app.filtered_geom_count == 1);
    geom_format_row(&app, 0, &row);
    ASSERT_TRUE("row has five cells", row.cell_count == 5);
    ASSERT_TRUE("row label uses pattern", strcmp(row.cells[0].text, "Alpha Title") == 0);
    ASSERT_TRUE("row includes flags", strstr(row.cells[3].text, "V") && strstr(row.cells[3].text, "L"));
}

static void test_delete_flow_and_selection_clamp(void) {
    AppData app; reset_app(&app); seed_layouts(&app);
    geom_on_query_changed(&app, "");
    app.current_tab = geom_tab_mode();
    app.selection.provider_index = 1;
    GdkEventKey ev = {.keyval = GDK_KEY_Delete};
    ASSERT_TRUE("delete key handled", handle_geom_tab_keys(&ev, &app) == TRUE);
    ASSERT_TRUE("delete asks confirm", g_show_confirm_calls == 1 && g_confirm_cb != NULL);
    g_confirm_cb(&app);
    ASSERT_TRUE("delete clears record", app.layouts.count == 1);
    ASSERT_TRUE("delete removes owned match entry",
                g_delete_by_match_id_calls == 1 &&
                g_last_deleted_match_id == 22 &&
                app.matching.count == 1 &&
                match_entry_find_index_by_match_id(&app.matching, 22) == -1 &&
                match_entry_find_index_by_match_id(&app.matching, 11) >= 0);
    ASSERT_TRUE("delete persists matching", g_save_match_calls == 1);
    ASSERT_TRUE("delete syncs geom rule", g_geom_rule_sync_calls == 1 && g_last_synced_match_id == 22);
    ASSERT_TRUE("selection clamped", app.selection.provider_index == 0);
}

static void test_clear_window_geometry_deletes_owned_entry(void) {
    AppData app; reset_app(&app); seed_layouts(&app);
    WindowInfo window = {.id = 0x111};
    g_strlcpy(window.title, "Alpha Title", sizeof(window.title));
    ASSERT_TRUE("geometry clear handled", clear_window_geometry_for_window(&app, &window) == TRUE);
    ASSERT_TRUE("geometry clear removes layout", app.layouts.count == 1 &&
                app.layouts.records[0].match_id == 22);
    ASSERT_TRUE("geometry clear removes owned match entry",
                g_delete_by_match_id_calls == 1 &&
                g_last_deleted_match_id == 11 &&
                app.matching.count == 1 &&
                match_entry_find_index_by_match_id(&app.matching, 11) == -1 &&
                match_entry_find_index_by_match_id(&app.matching, 22) >= 0);
    ASSERT_TRUE("geometry clear persists matching", g_save_match_calls == 1);
}

static void test_toggles_persist(void) {
    AppData app; reset_app(&app); seed_layouts(&app);
    geom_on_query_changed(&app, "");
    app.current_tab = geom_tab_mode();
    app.selection.provider_index = 0;
    GdkEventKey lock = {.keyval = GDK_KEY_l, .state = GDK_CONTROL_MASK};
    GdkEventKey dis = {.keyval = GDK_KEY_t, .state = GDK_CONTROL_MASK};
    ASSERT_TRUE("Ctrl+L handled", handle_geom_tab_keys(&lock, &app) == TRUE);
    ASSERT_TRUE("Ctrl+L flips restore_desktop", app.layouts.records[0].restore_desktop == false);
    ASSERT_TRUE("Ctrl+T handled", handle_geom_tab_keys(&dis, &app) == TRUE);
    ASSERT_TRUE("Ctrl+T flips disabled", app.layouts.records[0].disabled == true);
    ASSERT_TRUE("Ctrl+T syncs geom rule", g_geom_rule_sync_calls == 1 && g_last_synced_match_id == 11);

    GdkEventKey pat = {.keyval = GDK_KEY_p, .state = GDK_CONTROL_MASK};
    ASSERT_TRUE("Ctrl+P handled", handle_geom_tab_keys(&pat, &app) == TRUE);
    ASSERT_TRUE("Ctrl+P opens pattern overlay", g_show_pattern_overlay_calls == 1);
    ASSERT_TRUE("Ctrl+P passes layout context",
                strcmp(g_last_pattern_context, "Layout: 800x600+10+20 (disabled)") == 0);
}

static void test_skip_missing_entry_and_empty_store(void) {
    AppData app; reset_app(&app);
    app.layouts.count = 1;
    app.layouts.records[0].match_id = 999;
    geom_on_query_changed(&app, "");
    ASSERT_TRUE("missing match entry skipped", app.filtered_geom_count == 0);
    ASSERT_TRUE("empty list row count zero", geom_row_count(&app) == 0);
}

static void test_apply_respects_flags(void) {
    WindowGeometryRestoreTarget target = {.window = 0xBEEF, .x = 1, .y = 2, .width = 300, .height = 200, .desktop = 7, .restore_desktop = FALSE};
    g_set_state_calls = g_move_calls = g_move_desktop_calls = g_switch_desktop_calls = g_flush_calls = 0;
    ASSERT_TRUE("apply succeeds", apply_window_geometry_restore((Display *)0x1, &target) == TRUE);
    ASSERT_TRUE("restore_desktop=false skips desktop move", g_move_desktop_calls == 0 && g_switch_desktop_calls == 0);
    target.disabled = TRUE;
    g_set_state_calls = g_move_calls = g_flush_calls = 0;
    ASSERT_TRUE("disabled returns success", apply_window_geometry_restore((Display *)0x1, &target) == TRUE);
    ASSERT_TRUE("disabled no-op", g_set_state_calls == 0 && g_move_calls == 0 && g_flush_calls == 0);
}

int main(void) {
    printf("Geom provider tests\n");
    printf("===================\n\n");
    set_test_home();
    geom_provider_register();
    test_filter_and_row_format();
    test_delete_flow_and_selection_clamp();
    test_clear_window_geometry_deletes_owned_entry();
    test_toggles_persist();
    test_skip_missing_entry_and_empty_store();
    test_apply_respects_flags();
    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    cleanup_test_home();
    return tests_passed == tests_run ? 0 : 1;
}
