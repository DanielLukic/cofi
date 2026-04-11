/*
 * Behavioral tests for repeat-last-action (windows tab, session-only).
 *
 * UX contract:
 *   - '.' with empty query repeats the last successful windows-tab activation
 *   - session-only: no persistence across restarts
 *   - no stored state → no-op (stay open)
 *   - stored query, no current matches → no-op (stay open)
 *   - stored query, matches → activate top match and hide
 */

#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>

#include "../src/app_data.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else       { printf("FAIL: %s\n", name); fail++; } \
} while (0)

/* ---- Stubs ---- */

static int activate_calls = 0;
static Window last_activated = 0;
static int hide_calls = 0;

void activate_window(Display *d, Window w) { (void)d; activate_calls++; last_activated = w; }
void hide_window(AppData *app)             { (void)app; hide_calls++; }
void highlight_window(AppData *app, Window w) { (void)app; (void)w; }
void set_workspace_switch_state(int s)     { (void)s; }
void reset_selection(AppData *app)         { app->selection.window_index = 0; }

/* filter_windows: simple substring match against title for test purposes */
void filter_windows(AppData *app, const char *query) {
    app->filtered_count = 0;
    for (int i = 0; i < app->window_count; i++) {
        if (strlen(query) == 0 || strstr(app->windows[i].title, query)) {
            app->filtered[app->filtered_count++] = app->windows[i];
        }
    }
}

/* get_selected_window: return filtered[window_index] if valid */
WindowInfo *get_selected_window(AppData *app) {
    if (app->filtered_count > 0 && app->selection.window_index < app->filtered_count) {
        return &app->filtered[app->selection.window_index];
    }
    return NULL;
}

/* ---- Include the module under test ---- */
#include "../src/repeat_action.c"

/* ---- Helpers ---- */

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    activate_calls  = 0;
    last_activated  = 0;
    hide_calls      = 0;
}

static void add_window(AppData *app, Window id, const char *title) {
    int i = app->window_count;
    app->windows[i].id = id;
    strncpy(app->windows[i].title, title, MAX_TITLE_LEN - 1);
    strncpy(app->windows[i].type, "Normal", sizeof(app->windows[i].type) - 1);
    app->window_count++;
}

/* ---- Tests ---- */

static void test_initially_no_repeat_state(void) {
    AppData app;
    reset_state(&app);
    ASSERT_TRUE("initial: valid flag is false", !app.last_windows_query_valid);
}

static void test_store_records_query(void) {
    AppData app;
    reset_state(&app);

    store_last_windows_query(&app, "mscr");

    ASSERT_TRUE("store: valid flag set",         app.last_windows_query_valid);
    ASSERT_TRUE("store: query matches",          strcmp(app.last_windows_query, "mscr") == 0);
}

static void test_store_empty_query_is_ignored(void) {
    AppData app;
    reset_state(&app);

    store_last_windows_query(&app, "");

    ASSERT_TRUE("store empty: valid flag NOT set", !app.last_windows_query_valid);
}

static void test_store_empty_does_not_overwrite_prior_query(void) {
    AppData app;
    reset_state(&app);

    store_last_windows_query(&app, "mscr");
    store_last_windows_query(&app, "");

    ASSERT_TRUE("store empty survives: still valid",    app.last_windows_query_valid);
    ASSERT_TRUE("store empty survives: query intact",   strcmp(app.last_windows_query, "mscr") == 0);
}

static void test_repeat_no_stored_state_is_noop(void) {
    AppData app;
    reset_state(&app);
    add_window(&app, 0x100, "Terminal");

    /* last_windows_query_valid defaults to FALSE */
    handle_repeat_key(&app);

    ASSERT_TRUE("no state: activate not called", activate_calls == 0);
    ASSERT_TRUE("no state: hide not called",     hide_calls == 0);
}

