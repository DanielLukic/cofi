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

static int g_bus_get_calls;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

void bluetooth_model_note_bus_ready(AppData *app) {
    (void)app;
}

void bluetooth_model_note_bus_failed(AppData *app, const char *error_message) {
    (void)app;
    (void)error_message;
}

void bluetooth_model_refresh(AppData *app) {
    (void)app;
}

void bluetooth_model_apply_snapshot(AppData *app, const BtSnapshot *snapshot) {
    (void)app;
    (void)snapshot;
}

void bluetooth_model_complete_refresh(AppData *app,
                                      guint generation,
                                      gboolean cancelled,
                                      const char *error_message) {
    (void)app;
    (void)generation;
    (void)cancelled;
    (void)error_message;
}

gboolean bluetooth_model_refresh_generation_is_current(AppData *app, guint generation) {
    (void)app;
    (void)generation;
    return TRUE;
}

BtOpContext *bluetooth_op_context_ref(BtOpContext *ctx) {
    return ctx;
}

void bluetooth_op_context_unref(BtOpContext *ctx) {
    (void)ctx;
}

void bluetooth_model_complete_operation(AppData *app,
                                        BtOpContext *ctx,
                                        gboolean success,
                                        gboolean cancelled,
                                        const char *error_name,
                                        const char *error_message) {
    (void)app;
    (void)ctx;
    (void)success;
    (void)cancelled;
    (void)error_name;
    (void)error_message;
}

static void fake_bus_get(GBusType bus_type,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data) {
    (void)bus_type;
    (void)cancellable;
    (void)callback;
    (void)user_data;
    g_bus_get_calls++;
}

static GDBusConnection *fake_bus_get_finish(GAsyncResult *res, GError **error) {
    (void)res;
    (void)error;
    return NULL;
}

static void fake_connection_call(GDBusConnection *connection,
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
                                 gpointer user_data) {
    (void)connection;
    (void)bus_name;
    (void)object_path;
    (void)interface_name;
    (void)method_name;
    (void)parameters;
    (void)reply_type;
    (void)flags;
    (void)timeout_msec;
    (void)cancellable;
    (void)callback;
    (void)user_data;
}

static GVariant *fake_connection_call_finish(GDBusConnection *connection,
                                             GAsyncResult *res,
                                             GError **error) {
    (void)connection;
    (void)res;
    (void)error;
    return NULL;
}

#include "bluetooth/bluetooth_bluez.c"

static void reset_state(void) {
    g_bus_get_calls = 0;
    s_bus_state = BUS_UNINITIALIZED;
}

static GVariant *build_managed_objects_reply(void) {
    GVariantBuilder objects;
    GVariantBuilder object_ifaces;
    GVariantBuilder adapter_props;
    GVariantBuilder device_props;
    GVariantBuilder device_ifaces;

    g_variant_builder_init(&objects, G_VARIANT_TYPE("a{oa{sa{sv}}}"));

    g_variant_builder_init(&adapter_props, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&adapter_props, "{sv}", "Name", g_variant_new_string("hci0"));
    g_variant_builder_add(&adapter_props, "{sv}", "Address",
                          g_variant_new_string("AA:BB:CC:DD:EE:FF"));
    g_variant_builder_add(&adapter_props, "{sv}", "Powered", g_variant_new_boolean(TRUE));
    g_variant_builder_init(&object_ifaces, G_VARIANT_TYPE("a{sa{sv}}"));
    g_variant_builder_add(&object_ifaces, "{sa{sv}}", "org.bluez.Adapter1", &adapter_props);
    g_variant_builder_add(&objects, "{oa{sa{sv}}}", "/org/bluez/hci0", &object_ifaces);

    g_variant_builder_init(&device_props, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&device_props, "{sv}", "Paired", g_variant_new_boolean(TRUE));
    g_variant_builder_add(&device_props, "{sv}", "Connected", g_variant_new_boolean(FALSE));
    g_variant_builder_add(&device_props, "{sv}", "Alias", g_variant_new_string("WH-1000XM4"));
    g_variant_builder_add(&device_props, "{sv}", "Name", g_variant_new_string("Sony Headphones"));
    g_variant_builder_add(&device_props, "{sv}", "Icon", g_variant_new_string("audio-headphones"));
    g_variant_builder_add(&device_props, "{sv}", "Adapter",
                          g_variant_new_object_path("/org/bluez/hci0"));
    g_variant_builder_init(&device_ifaces, G_VARIANT_TYPE("a{sa{sv}}"));
    g_variant_builder_add(&device_ifaces, "{sa{sv}}", "org.bluez.Device1", &device_props);
    g_variant_builder_add(&objects, "{oa{sa{sv}}}",
                          "/org/bluez/hci0/dev_11_22_33_44_55_66", &device_ifaces);

    return g_variant_new("(a{oa{sa{sv}}})", &objects);
}

static void test_parse_managed_objects_reply(void) {
    BtSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    GVariant *reply = build_managed_objects_reply();

    ASSERT_TRUE("managed objects parse succeeds",
                parse_managed_objects_reply(reply, &snapshot) == TRUE);
    ASSERT_TRUE("powered adapter parsed", snapshot.adapter_count == 1);
    ASSERT_TRUE("paired device parsed", snapshot.device_count == 1);
    ASSERT_TRUE("device alias parsed",
                strcmp(snapshot.devices[0].alias, "WH-1000XM4") == 0);
    ASSERT_TRUE("device adapter path parsed",
                strcmp(snapshot.devices[0].adapter_path, "/org/bluez/hci0") == 0);

    g_variant_unref(reply);
}

static void test_second_ensure_during_acquiring_skips_duplicate_bus_get(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    reset_state();

    s_bus_get_impl = fake_bus_get;
    s_bus_get_finish_impl = fake_bus_get_finish;
    s_connection_call_impl = fake_connection_call;
    s_connection_call_finish_impl = fake_connection_call_finish;

    bluetooth_bluez_ensure_bus(&app);
    bluetooth_bluez_ensure_bus(&app);

    ASSERT_TRUE("ensure bus only calls g_bus_get once while acquiring", g_bus_get_calls == 1);
    ASSERT_TRUE("bus state stays acquiring", s_bus_state == BUS_ACQUIRING);
}

int main(void) {
    printf("bluetooth bluez tests\n");
    printf("=====================\n\n");

    test_parse_managed_objects_reply();
    test_second_ensure_during_acquiring_skips_duplicate_bus_get();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_run == tests_passed ? 0 : 1;
}
