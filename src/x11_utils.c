#include "x11_utils.h"
#include "window_info.h"
#include <X11/Xatom.h>
#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "constants.h"
#include "utils.h"
#include "log.h"

// Generic X11 property getter - centralizes XGetWindowProperty pattern
// Returns COFI_SUCCESS (0) on success, COFI_ERROR_X11 on failure
CofiResult get_x11_property(Display *display, Window window, Atom property, Atom req_type,
                     unsigned long max_items, Atom *actual_type_return,
                     int *actual_format_return, unsigned long *n_items_return,
                     unsigned char **prop_return) {
    Atom actual_type;
    int actual_format;
    unsigned long n_items, bytes_after;
    unsigned char *prop = NULL;
    
    int result = XGetWindowProperty(display, window, property, 0, max_items, False,
                                   req_type, &actual_type, &actual_format,
                                   &n_items, &bytes_after, &prop);
    
    if (result == Success && prop) {
        if (actual_type_return) *actual_type_return = actual_type;
        if (actual_format_return) *actual_format_return = actual_format;
        if (n_items_return) *n_items_return = n_items;
        if (prop_return) *prop_return = prop;
        return COFI_SUCCESS;
    }
    
    if (prop) XFree(prop);
    return COFI_ERROR_X11;
}

// Get property from X11 window
char *get_window_property(Display *display, Window window, Atom property) {
    unsigned char *prop = NULL;
    
    if (get_x11_property(display, window, property, AnyPropertyType, 
                        (~0L), NULL, NULL, NULL, &prop) == COFI_SUCCESS) {
        char *result = g_strdup((char*)prop);
        XFree(prop);
        return result;
    }
    return NULL;
}

// Get window type (Normal/Special) from _NET_WM_WINDOW_TYPE
char *get_window_type(Display *display, Window window) {
    Atom net_wm_window_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom net_wm_window_type_normal = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
    
    if (net_wm_window_type == None) {
        return g_strdup("Normal"); // Default
    }
    
    Atom actual_type;
    int actual_format;
    unsigned long n_items;
    unsigned char *prop = NULL;
    
    if (get_x11_property(display, window, net_wm_window_type, XA_ATOM,
                        64, &actual_type, &actual_format, &n_items, &prop) == COFI_SUCCESS) {
        
        if (actual_format == 32 && n_items > 0) {
            // Check if contains only normal type
            Atom *atoms = (Atom*)prop;
            gboolean contains_only_normal = TRUE;
            
            for (unsigned long i = 0; i < n_items; i++) {
                if (net_wm_window_type_normal == None || atoms[i] != net_wm_window_type_normal) {
                    contains_only_normal = FALSE;
                    break;
                }
            }
            
            XFree(prop);
            return g_strdup(contains_only_normal && n_items > 0 && net_wm_window_type_normal != None ? WINDOW_TYPE_NORMAL : WINDOW_TYPE_SPECIAL);
        }
        XFree(prop);
    }
    
    return g_strdup("Normal"); // Default if property missing
}

// Get window PID from _NET_WM_PID
int get_window_pid(Display *display, Window window) {
    Atom net_wm_pid = XInternAtom(display, "_NET_WM_PID", False);
    
    if (net_wm_pid == None) {
        return 0;
    }
    
    int actual_format;
    unsigned long n_items;
    unsigned char *prop = NULL;
    
    if (get_x11_property(display, window, net_wm_pid, XA_CARDINAL,
                        1, NULL, &actual_format, &n_items, &prop) == COFI_SUCCESS) {
        
        if (actual_format == 32 && n_items >= 1) {
            int pid = *(int*)prop;
            XFree(prop);
            return pid;
        }
        XFree(prop);
    }
    
    return 0;
}

