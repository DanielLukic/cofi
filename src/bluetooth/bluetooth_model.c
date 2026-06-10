#include "bluetooth/bluetooth_model.h"

#include <string.h>

#include "bluetooth/bluetooth_bluez.h"
#include "core/app/app_data.h"
#include "core/log/log.h"
#include "core/selection/selection.h"
#include "matching/fzf_algo.h"
#include "ui/display.h"

enum {
    BT_TERMINAL_STATE_USEC = 2 * G_USEC_PER_SEC,
};

typedef struct {
    int raw_idx;
    score_t score;
} BtScoredRow;

static BluetoothMode *bt_mode(AppData *app) {
    return app ? &app->bluetooth_mode : NULL;
}

BtOpContext *bluetooth_op_context_ref(BtOpContext *ctx) {
    if (ctx) {
        ctx->refcount++;
    }
    return ctx;
}

void bluetooth_op_context_unref(BtOpContext *ctx) {
    if (!ctx) {
        return;
    }
    ctx->refcount--;
    if (ctx->refcount <= 0) {
        g_clear_object(&ctx->cancel);
        g_free(ctx);
    }
}

static gint64 bt_now_usec(void) {
    return g_get_monotonic_time();
}

static void bt_mark_dirty_and_maybe_update(AppData *app) {
    BluetoothMode *mode = bt_mode(app);
    if (!mode) {
        return;
    }
    mode->dirty = TRUE;
    if (app->window_visible && (int)app->current_tab == mode->tab_mode) {
        update_display(app);
        mode->dirty = FALSE;
    }
}

static void bt_clear_terminal_state(BtDeviceEntry *device) {
    if (!device) {
        return;
    }
    device->transient_state = BT_TRANSIENT_NONE;
    device->transient_until_us = 0;
}

static void bt_prune_expired_states(BluetoothMode *mode, gint64 now_usec) {
    if (!mode) {
        return;
    }
    for (int i = 0; i < mode->device_count; i++) {
        BtDeviceEntry *device = &mode->devices[i];
        if (device->active_op) {
            continue;
        }
        if (device->transient_state != BT_TRANSIENT_NONE &&
            device->transient_until_us > 0 &&
            device->transient_until_us <= now_usec) {
            bt_clear_terminal_state(device);
        }
    }
}

static void bt_set_status_message(BluetoothMode *mode, const char *message) {
    if (!mode) {
        return;
    }
    g_strlcpy(mode->status_message, message ? message : "", sizeof(mode->status_message));
}

static BtDeviceEntry *bt_find_device(AppData *app, const char *device_path) {
    BluetoothMode *mode = bt_mode(app);
    if (!mode || !device_path || device_path[0] == '\0') {
        return NULL;
    }
    for (int i = 0; i < mode->device_count; i++) {
        if (strcmp(mode->devices[i].path, device_path) == 0) {
            return &mode->devices[i];
        }
    }
    return NULL;
}

static const BtDeviceEntry *bt_device_at_visible_const(AppData *app, int visible_idx) {
    BluetoothMode *mode = bt_mode(app);
    if (!mode || visible_idx < 0 || visible_idx >= mode->filtered_count) {
        return NULL;
    }
    int raw_idx = mode->filtered_indices[visible_idx];
    if (raw_idx < 0 || raw_idx >= mode->device_count) {
        return NULL;
    }
    return &mode->devices[raw_idx];
}

static BtDeviceEntry *bt_device_at_visible(AppData *app, int visible_idx) {
    return (BtDeviceEntry *)bt_device_at_visible_const(app, visible_idx);
}

static void bt_copy_filtered_query(BluetoothMode *mode, const char *query) {
    g_strlcpy(mode->query, query ? query : "", sizeof(mode->query));
}

static int bt_compare_scores(const void *lhs, const void *rhs) {
    const BtScoredRow *a = (const BtScoredRow *)lhs;
    const BtScoredRow *b = (const BtScoredRow *)rhs;
    if (a->score > b->score) return -1;
    if (a->score < b->score) return 1;
    if (a->raw_idx < b->raw_idx) return -1;
    if (a->raw_idx > b->raw_idx) return 1;
    return 0;
}

static void bt_build_match_text(const BtDeviceEntry *device, char *out, size_t out_size) {
    if (!out || out_size == 0) {
        return;
    }
    if (!device) {
        out[0] = '\0';
        return;
    }
    g_snprintf(out, out_size, "%s %s",
               device->alias[0] ? device->alias : device->path,
               device->adapter_name);
}

