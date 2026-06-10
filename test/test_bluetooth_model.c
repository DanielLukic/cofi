#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "bluetooth/bluetooth_bluez.h"
#include "bluetooth/bluetooth_model.h"
#include "core/app/app_data.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(msg, cond) \
    do { \
        tests_run++; \
        if (cond) { \
            tests_passed++; \
            printf("PASS: %s\n", msg); \
        } else { \
            printf("FAIL: %s (line %d)\n", msg, __LINE__); \
        } \
    } while (0)

static int g_update_display_calls;
static int g_refresh_requests;
static int g_connect_requests;
static int g_disconnect_requests;
static guint g_last_refresh_generation;
static int g_last_op_generation;
static BtOpType g_last_op_type;
static char g_last_device_path[BT_PATH_LEN];
static BusState g_fake_bus_state = BUS_READY;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

void update_display(AppData *app) {
    (void)app;
    g_update_display_calls++;
}

void preserve_selection(AppData *app) {
    if (!app) {
        return;
    }
    const char *id = bluetooth_model_row_identity(app, app->selection.provider_index);
    if (!id || id[0] == '\0') {
        app->selection.selected_provider_id[0] = '\0';
        return;
    }
    g_strlcpy(app->selection.selected_provider_id,
              id,
              sizeof(app->selection.selected_provider_id));
}

void restore_selection(AppData *app) {
    if (!app) {
        return;
    }
    if (app->selection.selected_provider_id[0] == '\0') {
        app->selection.provider_index = 0;
        return;
    }

    int count = bluetooth_model_row_count(app);
    for (int i = 0; i < count; i++) {
        const char *id = bluetooth_model_row_identity(app, i);
        if (id && strcmp(id, app->selection.selected_provider_id) == 0) {
            app->selection.provider_index = i;
            return;
        }
    }
    app->selection.provider_index = 0;
}

void update_scroll_position(AppData *app) {
    (void)app;
}

BusState bluetooth_bluez_bus_state(void) {
    return g_fake_bus_state;
}

void bluetooth_bluez_ensure_bus(AppData *app) {
    (void)app;
    g_fake_bus_state = BUS_READY;
}

gboolean bluetooth_bluez_request_refresh(AppData *app,
                                         guint generation,
                                         GCancellable *cancel) {
    (void)app;
    (void)cancel;
    g_refresh_requests++;
    g_last_refresh_generation = generation;
    return TRUE;
}

gboolean bluetooth_bluez_request_device_op(AppData *app, BtOpContext *ctx) {
    (void)app;
    g_connect_requests += ctx->op_type == BT_OP_CONNECTING;
    g_disconnect_requests += ctx->op_type == BT_OP_DISCONNECTING;
    g_last_op_generation = ctx->generation;
    g_last_op_type = ctx->op_type;
    g_strlcpy(g_last_device_path, ctx->device_path, sizeof(g_last_device_path));
    return TRUE;
}

void bluetooth_bluez_shutdown(void) {
}

#include "bluetooth/bluetooth_model.c"

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    init_bluetooth_mode(&app->bluetooth_mode);
    g_update_display_calls = 0;
    g_refresh_requests = 0;
    g_connect_requests = 0;
    g_disconnect_requests = 0;
    g_last_refresh_generation = 0;
    g_last_op_generation = 0;
    g_last_op_type = BT_OP_NONE;
    g_last_device_path[0] = '\0';
    g_fake_bus_state = BUS_READY;
}

