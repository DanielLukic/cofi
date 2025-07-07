#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <getopt.h>
#include <ctype.h>
#include "app_data.h"
#include "x11_utils.h"
#include "window_list.h"
#include "workspace_info.h"
#include "history.h"
#include "display.h"
#include "filter.h"
#include "harpoon_config.h"
#include "log.h"
#include "x11_events.h"
#include "instance.h"
#include "harpoon.h"
#include "match.h"
#include "command_mode.h"
#include "constants.h"
#include "utils.h"
#include "cli_args.h"
#include "gtk_window.h"
#include "selection.h"
#include "app_init.h"
#include "overlay_manager.h"
#include "version.h"
#include "dbus_service.h"

// MAX_WINDOWS, MAX_TITLE_LEN, MAX_CLASS_LEN are defined in src/window_info.h
// WindowInfo and AppData types are defined in src/app_data.h

// Forward declarations
static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, AppData *app);
static void on_entry_changed(GtkEntry *entry, AppData *app);
static gboolean on_delete_event(GtkWidget *widget, GdkEvent *event, AppData *app);
static gboolean on_focus_out_event(GtkWidget *widget, GdkEventFocus *event, AppData *app);
static void filter_workspaces(AppData *app, const char *filter);
static void filter_harpoon(AppData *app, const char *filter);
static gboolean handle_harpoon_tab_keys(GdkEventKey *event, AppData *app);

// Forward declaration for destroy_window function
void destroy_window(AppData *app);
static gboolean check_focus_loss_delayed(AppData *app);

// Note: Selection management functions moved to selection.c

// Helper function to switch between tabs
static void switch_to_tab(AppData *app, TabMode target_tab) {
    if (app->current_tab == target_tab) {
        return; // Already on the target tab
    }
    
    app->current_tab = target_tab;
    gtk_entry_set_text(GTK_ENTRY(app->entry), "");
    
    // Update placeholder text based on tab
    if (target_tab == TAB_WINDOWS) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter windows...");
        filter_windows(app, "");
    } else if (target_tab == TAB_WORKSPACES) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter workspaces...");
        filter_workspaces(app, "");
    } else if (target_tab == TAB_HARPOON) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter harpoon slots...");
        // Initialize harpoon display with no filter
        filter_harpoon(app, "");
    }
    reset_selection(app);
    
    update_display(app);
    const char *tab_names[] = {"Windows", "Workspaces", "Harpoon"};
    log_debug("Switched to %s tab", tab_names[target_tab]);
}

// Helper function to get harpoon slot from key event
static int get_harpoon_slot(GdkEventKey *event, gboolean is_assignment, AppData *app) {
    (void)app; // Unused parameter
    if (event->keyval >= GDK_KEY_0 && event->keyval <= GDK_KEY_9) {
        return event->keyval - GDK_KEY_0;
    } else if (event->keyval >= GDK_KEY_KP_0 && event->keyval <= GDK_KEY_KP_9) {
        return event->keyval - GDK_KEY_KP_0;
    } else if (event->keyval >= GDK_KEY_a && event->keyval <= GDK_KEY_z) {
        // For activation (Alt+key), we allow all keys including j,k,u
        // For assignment (Ctrl+key), we exclude j,k,u unless Shift is pressed
        if (is_assignment) {
            gboolean is_excluded_key = (event->keyval == GDK_KEY_j || 
                                       event->keyval == GDK_KEY_k ||
                                       event->keyval == GDK_KEY_u);
            
            if (is_excluded_key && !(event->state & GDK_SHIFT_MASK)) {
                // Excluded key without Shift - reject for assignment
                return -1;
            }
        }
        
        return HARPOON_FIRST_LETTER + (event->keyval - GDK_KEY_a);
    } else if (event->keyval >= GDK_KEY_A && event->keyval <= GDK_KEY_Z) {
        // Handle uppercase letters (when Shift is pressed)
        // For uppercase (shifted) keys, we allow even the normally excluded keys j,k,u
        // because user explicitly pressed Shift
        return HARPOON_FIRST_LETTER + (event->keyval - GDK_KEY_A);
    }
    return -1;
}

// Note: Selection movement functions moved to selection.c

