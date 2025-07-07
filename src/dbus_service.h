#ifndef DBUS_SERVICE_H
#define DBUS_SERVICE_H

#include <gio/gio.h>
#include <stdbool.h>
#include "app_data.h"
#include "types.h"

// D-Bus service constants
#define COFI_DBUS_SERVICE_NAME "org.cofi.WindowManager"
#define COFI_DBUS_OBJECT_PATH "/org/cofi/WindowManager"
#define COFI_DBUS_INTERFACE_NAME "org.cofi.WindowManager"

// D-Bus service manager structure
struct DBusService {
    GDBusConnection *connection;
    guint registration_id;
    guint name_owner_id;
    AppData *app_data;
    gboolean service_registered;
};

// Set global app data pointer for D-Bus service
void dbus_service_set_app_data(AppData *app_data);

// Initialize D-Bus service (for first instance)
DBusService* dbus_service_new(AppData *app_data);

// Check if another instance is running and call its ShowWindow method
// Returns true if another instance exists and was called successfully
bool dbus_service_check_existing_and_show(const char *mode);

// Cleanup D-Bus service
void dbus_service_cleanup(DBusService *service);

// Convert ShowMode enum to string for D-Bus
const char* show_mode_to_string(ShowMode mode);

// Convert string from D-Bus to ShowMode enum
ShowMode string_to_show_mode(const char *mode_str);

#endif // DBUS_SERVICE_H