// Get window class (instance and class) from WM_CLASS
void get_window_class(Display *display, Window window, char *instance, char *class_name) {
    Atom wm_class = XInternAtom(display, "WM_CLASS", False);
    
    // Initialize to empty
    instance[0] = '\0';
    class_name[0] = '\0';
    
    if (wm_class == None) {
        return;
    }
    
    unsigned long n_items;
    unsigned char *prop = NULL;
    
    if (get_x11_property(display, window, wm_class, XA_STRING,
                        1024, NULL, NULL, &n_items, &prop) == COFI_SUCCESS) {
        
        // WM_CLASS contains two null-terminated strings: instance, class
        char *str = (char*)prop;
        int null_pos = -1;
        
        // Find first null terminator
        for (unsigned long i = 0; i < n_items; i++) {
            if (str[i] == '\0') {
                null_pos = i;
                break;
            }
        }
        
        if (null_pos >= 0 && null_pos + 1 < (int)n_items) {
            // Copy instance (first string)
            safe_string_copy(instance, str, MAX_CLASS_LEN);
            
            // Copy class (second string, after null terminator)
            safe_string_copy(class_name, str + null_pos + 1, MAX_CLASS_LEN);
            
            // Remove any trailing nulls from class_name
            int len = strlen(class_name);
            while (len > 0 && class_name[len - 1] == '\0') {
                len--;
            }
            class_name[len] = '\0';
        } else if (null_pos == -1) {
            // Malformed, treat whole thing as instance
            safe_string_copy(instance, str, MAX_CLASS_LEN);
        }
        
        XFree(prop);
    }
}

// Get currently active window ID from _NET_ACTIVE_WINDOW
int get_active_window_id(Display *display) {
    Atom net_active_window = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    
    if (net_active_window == None) {
        return 0;
    }
    
    Window root = DefaultRootWindow(display);
    int actual_format;
    unsigned long n_items;
    unsigned char *prop = NULL;
    
    if (get_x11_property(display, root, net_active_window, XA_WINDOW,
                        1, NULL, &actual_format, &n_items, &prop) == COFI_SUCCESS) {
        
        if (actual_format == 32 && n_items >= 1) {
            int window_id = *(int*)prop;
            XFree(prop);
            
            // Validate that this window exists
            XWindowAttributes attrs;
            if (XGetWindowAttributes(display, (Window)window_id, &attrs) == 0) {
                return 0; // Window doesn't exist
            }
            
            return window_id;
        }
        XFree(prop);
    }
    
    return 0;
}
// Get number of workspaces/desktops from _NET_NUMBER_OF_DESKTOPS
int get_number_of_desktops(Display *display) {
    Atom net_num_desktops = XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS", False);
    int actual_format;
    unsigned long n_items;
    unsigned char *prop = NULL;
    
    if (get_x11_property(display, DefaultRootWindow(display), net_num_desktops,
                        XA_CARDINAL, 1, NULL, &actual_format, &n_items, &prop) == COFI_SUCCESS) {
        
        if (actual_format == 32 && n_items >= 1) {
            int num_desktops = *(int*)prop;
            XFree(prop);
            return num_desktops;
        }
        XFree(prop);
    }
    
    return 1; // Default to 1 desktop if property not found
}

// Get desktop names from _NET_DESKTOP_NAMES
char** get_desktop_names(Display *display, int *count) {
    Atom net_desktop_names = XInternAtom(display, "_NET_DESKTOP_NAMES", False);
    Atom utf8_string = XInternAtom(display, "UTF8_STRING", False);
    Atom actual_type;
    int actual_format;
    unsigned long n_items;
    unsigned char *prop = NULL;
    
    *count = get_number_of_desktops(display);
    char **names = malloc(*count * sizeof(char*));
    if (!names) {
        log_error("Failed to allocate memory for desktop names");
        *count = 0;
        return NULL;
    }
    
    // Initialize with default names
    for (int i = 0; i < *count; i++) {
        names[i] = malloc(32);
        if (!names[i]) {
            log_error("Failed to allocate memory for desktop name %d", i);
            // Clean up already allocated names
            for (int j = 0; j < i; j++) {
                free(names[j]);
            }
            free(names);
            *count = 0;
            return NULL;
        }
        snprintf(names[i], 32, "Desktop %d", i);
    }
    
    if (get_x11_property(display, DefaultRootWindow(display), net_desktop_names,
                        utf8_string, (~0L), &actual_type, &actual_format, &n_items, &prop) == COFI_SUCCESS) {
        
        if (actual_type == utf8_string && actual_format == 8) {
            // Parse the null-terminated string list
            char *p = (char*)prop;
            int desktop_idx = 0;
            
            while (desktop_idx < *count && p < (char*)prop + n_items) {
                if (*p != '\0') {
                    free(names[desktop_idx]);
                    names[desktop_idx] = g_strdup(p);
                    p += strlen(p);
                }
                p++; // Skip null terminator
                desktop_idx++;
            }
        }
        XFree(prop);
    }
    
    return names;
}

