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
    if (app->history_count <= 2) return; // Nothing to reorder if we have 2 or fewer windows
    
    // CRITICAL: Preserve first TWO windows for Alt-Tab functionality
    WindowInfo first_window = app->history[0];
    WindowInfo second_window = app->history[1];
    int current_desktop = get_current_desktop(app->display);
    
    // Arrays for each category
    WindowInfo current_normal[MAX_WINDOWS];
    WindowInfo other_normal[MAX_WINDOWS];
    WindowInfo current_special[MAX_WINDOWS];
    WindowInfo other_special[MAX_WINDOWS];
    WindowInfo sticky_windows[MAX_WINDOWS];
    
    int current_normal_count = 0;
    int other_normal_count = 0;
    int current_special_count = 0;
    int other_special_count = 0;
    int sticky_count = 0;
    
    log_trace("partition_and_reorder() - starting with %d windows, current desktop: %d", 
              app->history_count, current_desktop);
    
    // Partition windows starting from index 2 (skip first two windows)
    for (int i = 2; i < app->history_count; i++) {
        WindowInfo *win = &app->history[i];
        
        // Check if window is sticky
        if (win->desktop == -1) {
            if (sticky_count < MAX_WINDOWS) {
                sticky_windows[sticky_count] = *win;
                sticky_count++;
            }
        }
        // Check if window is Normal type
        else if (strcmp(win->type, "Normal") == 0) {
            if (win->desktop == current_desktop) {
                if (current_normal_count < MAX_WINDOWS) {
                    current_normal[current_normal_count] = *win;
                    current_normal_count++;
                }
            } else {
                if (other_normal_count < MAX_WINDOWS) {
                    other_normal[other_normal_count] = *win;
                    other_normal_count++;
                }
            }
        }
        // Window is Special type
        else {
            if (win->desktop == current_desktop) {
                if (current_special_count < MAX_WINDOWS) {
                    current_special[current_special_count] = *win;
                    current_special_count++;
                }
            } else {
                if (other_special_count < MAX_WINDOWS) {
                    other_special[other_special_count] = *win;
                    other_special_count++;
                }
            }
        }
    }
    
    // Rebuild history in the desired order:
    // 1. First window (currently active)
    // 2. Second window (previously active - for Alt-Tab)
    // 3. Current workspace Normal windows
    // 4. Other workspace Normal windows
    // 5. Current workspace Special windows
    // 6. Other workspace Special windows
    // 7. Sticky windows
    
    app->history_count = 0;
    
    // Always preserve the first TWO windows
    app->history[app->history_count++] = first_window;
    app->history[app->history_count++] = second_window;
    
    // Add current workspace Normal windows
    for (int i = 0; i < current_normal_count && app->history_count < MAX_WINDOWS; i++) {
        app->history[app->history_count++] = current_normal[i];
    }
    
    // Add other workspace Normal windows
    for (int i = 0; i < other_normal_count && app->history_count < MAX_WINDOWS; i++) {
        app->history[app->history_count++] = other_normal[i];
    }
    
    // Add current workspace Special windows
    for (int i = 0; i < current_special_count && app->history_count < MAX_WINDOWS; i++) {
        app->history[app->history_count++] = current_special[i];
    }
    
    // Add other workspace Special windows
    for (int i = 0; i < other_special_count && app->history_count < MAX_WINDOWS; i++) {
        app->history[app->history_count++] = other_special[i];
    }
    
    // Add sticky windows at the end
    for (int i = 0; i < sticky_count && app->history_count < MAX_WINDOWS; i++) {
        app->history[app->history_count++] = sticky_windows[i];
    }
    
    log_debug("Partitioned windows - Current Normal: %d, Other Normal: %d, Current Special: %d, Other Special: %d, Sticky: %d",
              current_normal_count, other_normal_count, current_special_count, other_special_count, sticky_count);
}