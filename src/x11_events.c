#include "app_data.h"
#include "x11_events.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <string.h>
#include "log.h"
#include "window_list.h"
#include "filter.h"
#include "display.h"
#include "x11_utils.h"
#include "harpoon.h"
#include "harpoon_config.h"
#include "named_window.h"
#include "named_window_config.h"
#include "window_highlight.h"
#include "hotkeys.h"
#include "command_mode.h"
#include "window_matcher.h"
#include "utils.h"

static GIOChannel *x11_channel = NULL;
static guint x11_watch_id = 0;
static guint workspace_switch_timer = 0;

typedef enum {
    WS_SWITCH_NONE,
    WS_SWITCH_HIGHLIGHT,  // external switch — ripple on next active window
    WS_SWITCH_SUPPRESS,   // cofi initiated — cofi handles the ripple
} WorkspaceSwitchState;

static WorkspaceSwitchState ws_switch_state = WS_SWITCH_NONE;

void set_workspace_switch_state(int suppress) {
    ws_switch_state = suppress ? WS_SWITCH_SUPPRESS : WS_SWITCH_HIGHLIGHT;
}

// Timeout fallback: if _NET_ACTIVE_WINDOW doesn't fire within 200ms of
// workspace switch, highlight whatever is active now
static gboolean workspace_switch_timeout(gpointer data) {
    AppData *app = (AppData *)data;
    workspace_switch_timer = 0;
    if (ws_switch_state == WS_SWITCH_HIGHLIGHT) {
        ws_switch_state = WS_SWITCH_NONE;
        Window active = get_active_window_id(app->display);
        if (active && active != (Window)app->own_window_id) {
            highlight_window(app, active);
        }
    } else {
        ws_switch_state = WS_SWITCH_NONE;
    }
    return FALSE;
}

// Function to update the current workspace
void update_current_workspace(AppData *app) {
    int current_desktop = get_current_desktop(app->display);
    for (int i = 0; i < app->workspace_count; i++) {
        app->workspaces[i].is_current = (i == current_desktop);
    }
}

// Track which windows we've subscribed to PropertyNotify
static Window subscribed_windows[MAX_WINDOWS];
static int subscribed_count = 0;

static int is_subscribed(Window id) {
    for (int i = 0; i < subscribed_count; i++) {
        if (subscribed_windows[i] == id) return 1;
    }
    return 0;
}

// Subscribe to PropertyNotify on all current windows (for title change detection)
static void subscribe_to_window_properties(AppData *app) {
    for (int i = 0; i < app->window_count; i++) {
        Window w = app->windows[i].id;
        if (!is_subscribed(w) && subscribed_count < MAX_WINDOWS) {
            XWindowAttributes attrs;
            if (XGetWindowAttributes(app->display, w, &attrs)) {
                XSelectInput(app->display, w, attrs.your_event_mask | PropertyChangeMask);
                subscribed_windows[subscribed_count++] = w;
                log_trace("Subscribed to PropertyNotify on 0x%lx", w);
            }
        }
    }
}

// Prune subscribed windows that no longer exist in the window list
static void prune_subscribed_windows(AppData *app) {
    int write = 0;
    for (int i = 0; i < subscribed_count; i++) {
        int found = 0;
        for (int j = 0; j < app->window_count; j++) {
            if (app->windows[j].id == subscribed_windows[i]) {
                found = 1;
                break;
            }
        }
        if (found) {
            subscribed_windows[write++] = subscribed_windows[i];
        } else {
            rule_state_remove_window(&app->rule_state, subscribed_windows[i]);
        }
    }
    subscribed_count = write;
}

// Apply rules to all windows (checks state machine — only fires on transitions)
static void apply_rules_to_windows(AppData *app) {
    if (app->rules_config.count == 0) return;

    for (int i = 0; i < app->window_count; i++) {
        WindowInfo *w = &app->windows[i];
        for (int r = 0; r < app->rules_config.count; r++) {
            RuleMatch match = check_rule_match(
                &app->rules_config.rules[r], &app->rule_state, w->id, w->title);
            if (match.should_fire) {
                log_info("RULE: '%s' matched window 0x%lx '%s' — executing: %s",
                         app->rules_config.rules[r].pattern, w->id, w->title, match.commands);
                execute_command_background(match.commands, app, w);
            }
        }
    }
}