// Get current desktop from _NET_CURRENT_DESKTOP
int get_current_desktop(Display *display) {
    Atom net_current_desktop = XInternAtom(display, "_NET_CURRENT_DESKTOP", False);
    int actual_format;
    unsigned long n_items;
    unsigned char *prop = NULL;
    
    if (get_x11_property(display, DefaultRootWindow(display), net_current_desktop,
                        XA_CARDINAL, 1, NULL, &actual_format, &n_items, &prop) == COFI_SUCCESS) {
        
        if (actual_format == 32 && n_items >= 1) {
            int current_desktop = *(int*)prop;
            XFree(prop);
            return current_desktop;
        }
        XFree(prop);
    }
    
    return 0; // Default to desktop 0
}

// Switch to a specific desktop using _NET_CURRENT_DESKTOP
void switch_to_desktop(Display *display, int desktop) {
    Atom net_current_desktop = XInternAtom(display, "_NET_CURRENT_DESKTOP", False);
    
    XEvent event;
    memset(&event, 0, sizeof(event));
    event.type = ClientMessage;
    event.xclient.type = ClientMessage;
    event.xclient.send_event = True;
    event.xclient.display = display;
    event.xclient.window = DefaultRootWindow(display);
    event.xclient.message_type = net_current_desktop;
    event.xclient.format = 32;
    event.xclient.data.l[0] = desktop;
    event.xclient.data.l[1] = CurrentTime;
    
    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display);
}

// Move window to specific desktop (0-based index)
void move_window_to_desktop(Display *display, Window window, int desktop_index) {
    Atom net_wm_desktop = XInternAtom(display, "_NET_WM_DESKTOP", False);
    
    if (net_wm_desktop == None) {
        log_error("_NET_WM_DESKTOP not supported by window manager");
        return;
    }
    
    // Method 1: Set property directly
    XChangeProperty(display, window, net_wm_desktop, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&desktop_index, 1);
    
    // Method 2: Send client message (more compatible)
    XEvent event;
    memset(&event, 0, sizeof(event));
    event.type = ClientMessage;
    event.xclient.type = ClientMessage;
    event.xclient.send_event = True;
    event.xclient.display = display;
    event.xclient.window = window;
    event.xclient.message_type = net_wm_desktop;
    event.xclient.format = 32;
    event.xclient.data.l[0] = desktop_index;  // 0-based for X11
    event.xclient.data.l[1] = 2; // Source indication (2 = pager/user action)
    
    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display);
    
    log_debug("Moved window %lu to desktop %d", window, desktop_index);
}

// Get window state (check if a specific state atom is set)
gboolean get_window_state(Display *display, Window window, const char *state_atom_name) {
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom state_atom = XInternAtom(display, state_atom_name, False);

    if (net_wm_state == None || state_atom == None) {
        return FALSE;
    }

    Atom actual_type;
    int actual_format;
    unsigned long n_items;
    unsigned char *prop = NULL;

    if (get_x11_property(display, window, net_wm_state, XA_ATOM,
                        64, &actual_type, &actual_format, &n_items, &prop) == COFI_SUCCESS) {

        if (actual_format == 32 && n_items > 0) {
            Atom *atoms = (Atom*)prop;
            for (unsigned long i = 0; i < n_items; i++) {
                if (atoms[i] == state_atom) {
                    XFree(prop);
                    return TRUE;
                }
            }
        }
        XFree(prop);
    }

    return FALSE;
}

