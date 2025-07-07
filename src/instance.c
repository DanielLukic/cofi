#include "instance.h"
#include "app_data.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <limits.h>
#include "selection.h"
#include "command_mode.h"
#include "dbus_service.h"

#define LOCK_FILE "cofi.lock"

static AppData *g_app_data = NULL;

// Forward declaration
extern void setup_application(AppData *app, int alignment);
extern void filter_windows(AppData *app, const char *filter);
extern void update_display(AppData *app);















// Get the lock file path using XDG_RUNTIME_DIR with fallback
static const char* get_lock_file_path(void) {
    static char path[PATH_MAX];
    const char* runtime_dir = getenv("XDG_RUNTIME_DIR");
    
    if (runtime_dir && access(runtime_dir, W_OK) == 0) {
        // Preferred: Use XDG_RUNTIME_DIR
        snprintf(path, sizeof(path), "%s/%s", runtime_dir, LOCK_FILE);
        log_debug("Using XDG_RUNTIME_DIR for lock file: %s", path);
    } else {
        // Fallback: Use /tmp with user-specific name
        snprintf(path, sizeof(path), "/tmp/%s-%d", LOCK_FILE, getuid());
        log_debug("Using /tmp fallback for lock file: %s", path);
    }
    
    return path;
}

InstanceManager* instance_manager_new(void) {
    InstanceManager *im = malloc(sizeof(InstanceManager));
    if (!im) {
        log_error("Failed to allocate memory for InstanceManager");
        return NULL;
    }

    im->lock_fd = -1;
    im->pid = getpid();
    im->dbus_service = NULL;

    // Get lock file path using XDG_RUNTIME_DIR or fallback (keep for backward compatibility)
    const char *lock_path = get_lock_file_path();
    im->lock_path = strdup(lock_path);

    if (!im->lock_path) {
        log_error("Failed to allocate memory for lock path");
        free(im);
        return NULL;
    }

    return im;
}

static bool create_lock_file(InstanceManager *im) {
    im->lock_fd = open(im->lock_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (im->lock_fd == -1) {
        log_error("Failed to create lock file %s: %s", im->lock_path, strerror(errno));
        return false;
    }
    
    // Write our PID to the lock file
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", im->pid);
    
    ssize_t written = write(im->lock_fd, pid_str, strlen(pid_str));
    if (written == -1) {
        log_error("Failed to write PID to lock file: %s", strerror(errno));
        close(im->lock_fd);
        im->lock_fd = -1;
        unlink(im->lock_path);
        return false;
    }
    
    // Ensure data is written to disk
    if (fsync(im->lock_fd) == -1) {
        log_warn("Failed to sync lock file to disk: %s", strerror(errno));
    }
    
    return true;
}







bool instance_manager_check_existing_with_mode(InstanceManager *im, ShowMode mode) {
    // Try to call existing D-Bus service first
    const char *mode_str = show_mode_to_string(mode);
    if (dbus_service_check_existing_and_show(mode_str)) {
        log_info("Found existing instance via D-Bus, called ShowWindow(%s)", mode_str);
        return true; // Another instance exists and was called successfully
    }

    log_debug("No existing D-Bus service found, this will be the first instance");

    // No existing instance found via D-Bus, so we'll be the first instance
    // Still create lock file for legacy compatibility
    if (!create_lock_file(im)) {
        log_warn("Failed to create lock file, but continuing with D-Bus service");
    }

    return false; // This is the first instance
}

void instance_manager_setup_dbus_service(InstanceManager *im) {
    if (!im) {
        log_error("Cannot setup D-Bus service: InstanceManager is NULL");
        return;
    }

    // Initialize D-Bus service for this instance
    im->dbus_service = dbus_service_new(g_app_data);
    if (!im->dbus_service) {
        log_error("Failed to initialize D-Bus service");
        return;
    }

    log_info("D-Bus service setup completed");
}

void instance_manager_set_app_data(void *app_data) {
    g_app_data = (AppData*)app_data;
}

void instance_manager_cleanup(InstanceManager *im) {
    if (!im) return;

    // Cleanup D-Bus service
    if (im->dbus_service) {
        dbus_service_cleanup(im->dbus_service);
        im->dbus_service = NULL;
    }

    if (im->lock_fd != -1) {
        close(im->lock_fd);
        im->lock_fd = -1;
    }

    if (im->lock_path) {
        // Only unlink if we own the lock (check PID matches)
        FILE *lock_file = fopen(im->lock_path, "r");
        if (lock_file) {
            char pid_str[32];
            if (fgets(pid_str, sizeof(pid_str), lock_file)) {
                pid_t lock_pid = atoi(pid_str);
                if (lock_pid == im->pid) {
                    unlink(im->lock_path);
                    log_debug("Removed lock file for PID %d", im->pid);
                }
            }
            fclose(lock_file);
        }

        free(im->lock_path);
        im->lock_path = NULL;
    }

    free(im);
}