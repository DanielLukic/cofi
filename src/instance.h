#ifndef INSTANCE_H
#define INSTANCE_H

#include <stdbool.h>
#include <sys/types.h>

// Show mode for instance signaling
typedef enum {
    SHOW_MODE_WINDOWS,      // Show windows tab (SIGUSR1)
    SHOW_MODE_WORKSPACES,   // Show workspaces tab (SIGUSR2)
    SHOW_MODE_COMMAND       // Show in command mode (SIGWINCH)
} ShowMode;

// Instance manager for single instance enforcement using signals
typedef struct {
    char *lock_path;
    int lock_fd;
    pid_t pid;
} InstanceManager;

// Initialize instance manager
InstanceManager* instance_manager_new(void);

// Check if another instance is running and signal it to show
// Returns true if another instance exists and was signaled
// show_workspaces: if true, signal to show workspaces tab; otherwise show windows tab
bool instance_manager_check_existing(InstanceManager *im, bool show_workspaces);

// Check if another instance is running and signal it with the specified mode
// Returns true if another instance exists and was signaled
bool instance_manager_check_existing_with_mode(InstanceManager *im, ShowMode mode);

// Setup signal handler for receiving show requests from other instances
void instance_manager_setup_signal_handler(void);

// Set the global app data pointer for showing window
void instance_manager_set_app_data(void *app_data);

// Cleanup instance manager
void instance_manager_cleanup(InstanceManager *im);

#endif // INSTANCE_H