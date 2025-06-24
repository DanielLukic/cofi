#include "x11_utils.h"
#include "window_info.h"
#include <X11/Xatom.h>
#include <glib.h>
#include <string.h>
#include <stdlib.h>

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