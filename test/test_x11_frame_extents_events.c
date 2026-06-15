#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "x11/frame_extents_restore.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(msg, cond) \
    do { \
        tests_run++; \
        if (cond) { \
            tests_passed++; \
            printf("PASS: %s\n", msg); \
        } else { \
            printf("FAIL: %s\n", msg); \
        } \
    } while (0)

typedef struct {
    guint id;
    guint interval;
    GSourceFunc function;
    gpointer data;
    gboolean active;
} FakeTimeout;

static FakeTimeout fake_timeouts[8];
static guint next_source_id = 1;
static int restore_calls = 0;
static Window restored_windows[8];
static guint removed_source_ids[8];
static int removed_source_count = 0;

gboolean restore_window_geometry_for_window(AppData *app, const WindowInfo *window) {
    (void)app;
    if (restore_calls < 8) {
        restored_windows[restore_calls] = window ? window->id : 0;
    }
    restore_calls++;
    return TRUE;
}

static guint fake_timeout_add(guint interval, GSourceFunc function, gpointer data) {
    for (int i = 0; i < 8; i++) {
        if (!fake_timeouts[i].active && fake_timeouts[i].data == NULL) {
            fake_timeouts[i].id = next_source_id++;
            fake_timeouts[i].interval = interval;
            fake_timeouts[i].function = function;
            fake_timeouts[i].data = data;
            fake_timeouts[i].active = TRUE;
            return fake_timeouts[i].id;
        }
    }
    return 0;
}

static gboolean fake_source_remove(guint source_id) {
    for (int i = 0; i < 8; i++) {
        if (fake_timeouts[i].id == source_id && fake_timeouts[i].active) {
            if (removed_source_count < 8) {
                removed_source_ids[removed_source_count++] = source_id;
            }
            fake_timeouts[i].active = FALSE;
            fake_timeouts[i].function = NULL;
            fake_timeouts[i].data = NULL;
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean source_was_removed(guint source_id) {
    for (int i = 0; i < removed_source_count; i++) {
        if (removed_source_ids[i] == source_id) {
            return TRUE;
        }
    }
    return FALSE;
}

static int active_timeout_count(void) {
    int count = 0;
    for (int i = 0; i < 8; i++) {
        if (fake_timeouts[i].active) {
            count++;
        }
    }
    return count;
}

static FakeTimeout *find_timeout(guint source_id) {
    for (int i = 0; i < 8; i++) {
        if (fake_timeouts[i].id == source_id) {
            return &fake_timeouts[i];
        }
    }
    return NULL;
}

static void fire_timeout(guint source_id) {
    FakeTimeout *timeout = find_timeout(source_id);
    if (!timeout || !timeout->active || !timeout->function) {
        return;
    }

    GSourceFunc function = timeout->function;
    gpointer data = timeout->data;
    timeout->active = FALSE;
    timeout->function = NULL;
    timeout->data = NULL;
    function(data);
}

static void reset_state(void) {
    cleanup_frame_extents_restore_timeouts();
    memset(fake_timeouts, 0, sizeof(fake_timeouts));
    next_source_id = 1;
    restore_calls = 0;
    memset(restored_windows, 0, sizeof(restored_windows));
    memset(removed_source_ids, 0, sizeof(removed_source_ids));
    removed_source_count = 0;
    frame_extents_restore_set_timeout_add_for_test(fake_timeout_add);
    frame_extents_restore_set_source_remove_for_test(fake_source_remove);
}

static void init_app_with_windows(AppData *app, const Window *ids, int count) {
    memset(app, 0, sizeof(*app));
    app->window_count = count;
    for (int i = 0; i < count; i++) {
        app->windows[i].id = ids[i];
    }
}

static void test_unknown_window_is_ignored(void) {
    AppData app;
    Window ids[] = {0x100};
    reset_state();
    init_app_with_windows(&app, ids, 1);

    handle_net_frame_extents_property(&app, 0x200);

    ASSERT_TRUE("unknown window does not schedule restore", active_timeout_count() == 0);
    ASSERT_TRUE("unknown window does not restore", restore_calls == 0);
}

static void test_known_window_schedules_150ms_restore(void) {
    AppData app;
    Window ids[] = {0x100};
    reset_state();
    init_app_with_windows(&app, ids, 1);

    handle_net_frame_extents_property(&app, 0x100);

    ASSERT_TRUE("known window schedules one restore", active_timeout_count() == 1);
    ASSERT_TRUE("restore delay is 150ms", fake_timeouts[0].interval == 150);
}

static void test_restore_is_not_synchronous(void) {
    AppData app;
    Window ids[] = {0x100};
    reset_state();
    init_app_with_windows(&app, ids, 1);

    handle_net_frame_extents_property(&app, 0x100);

    ASSERT_TRUE("_NET_FRAME_EXTENTS does not restore synchronously", restore_calls == 0);
}

static void test_timeout_restores_live_window(void) {
    AppData app;
    Window ids[] = {0x100};
    reset_state();
    init_app_with_windows(&app, ids, 1);

    handle_net_frame_extents_property(&app, 0x100);
    guint source_id = fake_timeouts[0].id;
    fire_timeout(source_id);

    ASSERT_TRUE("timeout restores once", restore_calls == 1);
    ASSERT_TRUE("timeout restores target window", restored_windows[0] == 0x100);
}

static void test_callback_consumes_pending_timeout(void) {
    AppData app;
    Window ids[] = {0x100};
    reset_state();
    init_app_with_windows(&app, ids, 1);

    handle_net_frame_extents_property(&app, 0x100);
    guint source_id = fake_timeouts[0].id;
    fire_timeout(source_id);

    ASSERT_TRUE("fired timeout is removed from pending set", active_timeout_count() == 0);
}

static void test_second_extents_event_replaces_pending_restore(void) {
    AppData app;
    Window ids[] = {0x100};
    reset_state();
    init_app_with_windows(&app, ids, 1);

    handle_net_frame_extents_property(&app, 0x100);
    guint first_source = fake_timeouts[0].id;
    handle_net_frame_extents_property(&app, 0x100);
    guint second_source = fake_timeouts[0].id;

    ASSERT_TRUE("second extents event cancels first timeout",
                source_was_removed(first_source));
    ASSERT_TRUE("second extents event leaves one pending timeout", active_timeout_count() == 1);

    fire_timeout(second_source);

    ASSERT_TRUE("replacement restore fires once", restore_calls == 1);
    ASSERT_TRUE("replacement restore targets same window", restored_windows[0] == 0x100);
}

static void test_removed_window_before_timeout_is_noop(void) {
    AppData app;
    Window ids[] = {0x100};
    reset_state();
    init_app_with_windows(&app, ids, 1);

    handle_net_frame_extents_property(&app, 0x100);
    guint source_id = fake_timeouts[0].id;
    app.window_count = 0;
    fire_timeout(source_id);

    ASSERT_TRUE("removed window is not restored", restore_calls == 0);
    ASSERT_TRUE("removed-window timeout is consumed", active_timeout_count() == 0);
}

int main(void) {
    test_unknown_window_is_ignored();
    test_known_window_schedules_150ms_restore();
    test_restore_is_not_synchronous();
    test_timeout_restores_live_window();
    test_callback_consumes_pending_timeout();
    test_second_extents_event_replaces_pending_restore();
    test_removed_window_before_timeout_is_noop();

    reset_state();
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