static void bt_refilter(AppData *app) {
    BluetoothMode *mode = bt_mode(app);
    if (!mode) {
        return;
    }

    bt_prune_expired_states(mode, bt_now_usec());
    mode->filtered_count = 0;

    if (mode->device_count <= 0) {
        return;
    }

    if (mode->query[0] == '\0') {
        for (int i = 0; i < mode->device_count; i++) {
            mode->filtered_indices[mode->filtered_count++] = i;
        }
        return;
    }

    BtScoredRow scored[BT_MAX_DEVICES];
    int scored_count = 0;
    char match_text[BT_NAME_LEN * 2 + 8];
    for (int i = 0; i < mode->device_count; i++) {
        bt_build_match_text(&mode->devices[i], match_text, sizeof(match_text));
        if (!fzf_has_match(mode->query, match_text)) {
            continue;
        }
        scored[scored_count].raw_idx = i;
        scored[scored_count].score = fzf_fuzzy_match(mode->query, match_text);
        scored_count++;
    }

    qsort(scored, (size_t)scored_count, sizeof(scored[0]), bt_compare_scores);
    for (int i = 0; i < scored_count; i++) {
        mode->filtered_indices[mode->filtered_count++] = scored[i].raw_idx;
    }
}

static gboolean bt_refresh_in_flight(BluetoothMode *mode) {
    return mode && mode->refresh_cancel != NULL;
}

static void bt_start_refresh(AppData *app) {
    BluetoothMode *mode = bt_mode(app);
    if (!mode || bt_refresh_in_flight(mode)) {
        return;
    }

    mode->refresh_generation++;
    mode->loading = TRUE;
    g_clear_object(&mode->refresh_cancel);
    mode->refresh_cancel = g_cancellable_new();
    bt_set_status_message(mode, "");

    if (!bluetooth_bluez_request_refresh(app, mode->refresh_generation, mode->refresh_cancel)) {
        bt_set_status_message(mode, "BlueZ refresh failed");
        mode->loading = FALSE;
        g_clear_object(&mode->refresh_cancel);
        bt_mark_dirty_and_maybe_update(app);
    }
}

static gboolean bt_prepare_device_operation(AppData *app,
                                            BtDeviceEntry *device,
                                            BtOpType op_type) {
    if (!app || !device || op_type == BT_OP_NONE) {
        return FALSE;
    }

    if (device->active_op) {
        if (device->active_op->cancel) {
            g_cancellable_cancel(device->active_op->cancel);
        }
        bluetooth_op_context_unref(device->active_op);
        device->active_op = NULL;
    }

    device->generation++;
    bt_clear_terminal_state(device);

    BtOpContext *ctx = g_new0(BtOpContext, 1);
    ctx->refcount = 1;
    g_strlcpy(ctx->device_path, device->path, sizeof(ctx->device_path));
    ctx->op_type = op_type;
    ctx->generation = device->generation;
    ctx->cancel = g_cancellable_new();
    device->active_op = ctx;

    if (!bluetooth_bluez_request_device_op(app, ctx)) {
        device->active_op = NULL;
        bluetooth_op_context_unref(ctx);
        device->transient_state = BT_TRANSIENT_FAILED;
        device->transient_until_us = bt_now_usec() + BT_TERMINAL_STATE_USEC;
        bt_mark_dirty_and_maybe_update(app);
        return FALSE;
    }

    bt_mark_dirty_and_maybe_update(app);
    return TRUE;
}

static void bt_copy_device_entry(BtDeviceEntry *dest,
                                 const BtParsedDevice *src,
                                 const BtDeviceEntry *previous,
                                 const BtAdapterInfo *adapter) {
    memset(dest, 0, sizeof(*dest));
    g_strlcpy(dest->path, src->path, sizeof(dest->path));
    g_strlcpy(dest->adapter_path, src->adapter_path, sizeof(dest->adapter_path));
    g_strlcpy(dest->alias, src->alias[0] ? src->alias : src->name, sizeof(dest->alias));
    g_strlcpy(dest->adapter_name, adapter ? adapter->name : "", sizeof(dest->adapter_name));
    g_strlcpy(dest->icon, src->icon, sizeof(dest->icon));
    dest->connected = src->connected;

    if (!previous) {
        return;
    }

    dest->generation = previous->generation;
    dest->active_op = previous->active_op;
    dest->transient_state = previous->transient_state;
    dest->transient_until_us = previous->transient_until_us;
}

