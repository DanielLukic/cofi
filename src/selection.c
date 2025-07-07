#include "selection.h"
#include "log.h"
#include "display.h"

// Initialize selection state
void init_selection(AppData *app) {
    if (!app) return;
    
    app->selection.window_index = 0;
    app->selection.workspace_index = 0;
    app->selection.harpoon_index = 0;
    app->selection.selected_window_id = 0;
    app->selection.selected_workspace_id = -1;
    
    log_debug("Selection initialized");
}

// Reset selection to first item
void reset_selection(AppData *app) {
    if (!app) return;
    
    if (app->current_tab == TAB_WINDOWS) {
        app->selection.window_index = 0;
        app->selection.selected_window_id = 0;
        if (app->filtered_count > 0) {
            app->selection.selected_window_id = app->filtered[0].id;
        }
    } else if (app->current_tab == TAB_WORKSPACES) {
        app->selection.workspace_index = 0;
        app->selection.selected_workspace_id = -1;
        if (app->filtered_workspace_count > 0) {
            app->selection.selected_workspace_id = app->filtered_workspaces[0].id;
        }
    } else if (app->current_tab == TAB_HARPOON) {
        app->selection.harpoon_index = 0;
    }
    
    const char *tab_names[] = {"windows", "workspaces", "harpoon"};
    log_debug("Selection reset for %s tab", tab_names[app->current_tab]);
}

// Get currently selected window (no Alt-Tab swap confusion)
WindowInfo* get_selected_window(AppData *app) {
    if (!app || app->current_tab != TAB_WINDOWS) return NULL;
    if (app->filtered_count == 0) return NULL;
    if (app->selection.window_index < 0 || app->selection.window_index >= app->filtered_count) {
        return NULL;
    }
    
    return &app->filtered[app->selection.window_index];
}

// Get currently selected workspace
WorkspaceInfo* get_selected_workspace(AppData *app) {
    if (!app || app->current_tab != TAB_WORKSPACES) return NULL;
    if (app->filtered_workspace_count == 0) return NULL;
    if (app->selection.workspace_index < 0 || app->selection.workspace_index >= app->filtered_workspace_count) {
        return NULL;
    }
    
    return &app->filtered_workspaces[app->selection.workspace_index];
}

// Get the appropriate selected index for current tab
int get_selected_index(AppData *app) {
    if (!app) return 0;
    
    if (app->current_tab == TAB_WINDOWS) {
        return app->selection.window_index;
    } else if (app->current_tab == TAB_WORKSPACES) {
        return app->selection.workspace_index;
    } else if (app->current_tab == TAB_HARPOON) {
        return app->selection.harpoon_index;
    }
    
    return 0;
}

// Move selection up (decrements index in display, moves up visually)
void move_selection_up(AppData *app) {
    if (!app) return;
    
    if (app->current_tab == TAB_WINDOWS) {
        if (app->selection.window_index < app->filtered_count - 1) {
            app->selection.window_index++;
            if (app->filtered_count > 0) {
                app->selection.selected_window_id = app->filtered[app->selection.window_index].id;
            }
            update_display(app);
            log_info("USER: Selection UP -> Window[%d] '%s' (ID: 0x%lx)",
                     app->selection.window_index,
                     app->filtered[app->selection.window_index].title,
                     app->filtered[app->selection.window_index].id);
        }
    } else if (app->current_tab == TAB_WORKSPACES) {
        if (app->selection.workspace_index < app->filtered_workspace_count - 1) {
            app->selection.workspace_index++;
            if (app->filtered_workspace_count > 0) {
                app->selection.selected_workspace_id = app->filtered_workspaces[app->selection.workspace_index].id;
            }
            update_display(app);
            log_info("USER: Selection UP -> Workspace[%d] '%s' (ID: %d)",
                     app->selection.workspace_index,
                     app->filtered_workspaces[app->selection.workspace_index].name,
                     app->filtered_workspaces[app->selection.workspace_index].id);
        }
    } else if (app->current_tab == TAB_HARPOON) {
        if (app->selection.harpoon_index < MAX_HARPOON_SLOTS - 1) {
            app->selection.harpoon_index++;
            update_display(app);
            log_info("USER: Selection UP -> Harpoon slot %d", app->selection.harpoon_index);
        }
    }
}

// Move selection down (increments index in display, moves down visually)
void move_selection_down(AppData *app) {
    if (!app) return;
    
    if (app->current_tab == TAB_WINDOWS) {
        if (app->selection.window_index > 0) {
            app->selection.window_index--;
            if (app->filtered_count > 0) {
                app->selection.selected_window_id = app->filtered[app->selection.window_index].id;
            }
            update_display(app);
            log_info("USER: Selection DOWN -> Window[%d] '%s' (ID: 0x%lx)",
                     app->selection.window_index,
                     app->filtered[app->selection.window_index].title,
                     app->filtered[app->selection.window_index].id);
        }
    } else if (app->current_tab == TAB_WORKSPACES) {
        if (app->selection.workspace_index > 0) {
            app->selection.workspace_index--;
            if (app->filtered_workspace_count > 0) {
                app->selection.selected_workspace_id = app->filtered_workspaces[app->selection.workspace_index].id;
            }
            update_display(app);
            log_info("USER: Selection DOWN -> Workspace[%d] '%s' (ID: %d)",
                     app->selection.workspace_index,
                     app->filtered_workspaces[app->selection.workspace_index].name,
                     app->filtered_workspaces[app->selection.workspace_index].id);
        }
    } else if (app->current_tab == TAB_HARPOON) {
        if (app->selection.harpoon_index > 0) {
            app->selection.harpoon_index--;
            update_display(app);
            log_info("USER: Selection DOWN -> Harpoon slot %d", app->selection.harpoon_index);
        }
    }
}