// Handle Ctrl+key for harpoon assignment/unassignment
static gboolean handle_harpoon_assignment(GdkEventKey *event, AppData *app) {
    if (!(event->state & GDK_CONTROL_MASK) || app->current_tab != TAB_WINDOWS) {
        return FALSE;
    }
    
    int slot = get_harpoon_slot(event, TRUE, app);  // TRUE = this is assignment
    if (slot < 0 || app->filtered_count == 0) {
        return FALSE;
    }

    // Get the selected window
    WindowInfo *selected_window = get_selected_window(app);
    if (!selected_window) {
        return FALSE;
    }

    // Use the already retrieved selected window
    WindowInfo *win = selected_window;
    
    // Check if this window is already assigned to this slot
    Window current_window = get_slot_window(&app->harpoon, slot);
    if (current_window == win->id) {
        // Unassign if same window
        unassign_slot(&app->harpoon, slot);
        log_info("Unassigned window '%s' from slot %d", win->title, slot);
    } else {
        // First unassign any other slot this window might have
        int old_slot = get_window_slot(&app->harpoon, win->id);
        if (old_slot >= 0) {
            unassign_slot(&app->harpoon, old_slot);
        }
        // Assign to new slot
        assign_window_to_slot(&app->harpoon, slot, win);
        log_info("Assigned window '%s' to slot %d", win->title, slot);
    }
    
    // Save config options and harpoon slots separately
    save_config(&app->config);
    save_harpoon_slots(&app->harpoon);
    update_display(app);
    return TRUE;
}

// Handle Alt+key for harpoon/workspace switching
static gboolean handle_harpoon_workspace_switching(GdkEventKey *event, AppData *app) {
    if (!(event->state & GDK_MOD1_MASK)) {  // GDK_MOD1_MASK is Alt
        return FALSE;
    }
    
    int slot = get_harpoon_slot(event, FALSE, app);  // FALSE = this is activation, not assignment
    if (slot < 0) {
        return FALSE;
    }
    
    if (app->current_tab == TAB_WINDOWS) {
        // Switch to harpooned window
        Window target_window = get_slot_window(&app->harpoon, slot);
        if (target_window != 0) {
            activate_window(target_window);
            
            // Clear last commanded window since this is normal selection
            if (app->last_commanded_window_id != 0) {
                log_info("Clearing last commanded window ID (was: 0x%lx)", app->last_commanded_window_id);
                app->last_commanded_window_id = 0;
            }
            
            destroy_window(app);
            log_info("Switched to harpooned window in slot %d", slot);
            return TRUE;
        }
    } else {
        // Switch to workspace by number
        if (slot < app->workspace_count) {
            switch_to_desktop(app->display, slot - 1);
            destroy_window(app);
            log_info("Switched to workspace %d", slot);
            return TRUE;
        }
    }
    
    return FALSE;
}

// Handle tab switching keys
static gboolean handle_tab_switching(GdkEventKey *event, AppData *app) {
    // Tab key (without Ctrl)
    if (event->keyval == GDK_KEY_Tab && !(event->state & GDK_CONTROL_MASK)) {
        TabMode next_tab;
        
        // Check if Shift is pressed for reverse direction
        if (event->state & GDK_SHIFT_MASK) {
            // Shift+Tab - go backwards
            switch (app->current_tab) {
                case TAB_WINDOWS:
                    next_tab = TAB_HARPOON;
                    break;
                case TAB_WORKSPACES:
                    next_tab = TAB_WINDOWS;
                    break;
                case TAB_HARPOON:
                    next_tab = TAB_WORKSPACES;
                    break;
                default:
                    next_tab = TAB_WINDOWS;
                    break;
            }
            const char *tab_names[] = {"Windows", "Workspaces", "Harpoon"};
            log_info("USER: SHIFT+TAB pressed -> Switching to %s tab", tab_names[next_tab]);
        } else {
            // Regular Tab - go forwards
            switch (app->current_tab) {
                case TAB_WINDOWS:
                    next_tab = TAB_WORKSPACES;
                    break;
                case TAB_WORKSPACES:
                    next_tab = TAB_HARPOON;
                    break;
                case TAB_HARPOON:
                    next_tab = TAB_WINDOWS;
                    break;
                default:
                    next_tab = TAB_WINDOWS;
                    break;
            }
            const char *tab_names[] = {"Windows", "Workspaces", "Harpoon"};
            log_info("USER: TAB pressed -> Switching to %s tab", tab_names[next_tab]);
        }
        
        switch_to_tab(app, next_tab);
        return TRUE;
    }
    
    // Handle ISO_Left_Tab (Shift+Tab on some systems)
    if (event->keyval == GDK_KEY_ISO_Left_Tab) {
        TabMode next_tab;
        switch (app->current_tab) {
            case TAB_WINDOWS:
                next_tab = TAB_HARPOON;
                break;
            case TAB_WORKSPACES:
                next_tab = TAB_WINDOWS;
                break;
            case TAB_HARPOON:
                next_tab = TAB_WORKSPACES;
                break;
            default:
                next_tab = TAB_WINDOWS;
                break;
        }
        
        const char *tab_names[] = {"Windows", "Workspaces", "Harpoon"};
        log_info("USER: SHIFT+TAB pressed -> Switching to %s tab", tab_names[next_tab]);
        switch_to_tab(app, next_tab);
        return TRUE;
    }
    
    return FALSE;
}

