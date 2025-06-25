#include "app_data.h"
#include "x11_events.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <string.h>
#include "log.h"
#include "window_list.h"
#include "filter.h"
#include "display.h"
#include "x11_utils.h"
#include "harpoon.h"

static GIOChannel *x11_channel = NULL;
static guint x11_watch_id = 0;
static Atom atom_net_client_list = None;
static Atom atom_net_active_window = None;

void setup_x11_event_monitoring(AppData *app) {
    Display *display = app->display;
    Window root = DefaultRootWindow(display);
    
    // Get atoms we're interested in
    atom_net_client_list = XInternAtom(display, "_NET_CLIENT_LIST", False);
    atom_net_active_window = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    
    // Select events on root window
    XSelectInput(display, root, PropertyChangeMask | SubstructureNotifyMask);
    
    // Create GIOChannel for X11 connection
    int x11_fd = ConnectionNumber(display);
    x11_channel = g_io_channel_unix_new(x11_fd);
    
    // Add watch for X11 events
    x11_watch_id = g_io_add_watch(x11_channel, G_IO_IN, process_x11_events, app);
    
    log_debug("X11 event monitoring setup complete");
}

void cleanup_x11_event_monitoring(void) {
    if (x11_watch_id > 0) {
        g_source_remove(x11_watch_id);
        x11_watch_id = 0;
    }
    
    if (x11_channel) {
        g_io_channel_unref(x11_channel);
        x11_channel = NULL;
    }
    
    log_debug("X11 event monitoring cleaned up");
}

gboolean process_x11_events(GIOChannel *source, GIOCondition condition, gpointer data) {
    (void)source;  // Unused
    (void)condition;  // Unused
    
    AppData *app = (AppData *)data;
    Display *display = app->display;
    
    // Process all pending X11 events
    while (XPending(display) > 0) {
        XEvent event;
        XNextEvent(display, &event);
        handle_x11_event(app, &event);
    }
    
    // Keep the event source active
    return TRUE;
}

void handle_x11_event(AppData *app, XEvent *event) {
    switch (event->type) {
        case PropertyNotify: {
            XPropertyEvent *prop_event = &event->xproperty;
            Window root = DefaultRootWindow(app->display);
            
            // Only interested in root window property changes
            if (prop_event->window != root) {
                break;
            }
            
            // Check which property changed
            if (prop_event->atom == atom_net_client_list) {
                log_debug("_NET_CLIENT_LIST changed - updating window list");
                
                // Get new window list
                int old_count = app->window_count;
                get_window_list(app);
                log_debug("Window count changed from %d to %d", old_count, app->window_count);
                
                // Log current windows for debugging
                for (int i = 0; i < app->window_count; i++) {
                    log_debug("Current window %d: 0x%lx '%s' (%s)", 
                             i, app->windows[i].id, app->windows[i].title, app->windows[i].class_name);
                }
                
                // Check for automatic reassignments
                log_debug("Calling check_and_reassign_windows()");
                check_and_reassign_windows(&app->harpoon, app->windows, app->window_count);
                
                // Only process if window still exists
                if (app->window && app->entry) {
                    // Get current filter text
                    const char *filter_text = gtk_entry_get_text(GTK_ENTRY(app->entry));
                    
                    // Re-apply filter (this resets selection to 0)
                    filter_windows(app, filter_text);
                } else {
                    // Window destroyed, just update with empty filter
                    filter_windows(app, "");
                }
                
                // Update display only if window exists
                if (app->window) {
                    update_display(app);
                }
            }
            else if (prop_event->atom == atom_net_active_window) {
                log_debug("_NET_ACTIVE_WINDOW changed - updating active window");
                
                // Update active window ID
                Window new_active_id = get_active_window_id(app->display);
                app->active_window_id = (int)new_active_id;
                
                // We don't need to refresh the whole list, just update history
                // This will be handled by the next filter operation
            }
            break;
        }
        
        case CreateNotify: {
            XCreateWindowEvent *create_event = &event->xcreatewindow;
            log_trace("Window created: 0x%lx", create_event->window);
            // We'll get a _NET_CLIENT_LIST update for this
            break;
        }
        
        case DestroyNotify: {
            XDestroyWindowEvent *destroy_event = &event->xdestroywindow;
            log_trace("Window destroyed: 0x%lx", destroy_event->window);
            // We'll get a _NET_CLIENT_LIST update for this
            break;
        }
        
        default:
            // Ignore other events
            break;
    }
}