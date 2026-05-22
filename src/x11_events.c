#include "app_data.h"
#include "x11_events.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#ifdef COFI_DEBUG_PRINTSCR_CAPTURE
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XInput2.h>
#endif
#include <gdk/gdkx.h>
#include <string.h>
#include "log.h"
#include "window_list.h"
#include "filter.h"
#include "display.h"
#include "x11_utils.h"
#include "harpoon.h"
#include "harpoon_config.h"
#include "match_entry.h"
#include "match_entry_config.h"
#include "window_highlight.h"
#include "hotkeys.h"
#include "command_api.h"
#include "window_matcher.h"
#include "utils.h"

static GIOChannel *x11_channel = NULL;
static guint x11_watch_id = 0;
static guint workspace_switch_timer = 0;
#ifdef COFI_DEBUG_PRINTSCR_CAPTURE
static int debug_xi_opcode = -1;
static gint64 debug_last_printscr_seen_us = 0;
#endif

typedef enum {
    WS_SWITCH_NONE,
    WS_SWITCH_HIGHLIGHT,  // external switch — ripple on next active window
    WS_SWITCH_SUPPRESS,   // cofi initiated — cofi handles the ripple
} WorkspaceSwitchState;

static WorkspaceSwitchState ws_switch_state = WS_SWITCH_NONE;

#ifdef COFI_DEBUG_PRINTSCR_CAPTURE
static void debug_note_printscr(AppData *app) {
    if (!app || !app->window_visible) {
        return;
    }

    gint64 now_us = g_get_monotonic_time();
    if (now_us - debug_last_printscr_seen_us < 500000) {
        return;
    }
    debug_last_printscr_seen_us = now_us;
    app->debug_printscr_keep_visible_until_us = now_us + 2000000;
    log_info("Debug PrintScr observer: preserving cofi focus/modal state for 2s");
}

static void setup_debug_printscr_capture(AppData *app) {
    int event = 0;
    int error = 0;

    if (!XQueryExtension(app->display, "XInputExtension", &debug_xi_opcode, &event, &error)) {
        log_warn("Debug PrintScr observer disabled: XInput2 extension unavailable");
        debug_xi_opcode = -1;
        return;
    }

    int major = 2;
    int minor = 0;
    if (XIQueryVersion(app->display, &major, &minor) != Success) {
        log_warn("Debug PrintScr observer disabled: XI2 query failed");
        debug_xi_opcode = -1;
        return;
    }

    unsigned char mask[XIMaskLen(XI_RawKeyPress)];
    memset(mask, 0, sizeof(mask));
    XISetMask(mask, XI_RawKeyPress);

    XIEventMask event_mask;
    event_mask.deviceid = XIAllMasterDevices;
    event_mask.mask_len = sizeof(mask);
    event_mask.mask = mask;

    Window root = DefaultRootWindow(app->display);
    if (XISelectEvents(app->display, root, &event_mask, 1) != Success) {
        log_warn("Debug PrintScr observer disabled: XISelectEvents failed");
        debug_xi_opcode = -1;
        return;
    }

    XFlush(app->display);
    log_info("Debug PrintScr observer enabled via XInput2 %d.%d", major, minor);
}

static gboolean handle_debug_printscr_event(AppData *app, XEvent *event) {
    if (debug_xi_opcode < 0 || event->type != GenericEvent ||
        event->xcookie.extension != debug_xi_opcode) {
        return FALSE;
    }

    if (!XGetEventData(app->display, &event->xcookie)) {
        return FALSE;
    }

    gboolean handled = FALSE;
    if (event->xcookie.evtype == XI_RawKeyPress) {
        XIRawEvent *raw = (XIRawEvent *)event->xcookie.data;
        KeySym primary = XkbKeycodeToKeysym(app->display, (KeyCode)raw->detail, 0, 0);
        KeySym shifted = XkbKeycodeToKeysym(app->display, (KeyCode)raw->detail, 0, 1);
        if (primary == XK_Print || shifted == XK_Print) {
            debug_note_printscr(app);
            handled = TRUE;
        }
    }

    XFreeEventData(app->display, &event->xcookie);
    return handled;
}
#endif

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

// Prune subscribed windows that no longer exist in the window list.
// Only removes X11 property subscriptions — rule state is managed separately
// via rule_state_prune_absent to tolerate transient _NET_CLIENT_LIST churn.
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
        }
        // Absent windows: drop X subscription silently. Rule state is handled
        // by rule_state_prune_absent after apply_rules_to_windows.
    }
    subscribed_count = write;
}

