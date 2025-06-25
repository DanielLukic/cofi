#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <string.h>
#include <strings.h>
#include "app_data.h"
#include "window_list.h"
#include "x11_utils.h"
#include "log.h"
#include "utils.h"

// Get list of all windows using _NET_CLIENT_LIST
void get_window_list(AppData *app) {
    app->window_count = 0;
    
    Atom net_client_list = XInternAtom(app->display, "_NET_CLIENT_LIST", False);
    Atom net_wm_name = XInternAtom(app->display, "_NET_WM_NAME", False);
    Atom net_wm_desktop = XInternAtom(app->display, "_NET_WM_DESKTOP", False);
    
    log_debug("Getting window list...");
    log_trace("net_client_list atom = %lu", net_client_list);
    
    unsigned char *prop = NULL;
    unsigned long n_items;
    
    // Get the client list from the root window
    if (!get_x11_property(app->display, DefaultRootWindow(app->display), 
                         net_client_list, XA_WINDOW, (~0L), 
                         NULL, NULL, &n_items, &prop)) {
        log_error("Failed to get window list");
        return;
    }
    
    log_debug("Found %lu windows", n_items);
    
    Window *windows = (Window*)prop;
    
    for (unsigned long i = 0; i < n_items; i++) {
        Window window = windows[i];
        
        // Skip null window IDs (like Go code line 244)
        if (window == 0) {
            continue;
        }
        
        // Validate window exists (like Go code's isValidWindow)
        XWindowAttributes attrs;
        if (XGetWindowAttributes(app->display, window, &attrs) == 0) {
            continue; // Window doesn't exist
        }
        
        // Get window title - prefer _NET_WM_NAME, fallback to WM_NAME
        char *title = get_window_property(app->display, window, net_wm_name);
        if (!title) {
            title = get_window_property(app->display, window, XA_WM_NAME);
        }
        // Don't skip windows without titles (Go code has this commented out)
        
        log_trace("Window %lu - Title: '%s'", window, title ? title : "(no title)");
        
        // Get window class (instance and class)
        char instance[MAX_CLASS_LEN];
        char class_name[MAX_CLASS_LEN];
        get_window_class(app->display, window, instance, class_name);
        
        // Skip cofi windows
        if (strcasecmp(class_name, "cofi") == 0) {
            log_trace("Skipping cofi window: %lu (class: %s)", window, class_name);
            continue;
        }
        
        // Get window type
        char *type = get_window_type(app->display, window);
        
        // Get window PID
        int pid = get_window_pid(app->display, window);
        
        // Get desktop number
        int desktop = -1;
        unsigned char *desk_prop = NULL;
        int desk_actual_format;
        unsigned long desk_n_items;
        
        if (get_x11_property(app->display, window, net_wm_desktop, XA_CARDINAL,
                            1, NULL, &desk_actual_format, &desk_n_items, &desk_prop)) {
            desktop = *(long*)desk_prop;
            XFree(desk_prop);
        }
        
        // Store window info
        if (app->window_count < MAX_WINDOWS) {
            WindowInfo *win = &app->windows[app->window_count];
            win->id = window;
            
            // Store title
            safe_string_copy(win->title, title, MAX_TITLE_LEN);
            
            // Store instance and class
            safe_string_copy(win->instance, instance, MAX_CLASS_LEN);
            safe_string_copy(win->class_name, class_name, MAX_CLASS_LEN);
            
            // Store type
            if (type) {
                safe_string_copy(win->type, type, sizeof(win->type));
                g_free(type);
            } else {
                strcpy(win->type, "Normal");
            }
            
            win->desktop = desktop;
            win->pid = pid;
            app->window_count++;
        }
        
        if (title) g_free(title);
    }
    
    XFree(prop);
    
    log_debug("Total windows stored: %d", app->window_count);
}