// Handle title change on a specific window
static void handle_window_title_change(AppData *app, Window id) {
    if (app->rules_config.count == 0) return;

    // Find the window in our list
    WindowInfo *w = NULL;
    for (int i = 0; i < app->window_count; i++) {
        if (app->windows[i].id == id) {
            w = &app->windows[i];
            break;
        }
    }
    if (!w) return;

    // Re-fetch its title using same approach as window_list.c
    char *new_title = get_window_property(app->display, id, app->atoms.net_wm_name);
    if (!new_title) {
        new_title = get_window_property(app->display, id, XA_WM_NAME);
    }
    if (!new_title) return;

    if (strcmp(w->title, new_title) != 0) {
        log_trace("Title changed for 0x%lx: '%s' -> '%s'", id, w->title, new_title);
        safe_string_copy(w->title, new_title, MAX_TITLE_LEN);

        // Check rules against updated title
        for (int r = 0; r < app->rules_config.count; r++) {
            RuleMatch match = check_rule_match(
                &app->rules_config.rules[r], &app->rule_state, id, w->title);
            if (match.should_fire) {
                log_info("RULE: '%s' matched window 0x%lx '%s' — executing: %s",
                         app->rules_config.rules[r].pattern, id, w->title, match.commands);
                execute_command_background(match.commands, app, w);
            }
        }
    }
    g_free(new_title);
}

void setup_x11_event_monitoring(AppData *app) {
    Display *display = app->display;
    Window root = DefaultRootWindow(display);
    
    // Select events on root window
    XSelectInput(display, root, PropertyChangeMask | SubstructureNotifyMask);
    
    // Create GIOChannel for X11 connection
    int x11_fd = ConnectionNumber(display);
    x11_channel = g_io_channel_unix_new(x11_fd);
    
    // Add watch for X11 events
    x11_watch_id = g_io_add_watch(x11_channel, G_IO_IN, process_x11_events, app);

    // Subscribe to property changes on existing windows (for title change rules)
    subscribe_to_window_properties(app);
    apply_rules_to_windows(app);

    log_debug("X11 event monitoring setup complete");
}

void cleanup_x11_event_monitoring(void) {
    if (x11_watch_id > 0) {
        g_source_remove(x11_watch_id);
        x11_watch_id = 0;
    }
    
    if (x11_channel) {
        g_io_channel_unref(x11_channel);
        x11_channel = NULL;
    }
    
    log_debug("X11 event monitoring cleaned up");
}

gboolean process_x11_events(GIOChannel *source, GIOCondition condition, gpointer data) {
    (void)source;  // Unused
    (void)condition;  // Unused
    
    AppData *app = (AppData *)data;
    Display *display = app->display;
    
    // Process all pending X11 events
    while (XPending(display) > 0) {
        XEvent event;
        XNextEvent(display, &event);
        handle_x11_event(app, &event);
    }
    
    // Keep the event source active
    return TRUE;
}