// Handle harpoon tab specific keys
static gboolean handle_harpoon_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != TAB_HARPOON) {
        return FALSE;
    }
    
    // Handle Ctrl+d for delete
    if (event->keyval == GDK_KEY_d && (event->state & GDK_CONTROL_MASK)) {
        // Get current harpoon slot from filtered list
        if (app->selection.harpoon_index < app->filtered_harpoon_count) {
            HarpoonSlot *slot = &app->filtered_harpoon[app->selection.harpoon_index];
            
            // Only allow delete if slot is assigned
            if (slot->assigned) {
                // Get the actual slot index from the filtered indices
                int actual_slot = app->filtered_harpoon_indices[app->selection.harpoon_index];
                
                // Show delete confirmation overlay
                show_harpoon_delete_overlay(app, actual_slot);
                return TRUE;
            }
        }
    }
    
    return FALSE;
}

// Handle navigation keys (arrows, Ctrl+j/k)
static gboolean handle_navigation_keys(GdkEventKey *event, AppData *app) {
    switch (event->keyval) {
        case GDK_KEY_Escape:
            // If in harpoon delete mode, cancel it instead of closing
            if (app->current_tab == TAB_HARPOON && app->harpoon_delete.pending_delete) {
                app->harpoon_delete.pending_delete = FALSE;
                log_info("Cancelled harpoon delete");
                update_display(app);
                return TRUE;
            }
            log_info("USER: ESCAPE pressed -> Closing cofi");
            destroy_window(app);
            return TRUE;
            
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            if (app->current_tab == TAB_WINDOWS) {
                WindowInfo *win = get_selected_window(app);
                if (win) {
                    log_info("USER: ENTER pressed -> Activating window '%s' (ID: 0x%lx)",
                             win->title, win->id);
                    activate_window(win->id);
                    
                    // Clear last commanded window since this is normal selection
                    if (app->last_commanded_window_id != 0) {
                        log_info("Clearing last commanded window ID (was: 0x%lx)", app->last_commanded_window_id);
                        app->last_commanded_window_id = 0;
                    }
                    
                    destroy_window(app);
                }
            } else {
                WorkspaceInfo *ws = get_selected_workspace(app);
                if (ws) {
                    log_info("USER: ENTER pressed -> Switching to workspace %d: %s", ws->id, ws->name);
                    switch_to_desktop(app->display, ws->id);
                    destroy_window(app);
                }
            }
            return TRUE;
            
        case GDK_KEY_Up:
            move_selection_up(app);
            return TRUE;
            
        case GDK_KEY_Down:
            move_selection_down(app);
            return TRUE;
        
        case GDK_KEY_k:
            if (event->state & GDK_CONTROL_MASK) {
                move_selection_up(app);
                return TRUE;
            }
            break;
            
        case GDK_KEY_j:
            if (event->state & GDK_CONTROL_MASK) {
                move_selection_down(app);
                return TRUE;
            }
            break;
    }
    
    return FALSE;
}