static BtSnapshot sample_snapshot(void) {
    BtSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.adapter_count = 1;
    g_strlcpy(snapshot.adapters[0].path, "/org/bluez/hci0", sizeof(snapshot.adapters[0].path));
    g_strlcpy(snapshot.adapters[0].name, "hci0", sizeof(snapshot.adapters[0].name));
    g_strlcpy(snapshot.adapters[0].address, "AA:BB:CC:DD:EE:FF", sizeof(snapshot.adapters[0].address));
    snapshot.adapters[0].powered = TRUE;

    snapshot.device_count = 1;
    g_strlcpy(snapshot.devices[0].path, "/org/bluez/hci0/dev_11_22_33_44_55_66",
              sizeof(snapshot.devices[0].path));
    g_strlcpy(snapshot.devices[0].adapter_path, "/org/bluez/hci0",
              sizeof(snapshot.devices[0].adapter_path));
    g_strlcpy(snapshot.devices[0].alias, "WH-1000XM4", sizeof(snapshot.devices[0].alias));
    g_strlcpy(snapshot.devices[0].name, "Sony Headphones", sizeof(snapshot.devices[0].name));
    g_strlcpy(snapshot.devices[0].icon, "audio-headphones", sizeof(snapshot.devices[0].icon));
    snapshot.devices[0].paired = TRUE;
    snapshot.devices[0].connected = FALSE;
    return snapshot;
}

static void test_icon_mapping(void) {
    AppData app;
    reset_state(&app);

    ASSERT_TRUE("headphones icon maps to glyph",
                strcmp(bluetooth_icon_glyph("audio-headphones"), "🎧") == 0);
    ASSERT_TRUE("phone icon maps to glyph",
                strcmp(bluetooth_icon_glyph("phone"), "📱") == 0);
    ASSERT_TRUE("unknown icon falls back",
                strcmp(bluetooth_icon_glyph("mystery"), "•") == 0);
    ASSERT_TRUE("connected state glyph maps to green",
                strcmp(bluetooth_connected_glyph(TRUE), "🟢") == 0);
    ASSERT_TRUE("disconnected state glyph maps to white",
                strcmp(bluetooth_connected_glyph(FALSE), "⚪") == 0);
}

static void fill_device(BtParsedDevice *device,
                        const char *path,
                        const char *alias,
                        gboolean connected) {
    memset(device, 0, sizeof(*device));
    g_strlcpy(device->path, path, sizeof(device->path));
    g_strlcpy(device->adapter_path, "/org/bluez/hci0", sizeof(device->adapter_path));
    g_strlcpy(device->alias, alias, sizeof(device->alias));
    g_strlcpy(device->name, alias, sizeof(device->name));
    g_strlcpy(device->icon, "audio-headphones", sizeof(device->icon));
    device->paired = TRUE;
    device->connected = connected;
}

static void test_snapshot_order_is_deterministic_by_alias_then_path(void) {
    AppData app;
    reset_state(&app);

    BtSnapshot first;
    memset(&first, 0, sizeof(first));
    first.adapter_count = 1;
    g_strlcpy(first.adapters[0].path, "/org/bluez/hci0", sizeof(first.adapters[0].path));
    g_strlcpy(first.adapters[0].name, "hci0", sizeof(first.adapters[0].name));
    first.adapters[0].powered = TRUE;
    first.device_count = 3;
    fill_device(&first.devices[0], "/org/bluez/hci0/dev_C", "Bravo", FALSE);
    fill_device(&first.devices[1], "/org/bluez/hci0/dev_B", "Alpha", FALSE);
    fill_device(&first.devices[2], "/org/bluez/hci0/dev_A", "Alpha", TRUE);

    BtSnapshot second = first;
    second.devices[0] = first.devices[2];
    second.devices[1] = first.devices[0];
    second.devices[2] = first.devices[1];

    bluetooth_model_apply_snapshot(&app, &first);
    ASSERT_TRUE("first snapshot sorts alias alpha path a first",
                strcmp(app.bluetooth_mode.devices[0].path,
                       "/org/bluez/hci0/dev_A") == 0);
    ASSERT_TRUE("first snapshot sorts alias alpha path b second",
                strcmp(app.bluetooth_mode.devices[1].path,
                       "/org/bluez/hci0/dev_B") == 0);
    ASSERT_TRUE("first snapshot sorts bravo third",
                strcmp(app.bluetooth_mode.devices[2].path,
                       "/org/bluez/hci0/dev_C") == 0);

    bluetooth_model_apply_snapshot(&app, &second);
    ASSERT_TRUE("second snapshot keeps alias/path order stable row 0",
                strcmp(app.bluetooth_mode.devices[0].path,
                       "/org/bluez/hci0/dev_A") == 0);
    ASSERT_TRUE("second snapshot keeps alias/path order stable row 1",
                strcmp(app.bluetooth_mode.devices[1].path,
                       "/org/bluez/hci0/dev_B") == 0);
    ASSERT_TRUE("second snapshot keeps alias/path order stable row 2",
                strcmp(app.bluetooth_mode.devices[2].path,
                       "/org/bluez/hci0/dev_C") == 0);
}

