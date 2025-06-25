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

#define LOCK_FILE "cofi.lock"

static AppData *g_app_data = NULL;

// Forward declaration
extern void setup_application(AppData *app, int alignment);
extern void filter_windows(AppData *app, const char *filter);
extern void update_display(AppData *app);

// Forward declaration for deferred window recreation
static gboolean recreate_window_idle(gpointer data);

// Signal handler for SIGUSR1 - show window request
static void show_window_signal_handler(int sig) {
    (void)sig; // Unused parameter
    
    if (g_app_data) {
        // Defer window recreation to the GTK main loop
        g_idle_add(recreate_window_idle, NULL);
    }
}

// Forward declaration for map event handler
static gboolean on_window_map(GtkWidget *widget, GdkEvent *event, gpointer data);
static gboolean grab_focus_delayed(gpointer data);

// Deferred window recreation - runs in GTK main loop context
static gboolean recreate_window_idle(gpointer data) {
    (void)data; // Unused parameter
    
    if (g_app_data) {
        // Destroy existing window if it exists
        if (g_app_data->window) {
            gtk_widget_destroy(g_app_data->window);
            g_app_data->window = NULL;
            g_app_data->entry = NULL;
            g_app_data->textview = NULL;
            g_app_data->scrolled = NULL;
            g_app_data->textbuffer = NULL;
        }
        
        // Reset selection to first entry (will be set again by filter_windows)
        g_app_data->selected_index = 0;
        log_debug("Reset selected_index to 0 before recreating window");
        
        // Create new window with stored alignment
        setup_application(g_app_data, g_app_data->alignment);
        
        // Connect map event to grab focus after window is mapped
        g_signal_connect(g_app_data->window, "map-event", G_CALLBACK(on_window_map), NULL);
        
        // Initialize display with all windows
        filter_windows(g_app_data, "");
        
        // ALWAYS reset selection to 0 after filtering
        g_app_data->selected_index = 0;
        log_debug("Selection reset to 0 after filtering in instance recreation");
        
        update_display(g_app_data);
        
        // Make sure focus will be set on map
        gtk_window_set_focus_on_map(GTK_WINDOW(g_app_data->window), TRUE);
        
        // Show the new window
        gtk_widget_show_all(g_app_data->window);
        
        // Update our own window ID
        GdkWindow *gdk_window = gtk_widget_get_window(g_app_data->window);
        if (gdk_window) {
            g_app_data->own_window_id = GDK_WINDOW_XID(gdk_window);
            log_debug("Updated own window ID: 0x%lx", g_app_data->own_window_id);
        }
        
        // Present the window
        gtk_window_present(GTK_WINDOW(g_app_data->window));
        
        log_info("Window recreated by signal from another instance");
    }
    
    return FALSE; // Remove from idle queue
}

// Handle window map event - grab focus after window is actually mapped
static gboolean on_window_map(GtkWidget *widget, GdkEvent *event, gpointer data) {
    (void)widget;
    (void)event;
    (void)data;
    
    if (g_app_data && g_app_data->entry && g_app_data->window) {
        // Force window to be active using multiple methods
        GtkWindow *window = GTK_WINDOW(g_app_data->window);
        
        // Method 1: Present the window with timestamp
        gtk_window_present_with_time(window, GDK_CURRENT_TIME);
        
        // Method 2: Set urgency hint to grab attention
        gtk_window_set_urgency_hint(window, TRUE);
        
        // Method 3: Grab focus on the entry
        gtk_widget_grab_focus(g_app_data->entry);
        
        // Method 4: Force keyboard grab after a short delay
        g_timeout_add(50, grab_focus_delayed, NULL);
        
        log_debug("Focus grabbed after window map (multi-method approach)");
    }
    
    return FALSE; // Let signal propagate
}

