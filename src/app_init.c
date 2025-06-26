#include <stdlib.h>
#include <X11/Xlib.h>
#include "app_init.h"
#include "window_list.h"
#include "workspace_info.h"
#include "harpoon.h"
#include "filter.h"
#include "log.h"
#include "utils.h"
#include "x11_utils.h"

void init_app_data(AppData *app) {
    // Initialize history and active window tracking
    app->history_count = 0;
    app->active_window_id = -1; // Use -1 to force initial active window to be moved to front
    
    // Initialize tab mode
    app->current_tab = TAB_WINDOWS;
    app->selected_workspace_index = 0;
    
    // Initialize harpoon manager
    init_harpoon_manager(&app->harpoon);
    
    // Initialize saved position data
    app->has_saved_position = 0;
    app->saved_x = 0;
    app->saved_y = 0;
}

void init_x11_connection(AppData *app) {
    // Open X11 display
    app->display = XOpenDisplay(NULL);
    if (!app->display) {
        log_error("Cannot open X11 display");
        exit(1);
    }
    
    log_debug("X11 display opened successfully");
}

void init_workspaces(AppData *app) {
    // Get workspace list
    int num_desktops = get_number_of_desktops(app->display);
    int current_desktop = get_current_desktop(app->display);
    int desktop_count = 0;
    char** desktop_names = get_desktop_names(app->display, &desktop_count);
    
    app->workspace_count = (num_desktops < MAX_WORKSPACES) ? num_desktops : MAX_WORKSPACES;
    for (int i = 0; i < app->workspace_count; i++) {
        app->workspaces[i].id = i;
        safe_string_copy(app->workspaces[i].name, desktop_names[i], MAX_WORKSPACE_NAME_LEN);
        app->workspaces[i].is_current = (i == current_desktop);
        app->filtered_workspaces[i] = app->workspaces[i];
    }
    app->filtered_workspace_count = app->workspace_count;
    
    // Free desktop names
    for (int i = 0; i < desktop_count; i++) {
        free(desktop_names[i]);
    }
    free(desktop_names);
    
    log_debug("Found %d workspaces, current workspace: %d", app->workspace_count, current_desktop);
}

void init_window_list(AppData *app) {
    // Get window list
    get_window_list(app);
    
    // Check for automatic reassignments after loading config and getting window list
    check_and_reassign_windows(&app->harpoon, app->windows, app->window_count);
}

void init_history_from_windows(AppData *app) {
    // Initialize history with current windows
    for (int i = 0; i < app->window_count && i < MAX_WINDOWS; i++) {
        app->history[i] = app->windows[i];
    }
    app->history_count = app->window_count;
    
    // Initialize filtered list with all windows (this will process history)
    filter_windows(app, "");
    
    log_trace("First 3 windows in history after filter:");
    for (int i = 0; i < 3 && i < app->history_count; i++) {
        log_trace("  [%d] %s (0x%lx)", i, app->history[i].title, app->history[i].id);
    }
}