#ifndef BLUETOOTH_BLUEZ_H
#define BLUETOOTH_BLUEZ_H

#include <gio/gio.h>
#include <glib.h>

#include "bluetooth/bluetooth_model.h"

#ifndef APPDATA_TYPEDEF_DEFINED
typedef struct AppData AppData;
#define APPDATA_TYPEDEF_DEFINED
#endif

typedef enum {
    BUS_UNINITIALIZED = 0,
    BUS_ACQUIRING,
    BUS_READY,
    BUS_FAILED,
} BusState;

BusState bluetooth_bluez_bus_state(void);
void bluetooth_bluez_ensure_bus(AppData *app);
gboolean bluetooth_bluez_request_refresh(AppData *app,
                                         guint generation,
                                         GCancellable *cancel);
gboolean bluetooth_bluez_request_device_op(AppData *app, BtOpContext *ctx);
void bluetooth_bluez_shutdown(void);

#endif
