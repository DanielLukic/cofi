#ifndef ATOM_CACHE_H
#define ATOM_CACHE_H

#include <X11/Xlib.h>

typedef struct {
    // Window property atoms
    Atom net_wm_name;
    Atom net_wm_visible_name;
    Atom net_wm_window_type;
    Atom net_wm_window_type_normal;
    Atom net_wm_window_type_dialog;
    Atom net_wm_window_type_dock;
    Atom net_wm_pid;
    Atom net_wm_desktop;
    Atom net_current_desktop;
    Atom net_number_of_desktops;
    Atom net_desktop_names;
    Atom net_active_window;
    Atom net_client_list;
    Atom net_workarea;
    
    // Additional atoms
    Atom wm_state;
    Atom wm_change_state;
    Atom utf8_string;
    
    // Standard atoms (for reference)
    // WM_NAME is XA_WM_NAME
    // WM_CLASS is XA_WM_CLASS
} AtomCache;

// Initialize all atoms at once
void atom_cache_init(Display *display, AtomCache *cache);

#endif // ATOM_CACHE_H