// Preserve current selection before filtering
void preserve_selection(AppData *app) {
    if (!app) return;
    
    if (app->current_tab == TAB_WINDOWS) {
        if (app->filtered_count > 0 && app->selection.window_index >= 0 && 
            app->selection.window_index < app->filtered_count) {
            app->selection.selected_window_id = app->filtered[app->selection.window_index].id;
            log_trace("Preserved window selection: ID 0x%lx at index %d",
                      app->selection.selected_window_id, app->selection.window_index);
        }
    } else {
        if (app->filtered_workspace_count > 0 && app->selection.workspace_index >= 0 && 
            app->selection.workspace_index < app->filtered_workspace_count) {
            app->selection.selected_workspace_id = app->filtered_workspaces[app->selection.workspace_index].id;
            log_debug("Preserved workspace selection: ID %d at index %d", 
                      app->selection.selected_workspace_id, app->selection.workspace_index);
        }
    }
}

// Restore selection after filtering
void restore_selection(AppData *app) {
    if (!app) return;
    
    if (app->current_tab == TAB_WINDOWS) {
        if (app->selection.selected_window_id != 0) {
            // Try to find the previously selected window
            bool found = false;
            for (int i = 0; i < app->filtered_count; i++) {
                if (app->filtered[i].id == app->selection.selected_window_id) {
                    app->selection.window_index = i;
                    log_trace("Restored window selection to index %d for window ID 0x%lx",
                              i, app->selection.selected_window_id);
                    found = true;
                    break;
                }
            }
            if (!found) {
                // Window no longer exists, reset to first
                app->selection.window_index = 0;
                app->selection.selected_window_id = (app->filtered_count > 0) ? app->filtered[0].id : 0;
                log_debug("Previously selected window ID 0x%lx no longer exists, reset to 0", 
                          app->selection.selected_window_id);
            }
        } else {
            // No previous selection, select first window
            app->selection.window_index = 0;
            app->selection.selected_window_id = (app->filtered_count > 0) ? app->filtered[0].id : 0;
            log_debug("No previous window selection, defaulting to index 0");
        }
    } else {
        if (app->selection.selected_workspace_id != -1) {
            // Try to find the previously selected workspace
            bool found = false;
            for (int i = 0; i < app->filtered_workspace_count; i++) {
                if (app->filtered_workspaces[i].id == app->selection.selected_workspace_id) {
                    app->selection.workspace_index = i;
                    log_debug("Restored workspace selection to index %d for workspace ID %d", 
                              i, app->selection.selected_workspace_id);
                    found = true;
                    break;
                }
            }
            if (!found) {
                // Workspace no longer exists, reset to first
                app->selection.workspace_index = 0;
                app->selection.selected_workspace_id = (app->filtered_workspace_count > 0) ? 
                    app->filtered_workspaces[0].id : -1;
                log_debug("Previously selected workspace ID %d no longer exists, reset to 0", 
                          app->selection.selected_workspace_id);
            }
        } else {
            // No previous selection, select first workspace
            app->selection.workspace_index = 0;
            app->selection.selected_workspace_id = (app->filtered_workspace_count > 0) ? 
                app->filtered_workspaces[0].id : -1;
            log_debug("No previous workspace selection, defaulting to index 0");
        }
    }
}

// Validate and fix selection bounds
void validate_selection(AppData *app) {
    if (!app) return;
    
    if (app->current_tab == TAB_WINDOWS) {
        if (app->filtered_count == 0) {
            app->selection.window_index = 0;
            app->selection.selected_window_id = 0;
        } else if (app->selection.window_index >= app->filtered_count) {
            app->selection.window_index = app->filtered_count - 1;
            app->selection.selected_window_id = app->filtered[app->selection.window_index].id;
        } else if (app->selection.window_index < 0) {
            app->selection.window_index = 0;
            app->selection.selected_window_id = app->filtered[0].id;
        }
    } else {
        if (app->filtered_workspace_count == 0) {
            app->selection.workspace_index = 0;
            app->selection.selected_workspace_id = -1;
        } else if (app->selection.workspace_index >= app->filtered_workspace_count) {
            app->selection.workspace_index = app->filtered_workspace_count - 1;
            app->selection.selected_workspace_id = app->filtered_workspaces[app->selection.workspace_index].id;
        } else if (app->selection.workspace_index < 0) {
            app->selection.workspace_index = 0;
            app->selection.selected_workspace_id = app->filtered_workspaces[0].id;
        }
    }
}