// Apply rules to all windows (checks state machine — only fires on transitions)
static void apply_rules_to_windows(AppData *app) {
    if (app->rules_config.count == 0) return;
    if (app->in_rule_dispatch) {
        log_debug("RULE: apply_rules_to_windows skipped — re-entry during rule dispatch");
        return;
    }

    gint64 now_ms = g_get_monotonic_time() / 1000;
    for (int i = 0; i < app->window_count; i++) {
        WindowInfo *w = &app->windows[i];
        for (int r = 0; r < app->rules_config.count; r++) {
            RuleMatch match = check_rule_match(
                &app->rules_config.rules[r], &app->rule_state, w->id, w->title);
            if (match.should_fire) {
                if (!rule_breaker_should_fire(&app->rule_breaker, r, w->id,
                                              now_ms, app->rules_config.rules[r].pattern)) {
                    continue;
                }
                log_info("RULE: '%s' matched window 0x%lx '%s' — executing: %s",
                         app->rules_config.rules[r].pattern, w->id, w->title, match.commands);
                log_info("LOOPDBG: rule-fire[apply] rule='%s' win=0x%lx title='%s'",
                         app->rules_config.rules[r].pattern, w->id, w->title);
                gboolean prev = app->in_rule_dispatch;
                app->in_rule_dispatch = TRUE;
                execute_command_background(match.commands, app, w);
                app->in_rule_dispatch = prev;
            }
        }
    }
}

// Handle title change on a specific window
static void handle_window_title_change(AppData *app, Window id) {
    if (app->rules_config.count == 0) return;
    if (app->in_rule_dispatch) {
        log_debug("RULE: handle_window_title_change skipped — re-entry during rule dispatch");
        return;
    }

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
        gint64 now_ms = g_get_monotonic_time() / 1000;
        for (int r = 0; r < app->rules_config.count; r++) {
            RuleMatch match = check_rule_match(
                &app->rules_config.rules[r], &app->rule_state, id, w->title);
            if (match.should_fire) {
                if (!rule_breaker_should_fire(&app->rule_breaker, r, id,
                                              now_ms, app->rules_config.rules[r].pattern)) {
                    continue;
                }
                log_info("RULE: '%s' matched window 0x%lx '%s' — executing: %s",
                         app->rules_config.rules[r].pattern, id, w->title, match.commands);
                log_info("LOOPDBG: rule-fire[title] rule='%s' win=0x%lx title='%s'",
                         app->rules_config.rules[r].pattern, id, w->title);
                gboolean prev = app->in_rule_dispatch;
                app->in_rule_dispatch = TRUE;
                execute_command_background(match.commands, app, w);
                app->in_rule_dispatch = prev;
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

#ifdef COFI_DEBUG_PRINTSCR_CAPTURE
    setup_debug_printscr_capture(app);
#endif

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
#ifdef COFI_DEBUG_PRINTSCR_CAPTURE
    if (handle_debug_printscr_event(app, event)) {
        return;
    }
#endif

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
                // Snapshot for LOOPDBG leave/re-enter detection (before update)
                Window loopdbg_old_ids[MAX_WINDOWS];
                char loopdbg_old_titles[MAX_WINDOWS][64];
                for (int _i = 0; _i < old_count; _i++) {
                    loopdbg_old_ids[_i] = app->windows[_i].id;
                    snprintf(loopdbg_old_titles[_i], sizeof(loopdbg_old_titles[_i]),
                             "%s", app->windows[_i].title);
                }
                get_window_list(app);
                log_trace("Window count changed from %d to %d", old_count, app->window_count);
                // LOOPDBG: log windows that left and windows that entered the list
                for (int _i = 0; _i < old_count; _i++) {
                    bool still = false;
                    for (int _j = 0; _j < app->window_count; _j++) {
                        if (app->windows[_j].id == loopdbg_old_ids[_i]) { still = true; break; }
                    }
                    if (!still)
                        log_info("LOOPDBG: win LEFT  list 0x%lx '%s'",
                                 loopdbg_old_ids[_i], loopdbg_old_titles[_i]);
                }
                for (int _i = 0; _i < app->window_count; _i++) {
                    bool was = false;
                    for (int _j = 0; _j < old_count; _j++) {
                        if (loopdbg_old_ids[_j] == app->windows[_i].id) { was = true; break; }
                    }
                    if (!was)
                        log_info("LOOPDBG: win ENTERED list 0x%lx '%s'",
                                 app->windows[_i].id, app->windows[_i].title);
                }

                // Log current windows for debugging
                for (int i = 0; i < app->window_count; i++) {
                    log_trace("Current window %d: 0x%lx '%s' (%s)",
                             i, app->windows[i].id, app->windows[i].title, app->windows[i].class_name);
                }

                // Reassign matching entries used by matching + harpoon.
                log_trace("Calling match_entry_reassign_live_windows()");
                bool names_changed = match_entry_reassign_live_windows(&app->matching, app->windows, app->window_count);
                if (names_changed) {
                    save_match_entries(&app->matching);
                    log_debug("Saved reassigned matching entries after window list change");
                }

                // Subscribe to per-window property changes and apply rules
                prune_subscribed_windows(app);
                subscribe_to_window_properties(app);
                apply_rules_to_windows(app);

                // Grace-count rule state pruning: tolerate one transient absence
                // (e.g. WM unmap/remap during maximize toggle) before dropping state.
                Window live_ids[MAX_WINDOWS];
                for (int k = 0; k < app->window_count; k++) {
                    live_ids[k] = app->windows[k].id;
                }
                rule_state_prune_absent(&app->rule_state, live_ids, app->window_count);

                // Only process if window still exists and is valid
                if (app->window && GTK_IS_WIDGET(app->window) &&
                    app->entry && GTK_IS_ENTRY(app->entry)) {
                    // Skip filtering when in command mode
                    if (app->command_mode.state != CMD_MODE_NORMAL) {
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