// Toggle window state (add or remove a specific state atom)
void toggle_window_state(Display *display, Window window, const char *state_atom_name) {
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom state_atom = XInternAtom(display, state_atom_name, False);

    if (net_wm_state == None || state_atom == None) {
        log_error("Failed to get atoms for window state manipulation");
        return;
    }

    // Check current state
    gboolean is_set = get_window_state(display, window, state_atom_name);

    // Send client message to toggle the state
    XEvent event;
    memset(&event, 0, sizeof(event));
    event.type = ClientMessage;
    event.xclient.type = ClientMessage;
    event.xclient.send_event = True;
    event.xclient.display = display;
    event.xclient.window = window;
    event.xclient.message_type = net_wm_state;
    event.xclient.format = 32;
    event.xclient.data.l[0] = is_set ? 0 : 1; // 0 = remove, 1 = add, 2 = toggle
    event.xclient.data.l[1] = state_atom;
    event.xclient.data.l[2] = 0; // Second property (unused for single state)
    event.xclient.data.l[3] = 1; // Source indication (1 = application)
    event.xclient.data.l[4] = 0; // Unused

    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display);

    log_debug("Toggled window state %s for window %lu (was %s, now %s)",
              state_atom_name, window,
              is_set ? "set" : "unset",
              is_set ? "unset" : "set");
}

// Close window by sending WM_DELETE_WINDOW message
void close_window(Display *display, Window window) {
    Atom wm_protocols = XInternAtom(display, "WM_PROTOCOLS", False);
    Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);

    if (wm_protocols == None || wm_delete_window == None) {
        log_error("Failed to get atoms for window close");
        return;
    }

    // Check if window supports WM_DELETE_WINDOW protocol
    Atom *protocols = NULL;
    int protocol_count = 0;
    gboolean supports_delete = FALSE;

    if (XGetWMProtocols(display, window, &protocols, &protocol_count)) {
        for (int i = 0; i < protocol_count; i++) {
            if (protocols[i] == wm_delete_window) {
                supports_delete = TRUE;
                break;
            }
        }
        XFree(protocols);
    }

    if (supports_delete) {
        // Send WM_DELETE_WINDOW message
        XEvent event;
        memset(&event, 0, sizeof(event));
        event.type = ClientMessage;
        event.xclient.type = ClientMessage;
        event.xclient.send_event = True;
        event.xclient.display = display;
        event.xclient.window = window;
        event.xclient.message_type = wm_protocols;
        event.xclient.format = 32;
        event.xclient.data.l[0] = wm_delete_window;
        event.xclient.data.l[1] = CurrentTime;
        event.xclient.data.l[2] = 0;
        event.xclient.data.l[3] = 0;
        event.xclient.data.l[4] = 0;

        XSendEvent(display, window, False, NoEventMask, &event);
        XFlush(display);

        log_debug("Sent WM_DELETE_WINDOW message to window %lu", window);
    } else {
        // Fallback: forcefully destroy the window
        XDestroyWindow(display, window);
        XFlush(display);

        log_debug("Forcefully destroyed window %lu (no WM_DELETE_WINDOW support)", window);
    }
}