// Handle key press events
static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, AppData *app) {
    (void)widget; // Unused parameter

    // Handle overlay events first if any overlay is active
    if (is_overlay_active(app)) {
        return handle_overlay_key_press(app, event);
    }

    // Handle command mode if active
    if (app->command_mode.state == CMD_MODE_COMMAND) {
        return handle_command_key(event, app);
    }
    
    // Handle ':' key to enter command mode
    if (event->keyval == GDK_KEY_colon) {
        log_info("USER: ':' pressed -> Entering command mode");
        
        // If Alt-Tab swap was applied (lcwi was 0) and we're at index 0,
        // move selection to 1 to select the "actual" current window
        if (app->last_commanded_window_id == 0 && 
            app->current_tab == TAB_WINDOWS &&
            app->selection.window_index == 0 && 
            app->filtered_count >= 2) {
            log_info("Command mode: Moving selection from 0 to 1 (Alt-Tab swap was active)");
            app->selection.window_index = 1;
            if (app->filtered_count > 1) {
                app->selection.selected_window_id = app->filtered[1].id;
            }
            update_display(app);
        }
        
        enter_command_mode(app);
        return TRUE;
    }
    
    // Try handlers in order of priority
    if (handle_harpoon_assignment(event, app)) {
        return TRUE;
    }
    
    if (handle_harpoon_workspace_switching(event, app)) {
        return TRUE;
    }
    
    if (handle_harpoon_tab_keys(event, app)) {
        return TRUE;
    }
    
    if (handle_tab_switching(event, app)) {
        return TRUE;
    }
    
    if (handle_navigation_keys(event, app)) {
        return TRUE;
    }
    
    return FALSE;
}

// Workspace filtering with fuzzy search
static void filter_workspaces(AppData *app, const char *filter) {
    app->filtered_workspace_count = 0;
    
    if (!filter || !*filter) {
        // No filter - show all workspaces
        for (int i = 0; i < app->workspace_count; i++) {
            app->filtered_workspaces[app->filtered_workspace_count++] = app->workspaces[i];
        }
        return;
    }
    
    // Build searchable string for each workspace: "id name"
    char searchable[512];
    for (int i = 0; i < app->workspace_count; i++) {
        snprintf(searchable, sizeof(searchable), "%d %s", 
                 app->workspaces[i].id + 1, app->workspaces[i].name);
        
        // Use has_match and match functions from filter system
        if (has_match(filter, searchable)) {
            app->filtered_workspaces[app->filtered_workspace_count++] = app->workspaces[i];
        }
    }
}

// Harpoon filtering
static void filter_harpoon(AppData *app, const char *filter) {
    app->filtered_harpoon_count = 0;
    
    if (!filter || !*filter) {
        // No filter - show all harpoon slots
        for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
            app->filtered_harpoon[app->filtered_harpoon_count] = app->harpoon.slots[i];
            app->filtered_harpoon_indices[app->filtered_harpoon_count] = i;
            app->filtered_harpoon_count++;
        }
        return;
    }
    
    // Build searchable string for each harpoon slot
    char searchable[1024];
    for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
        HarpoonSlot *slot = &app->harpoon.slots[i];
        
        // Get slot name
        char slot_name[4];
        if (i < 10) {
            snprintf(slot_name, sizeof(slot_name), "%d", i);
        } else {
            snprintf(slot_name, sizeof(slot_name), "%c", 'a' + (i - 10));
        }
        
        if (slot->assigned) {
            // Build searchable string: "slot title class instance"
            snprintf(searchable, sizeof(searchable), "%s %s %s %s", 
                     slot_name, slot->title, slot->class_name, slot->instance);
        } else {
            // For empty slots, just search by slot name
            snprintf(searchable, sizeof(searchable), "%s empty", slot_name);
        }
        
        // Use has_match for filtering
        if (has_match(filter, searchable)) {
            app->filtered_harpoon[app->filtered_harpoon_count] = *slot;
            app->filtered_harpoon_indices[app->filtered_harpoon_count] = i;
            app->filtered_harpoon_count++;
        }
    }
}

// Handle entry text changes
static void on_entry_changed(GtkEntry *entry, AppData *app) {
    // Skip filtering when in command mode
    if (app->command_mode.state == CMD_MODE_COMMAND) {
        return;
    }
    
    const char *text = gtk_entry_get_text(entry);

    if (strlen(text) > 0) {
        log_info("USER: Filter text changed -> '%s'", text);
    }

    if (app->current_tab == TAB_WINDOWS) {
        filter_windows(app, text);
    } else if (app->current_tab == TAB_WORKSPACES) {
        filter_workspaces(app, text);
    } else if (app->current_tab == TAB_HARPOON) {
        filter_harpoon(app, text);
    }
    // Ensure selection is always 0 after filtering
    reset_selection(app);

    update_display(app);
}