static void test_connect_success_sets_connected_transient(void) {
    AppData app;
    reset_state(&app);
    BtSnapshot snapshot = sample_snapshot();
    bluetooth_model_apply_snapshot(&app, &snapshot);

    ASSERT_TRUE("toggle starts connect op", bluetooth_model_toggle_device(&app, 0) == TRUE);
    ASSERT_TRUE("connect request issued once", g_connect_requests == 1);
    ASSERT_TRUE("connect op path recorded",
                strcmp(g_last_device_path, snapshot.devices[0].path) == 0);
    ASSERT_TRUE("row shows connecting",
                strcmp(bluetooth_model_device_state_text(app.bluetooth_mode.devices + 0),
                       "[…connecting…]") == 0);
    ASSERT_TRUE("persistent state stays disconnected while connecting",
                strcmp(bluetooth_connected_glyph(app.bluetooth_mode.devices[0].connected),
                       "⚪") == 0);

    bluetooth_model_complete_operation(&app,
                                       bluetooth_op_context_ref(app.bluetooth_mode.devices[0].active_op),
                                       TRUE,
                                       FALSE,
                                       NULL,
                                       NULL);

    ASSERT_TRUE("device becomes connected", app.bluetooth_mode.devices[0].connected == TRUE);
    ASSERT_TRUE("persistent state flips connected on success",
                strcmp(bluetooth_connected_glyph(app.bluetooth_mode.devices[0].connected),
                       "🟢") == 0);
    ASSERT_TRUE("row shows connected transient",
                strcmp(bluetooth_model_device_state_text(app.bluetooth_mode.devices + 0),
                       "[connected]") == 0);
}

static void test_connect_failure_sets_failed_state(void) {
    AppData app;
    reset_state(&app);
    BtSnapshot snapshot = sample_snapshot();
    bluetooth_model_apply_snapshot(&app, &snapshot);

    ASSERT_TRUE("connect starts op", bluetooth_model_connect_device(&app, 0) == TRUE);
    bluetooth_model_complete_operation(&app,
                                       bluetooth_op_context_ref(app.bluetooth_mode.devices[0].active_op),
                                       FALSE,
                                       FALSE,
                                       "org.bluez.Error.Failed",
                                       "device missing");

    ASSERT_TRUE("device remains disconnected", app.bluetooth_mode.devices[0].connected == FALSE);
    ASSERT_TRUE("row shows failed transient",
                strcmp(bluetooth_model_device_state_text(app.bluetooth_mode.devices + 0),
                       "[failed]") == 0);
}