static const BtAdapterInfo *bt_find_adapter(const BtSnapshot *snapshot, const char *path) {
    if (!snapshot || !path || path[0] == '\0') {
        return NULL;
    }
    for (int i = 0; i < snapshot->adapter_count; i++) {
        if (strcmp(snapshot->adapters[i].path, path) == 0) {
            return &snapshot->adapters[i];
        }
    }
    return NULL;
}

static int bt_compare_device_entries(const void *lhs, const void *rhs) {
    const BtDeviceEntry *a = (const BtDeviceEntry *)lhs;
    const BtDeviceEntry *b = (const BtDeviceEntry *)rhs;
    int alias_cmp = strcmp(a->alias, b->alias);
    if (alias_cmp != 0) {
        return alias_cmp;
    }
    return strcmp(a->path, b->path);
}

void cleanup_bluetooth_mode(BluetoothMode *mode) {
    if (!mode) {
        return;
    }
    if (mode->refresh_cancel) {
        g_cancellable_cancel(mode->refresh_cancel);
    }
    g_clear_object(&mode->refresh_cancel);
    for (int i = 0; i < mode->device_count; i++) {
        if (mode->devices[i].active_op && mode->devices[i].active_op->cancel) {
            g_cancellable_cancel(mode->devices[i].active_op->cancel);
        }
        bluetooth_op_context_unref(mode->devices[i].active_op);
        mode->devices[i].active_op = NULL;
    }
    bluetooth_bluez_shutdown();
    memset(mode, 0, sizeof(*mode));
    mode->tab_mode = -1;
}

void bluetooth_model_on_enter(AppData *app) {
    BluetoothMode *mode = bt_mode(app);
    if (!mode) {
        return;
    }

    mode->tab_mode = app->current_tab;
    bt_prune_expired_states(mode, bt_now_usec());

    if (mode->dirty && app->window_visible) {
        update_display(app);
        mode->dirty = FALSE;
    }

    bluetooth_model_refresh(app);
}

void bluetooth_model_on_leave(AppData *app) {
    BluetoothMode *mode = bt_mode(app);
    if (!mode || !mode->refresh_cancel) {
        return;
    }
    g_cancellable_cancel(mode->refresh_cancel);
}

void bluetooth_model_on_tick(AppData *app, int generation) {
    (void)generation;
    BluetoothMode *mode = bt_mode(app);
    if (!mode) {
        return;
    }
    bt_prune_expired_states(mode, bt_now_usec());
    if (!bt_refresh_in_flight(mode)) {
        bluetooth_model_refresh(app);
    }
}

void bluetooth_model_on_query_changed(AppData *app, const char *query) {
    BluetoothMode *mode = bt_mode(app);
    if (!mode) {
        return;
    }
    bt_copy_filtered_query(mode, query);
    bt_refilter(app);
}

int bluetooth_model_row_count(AppData *app) {
    BluetoothMode *mode = bt_mode(app);
    if (!mode) {
        return 0;
    }
    return mode->filtered_count > 0 ? mode->filtered_count : 1;
}

const BtDeviceEntry *bluetooth_model_device_at_visible(AppData *app, int visible_idx) {
    return bt_device_at_visible_const(app, visible_idx);
}

const char *bluetooth_model_match_string(AppData *app, int visible_idx) {
    static char match_text[BT_NAME_LEN * 2 + 8];
    bt_build_match_text(bt_device_at_visible_const(app, visible_idx), match_text, sizeof(match_text));
    return match_text;
}

const char *bluetooth_model_row_identity(AppData *app, int visible_idx) {
    const BtDeviceEntry *device = bt_device_at_visible_const(app, visible_idx);
    return device ? device->path : "";
}

const char *bluetooth_model_device_state_text(BtDeviceEntry *device) {
    if (!device) {
        return "";
    }
    if (device->active_op) {
        return device->active_op->op_type == BT_OP_CONNECTING
            ? "[…connecting…]"
            : "[…disconnecting…]";
    }
    switch (device->transient_state) {
        case BT_TRANSIENT_CONNECTED:
            return "[connected]";
        case BT_TRANSIENT_DISCONNECTED:
            return "[disconnected]";
        case BT_TRANSIENT_FAILED:
            return "[failed]";
        case BT_TRANSIENT_NONE:
        default:
            return "";
    }
}