// Handle window delete event (close button)
static gboolean on_delete_event(GtkWidget *widget, GdkEvent *event, AppData *app) {
    (void)widget;
    (void)event;
    destroy_window(app);
    return TRUE; // Prevent default handler
}

// Handle focus out event
static gboolean on_focus_out_event(GtkWidget *widget, GdkEventFocus *event, AppData *app) {
    (void)widget;
    (void)event;
    
    // Always reset command mode when losing focus
    if (app->command_mode.state == CMD_MODE_COMMAND) {
        log_debug("Resetting command mode due to focus loss");
        exit_command_mode(app);
    }
    
    // Only close if close_on_focus_loss is enabled
    if (!app->config.close_on_focus_loss) {
        return FALSE;
    }
    
    // Use a small delay to properly detect if focus is really lost
    // This helps distinguish between internal widget focus changes and actual window focus loss
    g_timeout_add(100, (GSourceFunc)check_focus_loss_delayed, app);
    
    return FALSE;
}

// Delayed check for focus loss
static gboolean check_focus_loss_delayed(AppData *app) {
    if (!app->window) {
        return FALSE; // Window already destroyed
    }
    

    
    // Check if our window still has toplevel focus
    if (gtk_window_has_toplevel_focus(GTK_WINDOW(app->window))) {
        log_debug("Window still has toplevel focus after delay, not closing");
        return FALSE;
    }
    
    log_info("Window lost focus to external application, closing");
    destroy_window(app);
    return FALSE; // Don't repeat the timeout
}

// Destroy window instead of hiding it
void destroy_window(AppData *app) {
    if (app->window) {
        // Save config options and harpoon slots separately
        save_config(&app->config);
        save_harpoon_slots(&app->harpoon);
        
        gtk_widget_destroy(app->window);
        app->window = NULL;
        app->entry = NULL;
        app->textview = NULL;
        app->scrolled = NULL;
        app->textbuffer = NULL;
        
        // Reset command mode state when window is destroyed
        app->command_mode.state = CMD_MODE_NORMAL;
        app->command_mode.showing_help = FALSE;
        app->command_mode.command_buffer[0] = '\0';
        app->command_mode.cursor_pos = 0;
        app->command_mode.history_index = -1;
        
        // ALWAYS reset selection to 0
        reset_selection(app);
        log_debug("Selection reset to 0 in destroy_window");
    }
}


