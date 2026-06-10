#include "bluetooth/bluetooth_bluez.h"

#include <string.h>

#include "core/app/app_data.h"

enum {
    BT_GET_MANAGED_OBJECTS_TIMEOUT_MS = 5000,
    BT_CONNECT_TIMEOUT_MS = 15000,
    BT_DISCONNECT_TIMEOUT_MS = 3000,
};

typedef void (*BtBusGetImpl)(GBusType bus_type,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data);
typedef GDBusConnection *(*BtBusGetFinishImpl)(GAsyncResult *res, GError **error);
typedef void (*BtConnectionCallImpl)(GDBusConnection *connection,
                                     const gchar *bus_name,
                                     const gchar *object_path,
                                     const gchar *interface_name,
                                     const gchar *method_name,
                                     GVariant *parameters,
                                     const GVariantType *reply_type,
                                     GDBusCallFlags flags,
                                     gint timeout_msec,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data);
typedef GVariant *(*BtConnectionCallFinishImpl)(GDBusConnection *connection,
                                                GAsyncResult *res,
                                                GError **error);

typedef struct {
    AppData *app;
    guint generation;
} BtRefreshRequest;

typedef struct {
    AppData *app;
    BtOpContext *ctx;
} BtOpRequest;

static BusState s_bus_state = BUS_UNINITIALIZED;
static GDBusConnection *s_bus_connection = NULL;
static GCancellable *s_bus_get_cancel = NULL;
static BtBusGetImpl s_bus_get_impl = g_bus_get;
static BtBusGetFinishImpl s_bus_get_finish_impl = g_bus_get_finish;
static BtConnectionCallImpl s_connection_call_impl = g_dbus_connection_call;
static BtConnectionCallFinishImpl s_connection_call_finish_impl = g_dbus_connection_call_finish;

static void on_bus_ready(GObject *source_object, GAsyncResult *res, gpointer user_data);
static void on_refresh_complete(GObject *source_object, GAsyncResult *res, gpointer user_data);
static void on_device_op_complete(GObject *source_object, GAsyncResult *res, gpointer user_data);

static void clear_bus_error_name(GError *error) {
    if (!error) {
        return;
    }
    if (g_dbus_error_is_remote_error(error)) {
        g_dbus_error_strip_remote_error(error);
    }
}

static void copy_error_name(GError *error, char *out, size_t out_size) {
    gchar *remote = error ? g_dbus_error_get_remote_error(error) : NULL;
    if (remote && remote[0]) {
        g_strlcpy(out, remote, out_size);
        g_free(remote);
        return;
    }
    g_free(remote);
    g_strlcpy(out,
              (error && g_quark_to_string(error->domain))
                  ? g_quark_to_string(error->domain)
                  : "error",
              out_size);
}

static gboolean parse_adapter_info(const char *object_path,
                                   GVariant *properties,
                                   BtAdapterInfo *adapter) {
    gboolean powered = FALSE;
    const char *name = NULL;
    const char *address = NULL;

    if (!properties || !adapter) {
        return FALSE;
    }

    if (!g_variant_lookup(properties, "Powered", "b", &powered) || !powered) {
        return FALSE;
    }

    g_variant_lookup(properties, "Name", "&s", &name);
    g_variant_lookup(properties, "Address", "&s", &address);

    memset(adapter, 0, sizeof(*adapter));
    g_strlcpy(adapter->path, object_path, sizeof(adapter->path));
    g_strlcpy(adapter->name, name ? name : object_path, sizeof(adapter->name));
    g_strlcpy(adapter->address, address ? address : "", sizeof(adapter->address));
    adapter->powered = powered;
    return TRUE;
}

static gboolean parse_device_info(const char *object_path,
                                  GVariant *properties,
                                  BtParsedDevice *device) {
    gboolean paired = FALSE;
    gboolean connected = FALSE;
    const char *alias = NULL;
    const char *name = NULL;
    const char *icon = NULL;
    const char *adapter_path = NULL;

    if (!properties || !device) {
        return FALSE;
    }

    if (!g_variant_lookup(properties, "Paired", "b", &paired) || !paired) {
        return FALSE;
    }

    g_variant_lookup(properties, "Connected", "b", &connected);
    g_variant_lookup(properties, "Alias", "&s", &alias);
    g_variant_lookup(properties, "Name", "&s", &name);
    g_variant_lookup(properties, "Icon", "&s", &icon);
    g_variant_lookup(properties, "Adapter", "&o", &adapter_path);

    if (!adapter_path || adapter_path[0] == '\0') {
        return FALSE;
    }

    memset(device, 0, sizeof(*device));
    g_strlcpy(device->path, object_path, sizeof(device->path));
    g_strlcpy(device->adapter_path, adapter_path, sizeof(device->adapter_path));
    g_strlcpy(device->alias, alias ? alias : "", sizeof(device->alias));
    g_strlcpy(device->name, name ? name : "", sizeof(device->name));
    g_strlcpy(device->icon, icon ? icon : "", sizeof(device->icon));
    device->paired = paired;
    device->connected = connected;
    return TRUE;
}