const char *bluetooth_connected_glyph(gboolean connected) {
    return connected ? "🟢" : "⚪";
}

const char *bluetooth_icon_glyph(const char *icon_name) {
    if (!icon_name) {
        return "•";
    }
    if (strcmp(icon_name, "audio-headphones") == 0 ||
        strcmp(icon_name, "audio-headset") == 0) {
        return "🎧";
    }
    if (strcmp(icon_name, "audio-card") == 0) {
        return "🔊";
    }
    if (strcmp(icon_name, "computer") == 0) {
        return "💻";
    }
    if (strcmp(icon_name, "phone") == 0) {
        return "📱";
    }
    if (strcmp(icon_name, "input-mouse") == 0) {
        return "🖱";
    }
    if (strcmp(icon_name, "input-keyboard") == 0) {
        return "⌨";
    }
    return "•";
}

gboolean bluetooth_model_show_adapter_column(AppData *app) {
    BluetoothMode *mode = bt_mode(app);
    return mode && mode->powered_adapter_count > 1;
}

gboolean bluetooth_model_has_visible_devices(AppData *app) {
    BluetoothMode *mode = bt_mode(app);
    return mode && mode->filtered_count > 0;
}

gboolean bluetooth_model_is_loading(AppData *app) {
    BluetoothMode *mode = bt_mode(app);
    return mode && mode->loading;
}

const char *bluetooth_model_status_message(AppData *app) {
    static const char *k_loading = "Loading Bluetooth devices...";
    static const char *k_unavailable = "BlueZ unavailable";
    static const char *k_empty = "No paired Bluetooth devices";
    static const char *k_no_match = "No matching Bluetooth devices";
    BluetoothMode *mode = bt_mode(app);
    if (!mode) {
        return k_unavailable;
    }
    if (bluetooth_bluez_bus_state() == BUS_FAILED) {
        return mode->status_message[0] ? mode->status_message : k_unavailable;
    }
    if (mode->loading || bluetooth_bluez_bus_state() == BUS_ACQUIRING) {
        return k_loading;
    }
    if (mode->status_message[0]) {
        return mode->status_message;
    }
    if (mode->device_count > 0) {
        return k_no_match;
    }
    return k_empty;
}

static gboolean bt_request_operation(AppData *app, int visible_idx, BtOpType op_type) {
    BtDeviceEntry *device = bt_device_at_visible(app, visible_idx);
    if (!device) {
        return FALSE;
    }
    return bt_prepare_device_operation(app, device, op_type);
}

gboolean bluetooth_model_toggle_device(AppData *app, int visible_idx) {
    BtDeviceEntry *device = bt_device_at_visible(app, visible_idx);
    if (!device) {
        return FALSE;
    }
    return bt_prepare_device_operation(app, device,
                                       device->connected ? BT_OP_DISCONNECTING
                                                         : BT_OP_CONNECTING);
}

gboolean bluetooth_model_connect_device(AppData *app, int visible_idx) {
    return bt_request_operation(app, visible_idx, BT_OP_CONNECTING);
}

gboolean bluetooth_model_disconnect_device(AppData *app, int visible_idx) {
    return bt_request_operation(app, visible_idx, BT_OP_DISCONNECTING);
}

void bluetooth_model_refresh(AppData *app) {
    BluetoothMode *mode = bt_mode(app);
    if (!mode) {
        return;
    }

    BusState bus_state = bluetooth_bluez_bus_state();
    if (bus_state == BUS_FAILED) {
        mode->loading = FALSE;
        bt_set_status_message(mode, "BlueZ unavailable");
        bt_mark_dirty_and_maybe_update(app);
        return;
    }

    if (bus_state == BUS_UNINITIALIZED) {
        mode->loading = TRUE;
        bluetooth_bluez_ensure_bus(app);
        bt_mark_dirty_and_maybe_update(app);
        return;
    }

    if (bus_state == BUS_ACQUIRING) {
        mode->loading = TRUE;
        return;
    }

    bt_start_refresh(app);
}