// Toggle maximize window (both horizontal and vertical)
void toggle_maximize_window(Display *display, Window window) {
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom net_wm_state_maximized_vert = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    Atom net_wm_state_maximized_horz = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);

    if (net_wm_state == None || net_wm_state_maximized_vert == None || net_wm_state_maximized_horz == None) {
        log_error("Failed to get atoms for window maximize");
        return;
    }

    // Check current maximization state
    gboolean is_maximized_vert = get_window_state(display, window, "_NET_WM_STATE_MAXIMIZED_VERT");
    gboolean is_maximized_horz = get_window_state(display, window, "_NET_WM_STATE_MAXIMIZED_HORZ");
    gboolean is_fully_maximized = is_maximized_vert && is_maximized_horz;

    // Send client message to toggle both maximize states
    XEvent event;
    memset(&event, 0, sizeof(event));
    event.type = ClientMessage;
    event.xclient.type = ClientMessage;
    event.xclient.send_event = True;
    event.xclient.display = display;
    event.xclient.window = window;
    event.xclient.message_type = net_wm_state;
    event.xclient.format = 32;
    event.xclient.data.l[0] = is_fully_maximized ? 0 : 1; // 0 = remove, 1 = add
    event.xclient.data.l[1] = net_wm_state_maximized_vert;
    event.xclient.data.l[2] = net_wm_state_maximized_horz;
    event.xclient.data.l[3] = 1; // Source indication (1 = application)
    event.xclient.data.l[4] = 0; // Unused

    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display);

    log_debug("Toggled maximize for window %lu (was %s, now %s)",
              window,
              is_fully_maximized ? "maximized" : "not maximized",
              is_fully_maximized ? "not maximized" : "maximized");
}

// Toggle horizontal maximize only
void toggle_maximize_horizontal(Display *display, Window window) {
    toggle_window_state(display, window, "_NET_WM_STATE_MAXIMIZED_HORZ");
}

// Toggle vertical maximize only
void toggle_maximize_vertical(Display *display, Window window) {
    toggle_window_state(display, window, "_NET_WM_STATE_MAXIMIZED_VERT");
}

// =============================================================================
// Cached versions - use pre-interned atoms for better performance
// =============================================================================

// Get window type using cached atoms
char* get_window_type_cached(Display *display, Window window, AtomCache *atoms) {
    if (atoms->net_wm_window_type == None) {
        return g_strdup("Normal"); // Default
    }
    
    Atom actual_type;
    int actual_format;
    unsigned long n_items;
    unsigned char *prop = NULL;
    
    if (get_x11_property(display, window, atoms->net_wm_window_type, XA_ATOM,
                        64, &actual_type, &actual_format, &n_items, &prop) == COFI_SUCCESS) {
        
        if (actual_format == 32 && n_items > 0) {
            // Check if contains only normal type
            Atom *window_atoms = (Atom*)prop;
            gboolean contains_only_normal = TRUE;
            
            for (unsigned long i = 0; i < n_items; i++) {
                if (atoms->net_wm_window_type_normal == None || window_atoms[i] != atoms->net_wm_window_type_normal) {
                    contains_only_normal = FALSE;
                    break;
                }
            }
            
            XFree(prop);
            return g_strdup(contains_only_normal && n_items > 0 && atoms->net_wm_window_type_normal != None ? WINDOW_TYPE_NORMAL : WINDOW_TYPE_SPECIAL);
        }
        XFree(prop);
    }
    
    return g_strdup("Normal"); // Default
}

// Get window PID using cached atoms
int get_window_pid_cached(Display *display, Window window, AtomCache *atoms) {
    if (atoms->net_wm_pid == None) {
        return 0;
    }
    
    int actual_format;
    unsigned long n_items;
    unsigned char *prop = NULL;
    
    if (get_x11_property(display, window, atoms->net_wm_pid, XA_CARDINAL,
                        1, NULL, &actual_format, &n_items, &prop) == COFI_SUCCESS) {
        
        if (actual_format == 32 && n_items >= 1) {
            int pid = *(int*)prop;
            XFree(prop);
            return pid;
        }
        XFree(prop);
    }
    
    return 0;
}

// Get window class using standard atom (no caching needed for XA_WM_CLASS)
void get_window_class_cached(Display *display, Window window, char *instance, char *class_name) {
    // XA_WM_CLASS is a standard atom, no need to intern it
    get_window_class(display, window, instance, class_name);
}
