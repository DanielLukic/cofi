#include "x11_utils.h"
#include "window_info.h"
#include <X11/Xatom.h>
#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Get property from X11 window
char *get_window_property(Display *display, Window window, Atom property) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    
    if (XGetWindowProperty(display, window, property, 0, (~0L), False, 
                          AnyPropertyType, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success) {
        if (prop) {
            char *result = g_strdup((char*)prop);
            XFree(prop);
            return result;
        }
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
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    
    if (XGetWindowProperty(display, window, net_wm_window_type, 0, 64, False,
                          XA_ATOM, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        
        if (actual_format == 32 && nitems > 0) {
            // Check if contains only normal type
            Atom *atoms = (Atom*)prop;
            gboolean contains_only_normal = TRUE;
            
            for (unsigned long i = 0; i < nitems; i++) {
                if (net_wm_window_type_normal == None || atoms[i] != net_wm_window_type_normal) {
                    contains_only_normal = FALSE;
                    break;
                }
            }
            
            XFree(prop);
            return g_strdup(contains_only_normal && nitems > 0 && net_wm_window_type_normal != None ? "Normal" : "Special");
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
    
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    
    if (XGetWindowProperty(display, window, net_wm_pid, 0, 1, False,
                          XA_CARDINAL, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        
        if (actual_format == 32 && nitems >= 1) {
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
    
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    
    if (XGetWindowProperty(display, window, wm_class, 0, 1024, False,
                          XA_STRING, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        
        // WM_CLASS contains two null-terminated strings: instance, class
        char *str = (char*)prop;
        int null_pos = -1;
        
        // Find first null terminator
        for (unsigned long i = 0; i < nitems; i++) {
            if (str[i] == '\0') {
                null_pos = i;
                break;
            }
        }
        
        if (null_pos >= 0 && null_pos + 1 < (int)nitems) {
            // Copy instance (first string)
            strncpy(instance, str, MAX_CLASS_LEN - 1);
            instance[MAX_CLASS_LEN - 1] = '\0';
            
            // Copy class (second string, after null terminator)
            strncpy(class_name, str + null_pos + 1, MAX_CLASS_LEN - 1);
            class_name[MAX_CLASS_LEN - 1] = '\0';
            
            // Remove any trailing nulls from class_name
            int len = strlen(class_name);
            while (len > 0 && class_name[len - 1] == '\0') {
                len--;
            }
            class_name[len] = '\0';
        } else if (null_pos == -1) {
            // Malformed, treat whole thing as instance
            strncpy(instance, str, MAX_CLASS_LEN - 1);
            instance[MAX_CLASS_LEN - 1] = '\0';
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
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    
    if (XGetWindowProperty(display, root, net_active_window, 0, 1, False,
                          XA_WINDOW, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        
        if (actual_format == 32 && nitems >= 1) {
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
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    
    if (XGetWindowProperty(display, DefaultRootWindow(display), net_num_desktops, 
                          0, 1, False, XA_CARDINAL, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        
        if (actual_format == 32 && nitems >= 1) {
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
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    
    *count = get_number_of_desktops(display);
    char **names = malloc(*count * sizeof(char*));
    
    // Initialize with default names
    for (int i = 0; i < *count; i++) {
        names[i] = malloc(32);
        snprintf(names[i], 32, "Desktop %d", i);
    }
    
    if (XGetWindowProperty(display, DefaultRootWindow(display), net_desktop_names,
                          0, (~0L), False, utf8_string, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        
        if (actual_type == utf8_string && actual_format == 8) {
            // Parse the null-terminated string list
            char *p = (char*)prop;
            int desktop_idx = 0;
            
            while (desktop_idx < *count && p < (char*)prop + nitems) {
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
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    
    if (XGetWindowProperty(display, DefaultRootWindow(display), net_current_desktop,
                          0, 1, False, XA_CARDINAL, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        
        if (actual_format == 32 && nitems >= 1) {
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
