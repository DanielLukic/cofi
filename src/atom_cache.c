#include "atom_cache.h"
#include "log.h"

void atom_cache_init(Display *display, AtomCache *cache) {
    log_debug("Initializing X11 atom cache");
    
    // Window property atoms
    cache->net_wm_name = XInternAtom(display, "_NET_WM_NAME", False);
    cache->net_wm_visible_name = XInternAtom(display, "_NET_WM_VISIBLE_NAME", False);
    cache->net_wm_window_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    cache->net_wm_window_type_normal = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
    cache->net_wm_window_type_dialog = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    cache->net_wm_window_type_dock = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    cache->net_wm_pid = XInternAtom(display, "_NET_WM_PID", False);
    cache->net_wm_desktop = XInternAtom(display, "_NET_WM_DESKTOP", False);
    cache->net_current_desktop = XInternAtom(display, "_NET_CURRENT_DESKTOP", False);
    cache->net_number_of_desktops = XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS", False);
    cache->net_desktop_names = XInternAtom(display, "_NET_DESKTOP_NAMES", False);
    cache->net_active_window = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    cache->net_client_list = XInternAtom(display, "_NET_CLIENT_LIST", False);
    cache->net_workarea = XInternAtom(display, "_NET_WORKAREA", False);
    
    // Additional atoms
    cache->wm_state = XInternAtom(display, "WM_STATE", False);
    cache->wm_change_state = XInternAtom(display, "WM_CHANGE_STATE", False);
    cache->utf8_string = XInternAtom(display, "UTF8_STRING", False);
    
    log_debug("Atom cache initialized with %d atoms", 18);
}