static void test_repeat_with_matching_window_activates(void) {
    AppData app;
    reset_state(&app);
    add_window(&app, 0x200, "Terminal");
    add_window(&app, 0x201, "Firefox");

    store_last_windows_query(&app, "Terminal");
    handle_repeat_key(&app);

    ASSERT_TRUE("match: activate called once",   activate_calls == 1);
    ASSERT_TRUE("match: correct window",         last_activated == 0x200);
}

static void test_repeat_hides_after_activation(void) {
    AppData app;
    reset_state(&app);
    add_window(&app, 0x200, "Terminal");

    store_last_windows_query(&app, "Terminal");
    handle_repeat_key(&app);

    ASSERT_TRUE("match: hide called",            hide_calls == 1);
}

static void test_repeat_no_match_is_noop(void) {
    AppData app;
    reset_state(&app);
    add_window(&app, 0x200, "Terminal");

    store_last_windows_query(&app, "zzznomatch");
    handle_repeat_key(&app);

    ASSERT_TRUE("no match: activate not called", activate_calls == 0);
    ASSERT_TRUE("no match: hide not called",     hide_calls == 0);
}

static void test_repeat_refilters_live_list(void) {
    AppData app;
    reset_state(&app);

    /* First activation used "Fire" when Firefox was open */
    store_last_windows_query(&app, "Fire");

    /* Firefox is now gone; only Terminal remains */
    add_window(&app, 0x300, "Terminal");

    handle_repeat_key(&app);

    ASSERT_TRUE("live refilter: no stale match", activate_calls == 0);
    ASSERT_TRUE("live refilter: stay open",      hide_calls == 0);
}

static void test_repeat_picks_top_of_live_list(void) {
    AppData app;
    reset_state(&app);
    /* Two matching windows; expect the first (index 0) to be activated */
    add_window(&app, 0x400, "Terminal A");
    add_window(&app, 0x401, "Terminal B");

    store_last_windows_query(&app, "Terminal");
    handle_repeat_key(&app);

    ASSERT_TRUE("top match: activate called",    activate_calls == 1);
    ASSERT_TRUE("top match: first window",       last_activated == 0x400);
}

/* ---- Interception gate tests ---- */
/*
 * repeat_key_should_trigger(tab, query) encodes the gate logic from
 * key_handler.c without needing GTK: fire only on TAB_WINDOWS + empty query.
 */
static int repeat_key_should_trigger(TabMode tab, const char *entry_text) {
    return (tab == TAB_WINDOWS && strlen(entry_text) == 0);
}

static void test_gate_empty_windows_triggers(void) {
    ASSERT_TRUE("gate: empty query on windows tab triggers",
                repeat_key_should_trigger(TAB_WINDOWS, "") == 1);
}

static void test_gate_nonempty_windows_does_not_trigger(void) {
    ASSERT_TRUE("gate: non-empty query on windows tab does NOT trigger",
                repeat_key_should_trigger(TAB_WINDOWS, "mscr") == 0);
}

static void test_gate_empty_other_tab_does_not_trigger(void) {
    ASSERT_TRUE("gate: empty query on workspaces tab does NOT trigger",
                repeat_key_should_trigger(TAB_WORKSPACES, "") == 0);
    ASSERT_TRUE("gate: empty query on harpoon tab does NOT trigger",
                repeat_key_should_trigger(TAB_HARPOON, "") == 0);
}

/* ---- Main ---- */

int main(void) {
    test_initially_no_repeat_state();
    test_store_records_query();
    test_store_empty_query_is_ignored();
    test_store_empty_does_not_overwrite_prior_query();
    test_repeat_no_stored_state_is_noop();
    test_repeat_with_matching_window_activates();
    test_repeat_hides_after_activation();
    test_repeat_no_match_is_noop();
    test_repeat_refilters_live_list();
    test_repeat_picks_top_of_live_list();
    test_gate_empty_windows_triggers();
    test_gate_nonempty_windows_does_not_trigger();
    test_gate_empty_other_tab_does_not_trigger();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return (fail == 0) ? 0 : 1;
}