void bluetooth_model_apply_snapshot(AppData *app, const BtSnapshot *snapshot) {
    BluetoothMode *mode = bt_mode(app);
    if (!mode || !snapshot) {
        return;
    }

    BtDeviceEntry previous[BT_MAX_DEVICES];
    memcpy(previous, mode->devices, sizeof(previous));
    int previous_count = mode->device_count;

    gboolean preserve = ((int)app->current_tab == mode->tab_mode);
    if (preserve) {
        preserve_selection(app);
    }

    mode->device_count = 0;
    mode->powered_adapter_count = snapshot->adapter_count;
    bt_set_status_message(mode, "");

    for (int i = 0; i < snapshot->device_count && mode->device_count < BT_MAX_DEVICES; i++) {
        const BtParsedDevice *src = &snapshot->devices[i];
        const BtAdapterInfo *adapter = bt_find_adapter(snapshot, src->adapter_path);
        if (!adapter) {
            continue;
        }

        const BtDeviceEntry *prev = NULL;
        for (int j = 0; j < previous_count; j++) {
            if (strcmp(previous[j].path, src->path) == 0) {
                prev = &previous[j];
                break;
            }
        }

        bt_copy_device_entry(&mode->devices[mode->device_count], src, prev, adapter);
        mode->device_count++;
    }

    qsort(mode->devices,
          (size_t)mode->device_count,
          sizeof(mode->devices[0]),
          bt_compare_device_entries);

    bt_refilter(app);
    if (preserve) {
        restore_selection(app);
    }
    bt_mark_dirty_and_maybe_update(app);
}

gboolean bluetooth_model_refresh_generation_is_current(AppData *app, guint generation) {
    BluetoothMode *mode = bt_mode(app);
    return mode && mode->refresh_generation == generation;
}

void bluetooth_model_complete_refresh(AppData *app,
                                      guint generation,
                                      gboolean cancelled,
                                      const char *error_message) {
    BluetoothMode *mode = bt_mode(app);
    if (!mode || mode->refresh_generation != generation) {
        return;
    }

    mode->loading = FALSE;
    g_clear_object(&mode->refresh_cancel);

    if (cancelled) {
        return;
    }

    if (error_message && error_message[0]) {
        bt_set_status_message(mode, error_message);
        bt_mark_dirty_and_maybe_update(app);
    }
}

void bluetooth_model_note_bus_ready(AppData *app) {
    BluetoothMode *mode = bt_mode(app);
    if (!mode) {
        return;
    }
    bt_set_status_message(mode, "");
}

void bluetooth_model_note_bus_failed(AppData *app, const char *error_message) {
    BluetoothMode *mode = bt_mode(app);
    if (!mode) {
        return;
    }
    mode->loading = FALSE;
    g_clear_object(&mode->refresh_cancel);
    bt_set_status_message(mode,
                          (error_message && error_message[0]) ? error_message
                                                              : "BlueZ unavailable");
    bt_mark_dirty_and_maybe_update(app);
}

void bluetooth_model_complete_operation(AppData *app,
                                        BtOpContext *ctx,
                                        gboolean success,
                                        gboolean cancelled,
                                        const char *error_name,
                                        const char *error_message) {
    if (!app || !ctx) {
        return;
    }

    BtDeviceEntry *device = bt_find_device(app, ctx->device_path);
    if (!device || device->generation != ctx->generation) {
        bluetooth_op_context_unref(ctx);
        return;
    }

    ctx->completed = TRUE;
    if (device->active_op == ctx) {
        device->active_op = NULL;
    }

    if (!cancelled) {
        if (success) {
            device->connected = (ctx->op_type == BT_OP_CONNECTING);
            device->transient_state = device->connected
                ? BT_TRANSIENT_CONNECTED
                : BT_TRANSIENT_DISCONNECTED;
            device->transient_until_us = bt_now_usec() + BT_TERMINAL_STATE_USEC;
        } else {
            device->transient_state = BT_TRANSIENT_FAILED;
            device->transient_until_us = bt_now_usec() + BT_TERMINAL_STATE_USEC;
            log_warn("bluetooth: %s %s failed: %s: %s",
                     ctx->op_type == BT_OP_CONNECTING ? "connect" : "disconnect",
                     ctx->device_path,
                     error_name ? error_name : "error",
                     error_message ? error_message : "unknown error");
        }
        bt_mark_dirty_and_maybe_update(app);
    }

    bluetooth_op_context_unref(ctx);
}