// Application setup
void setup_application(AppData *app, WindowAlignment alignment) {
    // Store alignment for future window recreations
    app->config.alignment = alignment;
    
    // Create main window
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "cofi");
    // Set reasonable default size; will be adjusted based on content
    gtk_window_set_default_size(GTK_WINDOW(app->window), 900, 500);
    // Set window position based on alignment
    apply_window_position(app->window, alignment);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(app->window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(app->window), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(app->window), FALSE);
    
    // Create main overlay container
    app->main_overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(app->window), app->main_overlay);

    // Create main content container (the original vbox)
    app->main_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(app->main_overlay), app->main_content);
    
    // Create text view and buffer (bottom-aligned, no scrolling)
    app->textview = gtk_text_view_new();
    app->textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->textview));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app->textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app->textview), FALSE);
    gtk_widget_set_can_focus(app->textview, FALSE);
    
    // Set monospace font and overlay styling using CSS
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const char *css =
        "textview { font-family: monospace; font-size: 12pt; }\n"
        "entry { font-family: monospace; font-size: 12pt; }\n"
        "#modal-background { background-color: rgba(0, 0, 0, 0.7); }\n"
        "#dialog-overlay { background-color: @theme_bg_color; border: 2px solid @theme_border_color; border-radius: 8px; box-shadow: 0 8px 32px rgba(0, 0, 0, 0.5); padding: 20px; margin: 20px; }\n"
        ".grid-cell { border: 1px solid @theme_border_color; background-color: @theme_base_color; border-radius: 3px; margin: 2px; }";
    gtk_css_provider_load_from_data(css_provider, css, -1, NULL);
    
    GtkStyleContext *textview_context = gtk_widget_get_style_context(app->textview);
    gtk_style_context_add_provider(textview_context,
                                   GTK_STYLE_PROVIDER(css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    // Add margins
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(app->textview), 10);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(app->textview), 10);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(app->textview), 10);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(app->textview), 10);
    
    // Bottom alignment: expand and pin to end
    gtk_widget_set_vexpand(app->textview, TRUE);
    gtk_widget_set_valign(app->textview, GTK_ALIGN_END);
    
    // Create entry
    app->entry = gtk_entry_new();
    // Set placeholder text based on current tab
    if (app->current_tab == TAB_WORKSPACES) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter workspaces...");
    } else if (app->current_tab == TAB_HARPOON) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter harpoon slots...");
    } else {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter windows...");
    }
    
    // Apply CSS to entry widget too
    GtkStyleContext *entry_context = gtk_widget_get_style_context(app->entry);
    gtk_style_context_add_provider(entry_context,
                                   GTK_STYLE_PROVIDER(css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // Apply CSS globally to all widgets (needed for overlay styling)
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                               GTK_STYLE_PROVIDER(css_provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_unref(css_provider);
    
    // Pack widgets into main content container (text view on top, entry at bottom - like fzf)
    gtk_box_pack_start(GTK_BOX(app->main_content), app->textview, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(app->main_content), app->entry, FALSE, FALSE, 0);
    
    // Connect signals
    g_signal_connect(app->window, "delete-event", G_CALLBACK(on_delete_event), app);
    g_signal_connect(app->window, "key-press-event", G_CALLBACK(on_key_press), app);
    g_signal_connect(app->entry, "changed", G_CALLBACK(on_entry_changed), app);
    g_signal_connect(app->window, "focus-out-event", G_CALLBACK(on_focus_out_event), app);
    
    // Make window capture all key events
    gtk_widget_set_can_focus(app->window, TRUE);
    gtk_widget_grab_focus(app->entry);
    
    // Additional window hints for better focus handling
    gtk_window_set_type_hint(GTK_WINDOW(app->window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_focus_on_map(GTK_WINDOW(app->window), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(app->window), TRUE);
    
    // Position window manually for non-center alignments
    if (alignment != ALIGN_CENTER) {
        // Store alignment in app data for use in callback
        app->config.alignment = alignment;
        // Connect to size-allocate to reposition whenever size changes
        g_signal_connect(app->window, "size-allocate", G_CALLBACK(on_window_size_allocate), app);
    }

    // Initialize overlay system
    init_overlay_system(app);
}



int main(int argc, char *argv[]) {
    AppData app = {0};
    
    // Default settings
    // Initialize config with defaults
    init_config_defaults(&app.config);
    int log_enabled = 1;
    char *log_file_path = NULL;
    FILE *log_file = NULL;
    int alignment_specified = 0;
    int close_on_focus_loss_specified = 0;
    
    // Set default log level to INFO
    log_set_level(LOG_INFO);
    
    // Parse command line arguments
    int parse_result = parse_command_line(argc, argv, &app, &log_file_path, &log_enabled, &alignment_specified, &close_on_focus_loss_specified, NULL);
    if (parse_result == 2) {
        // Version was printed
        return 0;
    } else if (parse_result == 3) {
        // Help was printed
        return 0;
    } else if (parse_result == 4) {
        // Command mode help was printed
        return 0;
    } else if (parse_result != 0) {
        return 1;
    }
    
    // Open log file if specified
    if (log_file_path) {
        log_file = fopen(log_file_path, "a");
        if (!log_file) {
            fprintf(stderr, "Failed to open log file: %s\n", log_file_path);
            return 1;
        }
    }
    
    // Initialize logging
    if (log_enabled) {
        if (log_file) {
            log_add_fp(log_file, LOG_DEBUG);
        }
    } else {
        log_set_quiet(true);
    }
    
    log_debug("Starting cofi...");
    
    // Check for existing instance
    InstanceManager *instance_manager = instance_manager_new();
    if (!instance_manager) {
        log_error("Failed to create instance manager");
        if (log_file) fclose(log_file);
        return 1;
    }
    
    // Determine show mode based on command line arguments
    ShowMode show_mode;
    if (app.start_in_command_mode) {
        show_mode = SHOW_MODE_COMMAND;
    } else if (app.current_tab == TAB_WORKSPACES) {
        show_mode = SHOW_MODE_WORKSPACES;
    } else if (app.current_tab == TAB_HARPOON) {
        show_mode = SHOW_MODE_HARPOON;
    } else {
        show_mode = SHOW_MODE_WINDOWS;
    }
    
    if (instance_manager_check_existing_with_mode(instance_manager, show_mode)) {
        log_info("Another instance is already running, exiting");
        instance_manager_cleanup(instance_manager);
        if (log_file) fclose(log_file);
        return 0;
    }
    
    // Set program name for WM_CLASS property
    g_set_prgname("cofi");
    
    // Initialize GTK (needs to be called with modified argc/argv after getopt)
    gtk_init(&argc, &argv);
    
    // Initialize application data
    init_app_data(&app);

    // Open X11 display
    init_x11_connection(&app);

    // Load config options and harpoon slots separately
    // Note: load_harpoon_slots must come AFTER init_app_data since init_harpoon_manager clears slots
    load_config(&app.config);
    load_harpoon_slots(&app.harpoon);

    // Apply precedence rules for close_on_focus_loss
    // Command line overrides config file (already set by cli_args.c if specified)
    log_debug("close_on_focus_loss = %d (cmdline_specified=%d)", app.config.close_on_focus_loss, close_on_focus_loss_specified);

    // Apply precedence rules for alignment:
    // 1. Command line --align takes highest precedence (already set by cli_args.c if specified)
    // 2. Config align is fallback (already loaded)
    if (alignment_specified) {
        // Command line --align was specified, save the updated config
        save_config(&app.config);
        log_debug("Using command line alignment: %d", app.config.alignment);
    } else {
        log_debug("Using config alignment: %d", app.config.alignment);
    }
    
    // Initialize window and workspace lists
    init_window_list(&app);
    init_workspaces(&app);

    // Initialize history from windows
    init_history_from_windows(&app);

    // Initialize selection management
    init_selection(&app);
    
    // Setup GUI
    setup_application(&app, app.config.alignment);
    
    // Set app data for instance manager and setup D-Bus service
    // Do this after GUI setup so the window exists
    instance_manager_set_app_data(&app);
    dbus_service_set_app_data(&app);
    instance_manager_setup_dbus_service(instance_manager);
    
    // Setup X11 event monitoring for dynamic window list updates
    setup_x11_event_monitoring(&app);
    
    // Initialize the correct tab data based on start tab
    if (app.current_tab == TAB_WINDOWS) {
        filter_windows(&app, "");
    } else if (app.current_tab == TAB_WORKSPACES) {
        filter_workspaces(&app, "");
    } else if (app.current_tab == TAB_HARPOON) {
        filter_harpoon(&app, "");
    }
    
    // Update display
    update_display(&app);
    
    // ALWAYS reset selection to 0 before showing window
    reset_selection(&app);
    log_debug("Selection reset to 0 before showing window");
    
    // Show window and run
    gtk_widget_show_all(app.window);
    gtk_widget_grab_focus(app.entry);
    
    // Enter command mode if requested via --command
    if (app.start_in_command_mode) {
        enter_command_mode(&app);
        log_info("Started in command mode via --command flag");
    }
    
    // Log last commanded window if set
    if (app.last_commanded_window_id != 0) {
        log_info("Last commanded window ID: 0x%lx", app.last_commanded_window_id);
    }
    
    // Get our own window ID for filtering
    GdkWindow *gdk_window = gtk_widget_get_window(app.window);
    if (gdk_window) {
        app.own_window_id = GDK_WINDOW_XID(gdk_window);
        log_debug("Stored own window ID: 0x%lx", app.own_window_id);
    } else {
        app.own_window_id = 0;
        log_warn("Could not get own window ID");
    }
    
    gtk_main();
    
    // Cleanup
    cleanup_x11_event_monitoring();
    instance_manager_cleanup(instance_manager);
    XCloseDisplay(app.display);
    
    if (log_file) {
        log_debug("Closing log file");
        fclose(log_file);
    }
    
    return 0;
}