void handle_x11_event(AppData *app, XEvent *event) {
    switch (event->type) {
        case PropertyNotify: {
            XPropertyEvent *prop_event = &event->xproperty;
            Window root = DefaultRootWindow(app->display);

            // Handle per-window title changes (for rules)
            if (prop_event->window != root) {
                if (prop_event->atom == app->atoms.net_wm_name ||
                    prop_event->atom == XA_WM_NAME) {
                    handle_window_title_change(app, prop_event->window);
                }
                break;
            }

            // Check which root window property changed
            if (prop_event->atom == app->atoms.net_client_list) {
                log_debug("_NET_CLIENT_LIST changed - updating window list");
                
                // Get new window list
                int old_count = app->window_count;
                get_window_list(app);
                log_trace("Window count changed from %d to %d", old_count, app->window_count);

                // Log current windows for debugging
                for (int i = 0; i < app->window_count; i++) {
                    log_trace("Current window %d: 0x%lx '%s' (%s)",
                             i, app->windows[i].id, app->windows[i].title, app->windows[i].class_name);
                }

                // Check for automatic reassignments
                log_trace("Calling check_and_reassign_windows()");
                bool harpoon_changed = check_and_reassign_windows(&app->harpoon, app->windows, app->window_count);
                if (harpoon_changed) {
                    save_harpoon_slots(&app->harpoon);
                    log_debug("Saved reassigned harpoon slots after window list change");
                }
                
                // Check for named windows reassignments
                log_trace("Calling check_and_reassign_names()");
                bool names_changed = check_and_reassign_names(&app->names, app->windows, app->window_count);
                if (names_changed) {
                    save_named_windows(&app->names);
                    log_debug("Saved reassigned named windows after window list change");
                }

                // Subscribe to per-window property changes and apply rules
                prune_subscribed_windows(app);
                subscribe_to_window_properties(app);
                apply_rules_to_windows(app);

                // Only process if window still exists and is valid
                if (app->window && GTK_IS_WIDGET(app->window) &&
                    app->entry && GTK_IS_ENTRY(app->entry)) {
                    // Skip filtering when in command mode
                    if (app->command_mode.state == CMD_MODE_COMMAND) {
                        // In command mode, don't apply entry text as filter
                        filter_windows(app, "");
                    } else {
                        // Get current filter text
                        const char *filter_text = gtk_entry_get_text(GTK_ENTRY(app->entry));

                        // Re-apply filter
                        filter_windows(app, filter_text);
                    }
                } else {
                    // Window destroyed or invalid, just update with empty filter
                    filter_windows(app, "");
                }
                
                // Update display only if window exists and is visible
                if (app->window && GTK_IS_WIDGET(app->window) && 
                    gtk_widget_get_visible(app->window)) {
                    update_display(app);
                }
            }
            else if (prop_event->atom == app->atoms.net_active_window) {
                log_trace("_NET_ACTIVE_WINDOW changed - updating active window");

                // Update active window ID
                Window new_active_id = get_active_window_id(app->display);
                app->active_window_id = (int)new_active_id;

                // Highlight active window after workspace switch
                if (ws_switch_state != WS_SWITCH_NONE && new_active_id &&
                    new_active_id != (Window)app->own_window_id) {
                    WorkspaceSwitchState state = ws_switch_state;
                    ws_switch_state = WS_SWITCH_NONE;
                    if (workspace_switch_timer > 0) {
                        g_source_remove(workspace_switch_timer);
                        workspace_switch_timer = 0;
                    }
                    if (state == WS_SWITCH_HIGHLIGHT) {
                        highlight_window(app, new_active_id);
                    }
                    // WS_SWITCH_SUPPRESS: cofi already called highlight_window
                }

                // We don't need to refresh the whole list, just update history
                // This will be handled by the next filter operation
            }
            else if (prop_event->atom == app->atoms.net_current_desktop) {
                log_debug("_NET_CURRENT_DESKTOP changed - updating current workspace");
                // Only cancel ripple on external workspace switches — cofi-initiated
                // switches (WS_SWITCH_SUPPRESS) already have a fresh ripple in flight
                if (ws_switch_state != WS_SWITCH_SUPPRESS) {
                    destroy_highlight(app);
                }
                update_current_workspace(app);

                // Set flag for highlight on next active window change
                if (ws_switch_state != WS_SWITCH_SUPPRESS) {
                    ws_switch_state = WS_SWITCH_HIGHLIGHT;
                }
                if (workspace_switch_timer > 0) {
                    g_source_remove(workspace_switch_timer);
                }
                workspace_switch_timer = g_timeout_add(200, workspace_switch_timeout, app);

                if (app->window) {
                    update_display(app);
                }
            }
            break;
        }
        
        case CreateNotify: {
            XCreateWindowEvent *create_event = &event->xcreatewindow;
            log_trace("Window created: 0x%lx", create_event->window);
            // We'll get a _NET_CLIENT_LIST update for this
            break;
        }
        
        case DestroyNotify: {
            XDestroyWindowEvent *destroy_event = &event->xdestroywindow;
            log_trace("Window destroyed: 0x%lx", destroy_event->window);
            // We'll get a _NET_CLIENT_LIST update for this
            break;
        }
        
        case KeyPress:
            handle_hotkey_event(app, &event->xkey);
            break;

        default:
            // Ignore other events
            break;
    }
}