#include <string.h>
#include <strings.h>
#include <gtk/gtk.h>
#include "app_data.h"
#include "history.h"
#include "x11_utils.h"
#include "window_info.h"
#include "log.h"

// Update history with current window list (like Go code's KeepOnly + AddNew + UpdateActiveWindow)
void update_history(AppData *app) {
    // Get current active window
    int current_active = get_active_window_id(app->display);
    
    log_trace("update_history() - current_active=0x%x, previous_active=0x%x",
            current_active, app->active_window_id);
    
    // Keep only existing windows in history (KeepOnly logic)
    WindowInfo new_history[MAX_WINDOWS];
    int new_history_count = 0;
    
    for (int i = 0; i < app->history_count; i++) {
        WindowInfo *hist_win = &app->history[i];
        
        // Find this window in current list
        for (int j = 0; j < app->window_count; j++) {
            if (app->windows[j].id == hist_win->id) {
                // Update with current window info and keep in history
                new_history[new_history_count] = app->windows[j];
                new_history_count++;
                break;
            }
        }
    }
    
    // Add new windows that aren't in history (AddNew logic)
    for (int i = 0; i < app->window_count; i++) {
        WindowInfo *win = &app->windows[i];
        gboolean found_in_history = FALSE;
        
        for (int j = 0; j < new_history_count; j++) {
            if (new_history[j].id == win->id) {
                found_in_history = TRUE;
                break;
            }
        }
        
        if (!found_in_history && new_history_count < MAX_WINDOWS) {
            new_history[new_history_count] = *win;
            new_history_count++;
        }
    }
    
    // Copy back to history
    for (int i = 0; i < new_history_count; i++) {
        app->history[i] = new_history[i];
    }
    app->history_count = new_history_count;
    
    // Update active window (move to front if changed, like Go code's UpdateActiveWindow)
    if (current_active != 0 && current_active != app->active_window_id) {
        log_debug("Active window changed, looking for window 0x%x in history", current_active);
        // Find active window in history and move to front
        for (int i = 1; i < app->history_count; i++) { // Start from 1, skip if already at front
            if (app->history[i].id == (Window)current_active) {
                // Check if this is not our own window (by class name)
                if (strcasecmp(app->history[i].class_name, "cofi") != 0) {
                    log_trace("Moving window '%s' (0x%lx) to front from position %d", 
                            app->history[i].title, app->history[i].id, i);
                    // Move to front
                    WindowInfo active_win = app->history[i];
                    for (int j = i; j > 0; j--) {
                        app->history[j] = app->history[j-1];
                    }
                    app->history[0] = active_win;
                } else {
                    log_trace("Skipping cofi window (class: %s)", app->history[i].class_name);
                }
                break;
            }
        }
        app->active_window_id = current_active;
    } else if (current_active == app->active_window_id) {
        log_trace("Active window unchanged (0x%x)", current_active);
    }
    
    log_trace("update_history() complete - history_count=%d", app->history_count);
}

// Partition windows by type and reorder (Normal first, then Special)
void partition_and_reorder(AppData *app) {
    WindowInfo normal_windows[MAX_WINDOWS];
    WindowInfo special_windows[MAX_WINDOWS];
    int normal_count = 0;
    int special_count = 0;
    
    log_trace("partition_and_reorder() - starting with %d windows", app->history_count);
    
    // Partition by type
    for (int i = 0; i < app->history_count; i++) {
        if (strcmp(app->history[i].type, "Normal") == 0) {
            if (normal_count < MAX_WINDOWS) {
                normal_windows[normal_count] = app->history[i];
                normal_count++;
            }
        } else {
            if (special_count < MAX_WINDOWS) {
                special_windows[special_count] = app->history[i];
                special_count++;
            }
        }
    }
    
    // Rebuild history with Normal windows first, then Special
    app->history_count = 0;
    for (int i = 0; i < normal_count && app->history_count < MAX_WINDOWS; i++) {
        app->history[app->history_count] = normal_windows[i];
        app->history_count++;
    }
    for (int i = 0; i < special_count && app->history_count < MAX_WINDOWS; i++) {
        app->history[app->history_count] = special_windows[i];
        app->history_count++;
    }
}