static gboolean snapshot_has_adapter(const BtSnapshot *snapshot, const char *path) {
    if (!snapshot || !path) {
        return FALSE;
    }
    for (int i = 0; i < snapshot->adapter_count; i++) {
        if (strcmp(snapshot->adapters[i].path, path) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean parse_managed_objects_reply(GVariant *reply, BtSnapshot *snapshot) {
    GVariant *objects = NULL;
    GVariantIter object_iter;
    const char *object_path = NULL;
    GVariant *interfaces = NULL;
    BtParsedDevice candidates[BT_MAX_DEVICES];
    int candidate_count = 0;

    if (!reply || !snapshot) {
        return FALSE;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    g_variant_get(reply, "(@a{oa{sa{sv}}})", &objects);
    if (!objects) {
        return FALSE;
    }

    g_variant_iter_init(&object_iter, objects);
    while (g_variant_iter_next(&object_iter, "{&o@a{sa{sv}}}", &object_path, &interfaces)) {
        GVariantIter iface_iter;
        const char *iface_name = NULL;
        GVariant *properties = NULL;

        g_variant_iter_init(&iface_iter, interfaces);
        while (g_variant_iter_next(&iface_iter, "{&s@a{sv}}", &iface_name, &properties)) {
            if (strcmp(iface_name, "org.bluez.Adapter1") == 0) {
                if (snapshot->adapter_count < BT_MAX_ADAPTERS &&
                    parse_adapter_info(object_path, properties,
                                       &snapshot->adapters[snapshot->adapter_count])) {
                    snapshot->adapter_count++;
                }
            } else if (strcmp(iface_name, "org.bluez.Device1") == 0) {
                if (candidate_count < BT_MAX_DEVICES &&
                    parse_device_info(object_path, properties, &candidates[candidate_count])) {
                    candidate_count++;
                }
            }
            g_variant_unref(properties);
        }
        g_variant_unref(interfaces);
    }

    for (int i = 0; i < candidate_count && snapshot->device_count < BT_MAX_DEVICES; i++) {
        if (!snapshot_has_adapter(snapshot, candidates[i].adapter_path)) {
            continue;
        }
        snapshot->devices[snapshot->device_count++] = candidates[i];
    }

    g_variant_unref(objects);
    return TRUE;
}

BusState bluetooth_bluez_bus_state(void) {
    return s_bus_state;
}

void bluetooth_bluez_ensure_bus(AppData *app) {
    if (!app || s_bus_state == BUS_ACQUIRING || s_bus_state == BUS_READY || s_bus_state == BUS_FAILED) {
        return;
    }

    s_bus_state = BUS_ACQUIRING;
    g_clear_object(&s_bus_get_cancel);
    s_bus_get_cancel = g_cancellable_new();
    s_bus_get_impl(G_BUS_TYPE_SYSTEM, s_bus_get_cancel, on_bus_ready, app);
}

static void on_bus_ready(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    (void)source_object;
    AppData *app = (AppData *)user_data;
    GError *error = NULL;

    GDBusConnection *connection = s_bus_get_finish_impl(res, &error);
    g_clear_object(&s_bus_get_cancel);

    if (!connection) {
        const char *message = error ? error->message : "BlueZ unavailable";
        s_bus_state = BUS_FAILED;
        bluetooth_model_note_bus_failed(app, message);
        clear_bus_error_name(error);
        g_clear_error(&error);
        return;
    }

    g_clear_object(&s_bus_connection);
    s_bus_connection = connection;
    s_bus_state = BUS_READY;
    bluetooth_model_note_bus_ready(app);
    bluetooth_model_refresh(app);
}

gboolean bluetooth_bluez_request_refresh(AppData *app,
                                         guint generation,
                                         GCancellable *cancel) {
    if (!app || !s_bus_connection || s_bus_state != BUS_READY) {
        return FALSE;
    }

    BtRefreshRequest *request = g_new0(BtRefreshRequest, 1);
    request->app = app;
    request->generation = generation;

    s_connection_call_impl(s_bus_connection,
                           "org.bluez",
                           "/",
                           "org.freedesktop.DBus.ObjectManager",
                           "GetManagedObjects",
                           NULL,
                           G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
                           G_DBUS_CALL_FLAGS_NONE,
                           BT_GET_MANAGED_OBJECTS_TIMEOUT_MS,
                           cancel,
                           on_refresh_complete,
                           request);
    return TRUE;
}

static void on_refresh_complete(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    BtRefreshRequest *request = (BtRefreshRequest *)user_data;
    GError *error = NULL;
    GVariant *reply =
        s_connection_call_finish_impl(G_DBUS_CONNECTION(source_object), res, &error);

    if (!bluetooth_model_refresh_generation_is_current(request->app, request->generation)) {
        if (reply) g_variant_unref(reply);
        clear_bus_error_name(error);
        g_clear_error(&error);
        g_free(request);
        return;
    }

    if (!reply) {
        gboolean cancelled = error &&
            g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
        bluetooth_model_complete_refresh(request->app,
                                         request->generation,
                                         cancelled,
                                         cancelled ? NULL : (error ? error->message : "BlueZ refresh failed"));
        clear_bus_error_name(error);
        g_clear_error(&error);
        g_free(request);
        return;
    }

    BtSnapshot snapshot;
    gboolean parsed = parse_managed_objects_reply(reply, &snapshot);
    g_variant_unref(reply);

    if (!parsed) {
        bluetooth_model_complete_refresh(request->app,
                                         request->generation,
                                         FALSE,
                                         "Failed to parse BlueZ device list");
        g_free(request);
        return;
    }

    bluetooth_model_complete_refresh(request->app, request->generation, FALSE, NULL);
    bluetooth_model_apply_snapshot(request->app, &snapshot);
    g_free(request);
}

gboolean bluetooth_bluez_request_device_op(AppData *app, BtOpContext *ctx) {
    if (!app || !ctx || !s_bus_connection || s_bus_state != BUS_READY) {
        return FALSE;
    }

    BtOpRequest *request = g_new0(BtOpRequest, 1);
    request->app = app;
    request->ctx = bluetooth_op_context_ref(ctx);

    const char *method = ctx->op_type == BT_OP_CONNECTING ? "Connect" : "Disconnect";
    gint timeout = ctx->op_type == BT_OP_CONNECTING
        ? BT_CONNECT_TIMEOUT_MS
        : BT_DISCONNECT_TIMEOUT_MS;

    s_connection_call_impl(s_bus_connection,
                           "org.bluez",
                           ctx->device_path,
                           "org.bluez.Device1",
                           method,
                           NULL,
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           timeout,
                           ctx->cancel,
                           on_device_op_complete,
                           request);
    return TRUE;
}

static void on_device_op_complete(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    BtOpRequest *request = (BtOpRequest *)user_data;
    GError *error = NULL;
    char error_name[128];
    GVariant *reply =
        s_connection_call_finish_impl(G_DBUS_CONNECTION(source_object), res, &error);
    gboolean success = (reply != NULL);
    gboolean cancelled = error &&
        g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

    if (reply) {
        g_variant_unref(reply);
    }

    copy_error_name(error, error_name, sizeof(error_name));
    const char *message = error ? error->message : NULL;
    bluetooth_model_complete_operation(request->app,
                                       request->ctx,
                                       success,
                                       cancelled,
                                       error_name,
                                       message);

    clear_bus_error_name(error);
    g_clear_error(&error);
    g_free(request);
}

void bluetooth_bluez_shutdown(void) {
    if (s_bus_get_cancel) {
        g_cancellable_cancel(s_bus_get_cancel);
    }
    g_clear_object(&s_bus_get_cancel);
    g_clear_object(&s_bus_connection);
    s_bus_state = BUS_UNINITIALIZED;
    s_bus_get_impl = g_bus_get;
    s_bus_get_finish_impl = g_bus_get_finish;
    s_connection_call_impl = g_dbus_connection_call;
    s_connection_call_finish_impl = g_dbus_connection_call_finish;
}