static void test_generation_mismatch_bails_without_mutating(void) {
    AppData app;
    reset_state(&app);
    BtSnapshot snapshot = sample_snapshot();
    bluetooth_model_apply_snapshot(&app, &snapshot);

    ASSERT_TRUE("first connect starts", bluetooth_model_connect_device(&app, 0) == TRUE);
    BtOpContext *first_ctx = bluetooth_op_context_ref(app.bluetooth_mode.devices[0].active_op);
    ASSERT_TRUE("second connect while in flight starts replacement",
                bluetooth_model_disconnect_device(&app, 0) == TRUE);
    BtOpContext *second_ctx = bluetooth_op_context_ref(app.bluetooth_mode.devices[0].active_op);

    bluetooth_model_complete_operation(&app,
                                       first_ctx,
                                       TRUE,
                                       FALSE,
                                       NULL,
                                       NULL);

    ASSERT_TRUE("stale callback does not connect device",
                app.bluetooth_mode.devices[0].connected == FALSE);
    ASSERT_TRUE("current row still shows disconnecting",
                strcmp(bluetooth_model_device_state_text(app.bluetooth_mode.devices + 0),
                       "[…disconnecting…]") == 0);

    bluetooth_model_complete_operation(&app,
                                       second_ctx,
                                       TRUE,
                                       FALSE,
                                       NULL,
                                       NULL);

    ASSERT_TRUE("current callback clears to disconnected transient",
                strcmp(bluetooth_model_device_state_text(app.bluetooth_mode.devices + 0),
                       "[disconnected]") == 0);
}

static void test_hidden_dirty_does_not_update_until_enter(void) {
    AppData app;
    reset_state(&app);
    BtSnapshot snapshot = sample_snapshot();
    app.window_visible = FALSE;

    bluetooth_model_apply_snapshot(&app, &snapshot);
    ASSERT_TRUE("hidden snapshot does not redraw", g_update_display_calls == 0);
    ASSERT_TRUE("hidden snapshot marks mode dirty", app.bluetooth_mode.dirty == TRUE);

    app.window_visible = TRUE;
    app.current_tab = 9;
    bluetooth_model_on_enter(&app);

    ASSERT_TRUE("enter redraws dirty bluetooth state", g_update_display_calls == 1);
    ASSERT_TRUE("enter clears dirty flag", app.bluetooth_mode.dirty == FALSE);
}

static void test_snapshot_preserves_selected_device_identity(void) {
    AppData app;
    reset_state(&app);

    BtSnapshot first;
    memset(&first, 0, sizeof(first));
    first.adapter_count = 1;
    g_strlcpy(first.adapters[0].path, "/org/bluez/hci0", sizeof(first.adapters[0].path));
    g_strlcpy(first.adapters[0].name, "hci0", sizeof(first.adapters[0].name));
    first.adapters[0].powered = TRUE;
    first.device_count = 3;
    fill_device(&first.devices[0], "/org/bluez/hci0/dev_C", "Charlie", FALSE);
    fill_device(&first.devices[1], "/org/bluez/hci0/dev_B", "Bravo", FALSE);
    fill_device(&first.devices[2], "/org/bluez/hci0/dev_A", "Alpha", FALSE);

    BtSnapshot second = first;
    second.devices[0] = first.devices[1];
    second.devices[1] = first.devices[2];
    second.devices[2] = first.devices[0];

    app.current_tab = 7;
    app.bluetooth_mode.tab_mode = 7;

    bluetooth_model_apply_snapshot(&app, &first);
    app.selection.provider_index = 1;
    ASSERT_TRUE("initial selected row is bravo",
                strcmp(bluetooth_model_row_identity(&app, 1),
                       "/org/bluez/hci0/dev_B") == 0);

    bluetooth_model_apply_snapshot(&app, &second);

    ASSERT_TRUE("selection index preserved after snapshot",
                app.selection.provider_index == 1);
    ASSERT_TRUE("selection still points at bravo identity",
                strcmp(bluetooth_model_row_identity(&app, app.selection.provider_index),
                       "/org/bluez/hci0/dev_B") == 0);
}

int main(void) {
    printf("bluetooth model tests\n");
    printf("=====================\n\n");

    test_icon_mapping();
    test_snapshot_order_is_deterministic_by_alias_then_path();
    test_connect_success_sets_connected_transient();
    test_connect_failure_sets_failed_state();
    test_generation_mismatch_bails_without_mutating();
    test_hidden_dirty_does_not_update_until_enter();
    test_snapshot_preserves_selected_device_identity();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_run == tests_passed ? 0 : 1;
}
