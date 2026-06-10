#ifndef BLUETOOTH_MODEL_H
#define BLUETOOTH_MODEL_H

#include <gio/gio.h>
#include <glib.h>

#ifndef APPDATA_TYPEDEF_DEFINED
typedef struct AppData AppData;
#define APPDATA_TYPEDEF_DEFINED
#endif

#define BT_MAX_ADAPTERS 8
#define BT_MAX_DEVICES 64
#define BT_PATH_LEN 256
#define BT_NAME_LEN 128
#define BT_ADDRESS_LEN 32
#define BT_ICON_LEN 64
#define BT_QUERY_LEN 256
#define BT_STATUS_LEN 256

typedef enum {
    BT_OP_NONE = 0,
    BT_OP_CONNECTING,
    BT_OP_DISCONNECTING,
} BtOpType;

typedef enum {
    BT_TRANSIENT_NONE = 0,
    BT_TRANSIENT_CONNECTED,
    BT_TRANSIENT_DISCONNECTED,
    BT_TRANSIENT_FAILED,
} BtTransientState;

typedef struct {
    char path[BT_PATH_LEN];
    char name[BT_NAME_LEN];
    char address[BT_ADDRESS_LEN];
    gboolean powered;
} BtAdapterInfo;

typedef struct {
    char path[BT_PATH_LEN];
    char adapter_path[BT_PATH_LEN];
    char name[BT_NAME_LEN];
    char alias[BT_NAME_LEN];
    char icon[BT_ICON_LEN];
    gboolean paired;
    gboolean connected;
} BtParsedDevice;

typedef struct {
    BtAdapterInfo adapters[BT_MAX_ADAPTERS];
    int adapter_count;
    BtParsedDevice devices[BT_MAX_DEVICES];
    int device_count;
} BtSnapshot;

typedef struct BtOpContext {
    int refcount;
    char device_path[BT_PATH_LEN];
    BtOpType op_type;
    int generation;
    GCancellable *cancel;
    gboolean completed;
} BtOpContext;

typedef struct {
    char path[BT_PATH_LEN];
    char adapter_path[BT_PATH_LEN];
    char alias[BT_NAME_LEN];
    char adapter_name[BT_NAME_LEN];
    char icon[BT_ICON_LEN];
    gboolean connected;
    int generation;
    BtOpContext *active_op;
    BtTransientState transient_state;
    gint64 transient_until_us;
} BtDeviceEntry;

typedef struct {
    BtDeviceEntry devices[BT_MAX_DEVICES];
    int device_count;
    int filtered_indices[BT_MAX_DEVICES];
    int filtered_count;
    int powered_adapter_count;
    gboolean loading;
    gboolean dirty;
    guint refresh_generation;
    GCancellable *refresh_cancel;
    int tab_mode;
    char query[BT_QUERY_LEN];
    char status_message[BT_STATUS_LEN];
} BluetoothMode;

static inline void init_bluetooth_mode(BluetoothMode *mode) {
    if (!mode) {
        return;
    }
    memset(mode, 0, sizeof(*mode));
    mode->tab_mode = -1;
}
void cleanup_bluetooth_mode(BluetoothMode *mode);

void bluetooth_model_on_enter(AppData *app);
void bluetooth_model_on_leave(AppData *app);
void bluetooth_model_on_tick(AppData *app, int generation);
void bluetooth_model_on_query_changed(AppData *app, const char *query);

int bluetooth_model_row_count(AppData *app);
const BtDeviceEntry *bluetooth_model_device_at_visible(AppData *app, int visible_idx);
const char *bluetooth_model_match_string(AppData *app, int visible_idx);
const char *bluetooth_model_row_identity(AppData *app, int visible_idx);
const char *bluetooth_model_device_state_text(BtDeviceEntry *device);
const char *bluetooth_connected_glyph(gboolean connected);
const char *bluetooth_icon_glyph(const char *icon_name);
gboolean bluetooth_model_show_adapter_column(AppData *app);
gboolean bluetooth_model_has_visible_devices(AppData *app);
gboolean bluetooth_model_is_loading(AppData *app);
const char *bluetooth_model_status_message(AppData *app);

gboolean bluetooth_model_toggle_device(AppData *app, int visible_idx);
gboolean bluetooth_model_connect_device(AppData *app, int visible_idx);
gboolean bluetooth_model_disconnect_device(AppData *app, int visible_idx);
void bluetooth_model_refresh(AppData *app);

void bluetooth_model_apply_snapshot(AppData *app, const BtSnapshot *snapshot);
gboolean bluetooth_model_refresh_generation_is_current(AppData *app, guint generation);
void bluetooth_model_complete_refresh(AppData *app,
                                      guint generation,
                                      gboolean cancelled,
                                      const char *error_message);
void bluetooth_model_note_bus_ready(AppData *app);
void bluetooth_model_note_bus_failed(AppData *app, const char *error_message);
void bluetooth_model_complete_operation(AppData *app,
                                        BtOpContext *ctx,
                                        gboolean success,
                                        gboolean cancelled,
                                        const char *error_name,
                                        const char *error_message);
BtOpContext *bluetooth_op_context_ref(BtOpContext *ctx);
void bluetooth_op_context_unref(BtOpContext *ctx);

#endif
