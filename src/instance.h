#ifndef INSTANCE_H
#define INSTANCE_H

#include <stdbool.h>
#include <sys/types.h>
#include "types.h"

// Instance manager for single instance enforcement using D-Bus
typedef struct {
    char *lock_path;        // Legacy lock file (unused, kept for cleanup)
    int lock_fd;            // Legacy lock file descriptor (unused, kept for cleanup)
    pid_t pid;
    DBusService *dbus_service;  // D-Bus service for first instance
} InstanceManager;

// Initialize instance manager
InstanceManager* instance_manager_new(void);



// Check if another instance is running and call it with the specified mode via D-Bus
// Returns true if another instance exists and was called successfully
bool instance_manager_check_existing_with_mode(InstanceManager *im, ShowMode mode);

// Setup D-Bus service for receiving show requests from other instances
void instance_manager_setup_dbus_service(InstanceManager *im);

// Set the global app data pointer for showing window
void instance_manager_set_app_data(void *app_data);

// Cleanup instance manager
void instance_manager_cleanup(InstanceManager *im);

#endif // INSTANCE_H