// Delayed focus grab - sometimes WMs need a moment
static gboolean grab_focus_delayed(gpointer data) {
    (void)data;
    
    if (g_app_data && g_app_data->entry && g_app_data->window) {
        GtkWindow *window = GTK_WINDOW(g_app_data->window);
        
        // Clear urgency hint
        gtk_window_set_urgency_hint(window, FALSE);
        
        // Try to grab keyboard focus at X11 level
        GdkWindow *gdk_window = gtk_widget_get_window(g_app_data->window);
        if (gdk_window) {
            Display *display = GDK_WINDOW_XDISPLAY(gdk_window);
            Window xwindow = GDK_WINDOW_XID(gdk_window);
            
            // Force raise and focus at X11 level
            XRaiseWindow(display, xwindow);
            XSetInputFocus(display, xwindow, RevertToParent, CurrentTime);
            XFlush(display);
        }
        
        // Final attempt to grab GTK focus
        gtk_window_present(window);
        gtk_widget_grab_focus(g_app_data->entry);
        
        log_debug("Delayed focus grab completed");
    }
    
    return FALSE; // Remove timeout
}

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
    if (!im) return NULL;
    
    im->lock_fd = -1;
    im->pid = getpid();
    
    // Get lock file path using XDG_RUNTIME_DIR or fallback
    const char *lock_path = get_lock_file_path();
    im->lock_path = strdup(lock_path);
    
    if (!im->lock_path) {
        free(im);
        return NULL;
    }
    
    return im;
}

static bool create_lock_file(InstanceManager *im) {
    im->lock_fd = open(im->lock_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (im->lock_fd == -1) {
        log_error("Failed to create lock file: %s", strerror(errno));
        return false;
    }
    
    // Write our PID to the lock file
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", im->pid);
    
    if (write(im->lock_fd, pid_str, strlen(pid_str)) == -1) {
        log_error("Failed to write PID to lock file: %s", strerror(errno));
        close(im->lock_fd);
        unlink(im->lock_path);
        im->lock_fd = -1;
        return false;
    }
    
    return true;
}

static bool is_process_running(pid_t pid) {
    // Send signal 0 to check if process exists
    return kill(pid, 0) == 0;
}

static bool signal_existing_instance(pid_t pid) {
    // Send SIGUSR1 to existing instance to show window
    if (kill(pid, SIGUSR1) == 0) {
        log_info("Sent show signal to existing instance (PID %d)", pid);
        return true;
    } else {
        log_error("Failed to signal existing instance: %s", strerror(errno));
        return false;
    }
}

bool instance_manager_check_existing(InstanceManager *im) {
    // Try to read existing lock file
    FILE *lock_file = fopen(im->lock_path, "r");
    if (lock_file) {
        char pid_str[32];
        if (fgets(pid_str, sizeof(pid_str), lock_file)) {
            pid_t existing_pid = atoi(pid_str);
            fclose(lock_file);
            
            // Check if process is still running
            if (existing_pid != im->pid && is_process_running(existing_pid)) {
                // Try to signal existing instance
                if (signal_existing_instance(existing_pid)) {
                    return true; // Another instance exists and was signaled
                }
            }
        } else {
            fclose(lock_file);
        }
        
        // Remove stale lock file
        unlink(im->lock_path);
    }
    
    // No existing instance, create lock file
    if (!create_lock_file(im)) {
        return false;
    }
    
    return false; // This is the first instance
}

void instance_manager_setup_signal_handler(void) {
    struct sigaction sa;
    sa.sa_handler = show_window_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; // Restart interrupted system calls
    
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        log_error("Failed to setup signal handler: %s", strerror(errno));
    } else {
        log_debug("Signal handler setup for SIGUSR1");
    }
}

void instance_manager_set_app_data(void *app_data) {
    g_app_data = (AppData*)app_data;
}

void instance_manager_cleanup(InstanceManager *im) {
    if (!im) return;
    
    if (im->lock_fd != -1) {
        close(im->lock_fd);
        unlink(im->lock_path);
    }
    
    free(im->lock_path);
    free(im);
}