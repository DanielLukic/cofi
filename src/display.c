#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "display.h"
#include "window_info.h"
#include "log.h"

// Format desktop string like Go code
static void format_desktop_str(int desktop, char *output) {
    if (desktop < 0 || desktop > 99) {
        strcpy(output, "[S] ");
    } else {
        snprintf(output, 5, "[%d] ", desktop);
    }
}

// Fit text to column width like Go code
static void fit_column(const char *text, int width, char *output) {
    if (!text || text[0] == '\0') {
        // Fill with spaces if empty
        memset(output, ' ', width);
        output[width] = '\0';
        return;
    }
    
    // Remove newlines and normalize spaces (simplified)
    char clean_text[512];
    strncpy(clean_text, text, sizeof(clean_text) - 1);
    clean_text[sizeof(clean_text) - 1] = '\0';
    
    // Replace newlines with spaces
    for (char *p = clean_text; *p; p++) {
        if (*p == '\n') *p = ' ';
    }
    
    // Truncate if too long
    if (strlen(clean_text) > (size_t)width) {
        clean_text[width] = '\0';
    }
    
    // Left-align with padding
    snprintf(output, width + 1, "%-*s", width, clean_text);
}

// Update the text display with proper 5-column format like Go code
void update_display(AppData *app) {
    log_debug("update_display() - filtered_count=%d, selected_index=%d", 
            app->filtered_count, app->selected_index);
    
    // Log first few windows to understand ordering
    if (app->filtered_count > 0) {
        log_trace("Data order - [0]: '%s' (0x%lx), [1]: '%s' (0x%lx)", 
                app->filtered[0].title, app->filtered[0].id,
                (app->filtered_count > 1) ? app->filtered[1].title : "(none)",
                (app->filtered_count > 1) ? app->filtered[1].id : 0);
        log_trace("Display order (after swap) - [0]: '%s', [1]: '%s'",
                (app->filtered_count > 1) ? app->filtered[1].title : app->filtered[0].title,
                (app->filtered_count > 1) ? app->filtered[0].title : "(none)");
        log_trace("Selected index: %d (displaying '%s')", 
                app->selected_index,
                (app->filtered_count > 1) ? app->filtered[1].title : app->filtered[0].title);
    }
    
    GString *text = g_string_new("");
    
    // Column widths (from Go code)
    const int HARPOON_WIDTH = 1;    // "0" or " "
    const int DESKTOP_WIDTH = 4;   // "[0] "
    const int INSTANCE_WIDTH = 20;
    const int TITLE_WIDTH = 55;
    const int CLASS_WIDTH = 18;
    
    // Display filtered windows in reverse order (bottom-up like fzf)
    // The Alt-Tab swap is already applied in the filtered array
    for (int i = app->filtered_count - 1; i >= 0; i--) {
        WindowInfo *win = &app->filtered[i];
        
        gboolean is_selected = (i == app->selected_index);
        
        // Selection indicator
        if (is_selected) {
            g_string_append(text, "> ");
        } else {
            g_string_append(text, "  ");
        }
        
        // Apply instance/class swapping rule like Go code (line 82-84)
        char display_instance[MAX_CLASS_LEN];
        char display_class[MAX_CLASS_LEN];
        
        if (strlen(win->instance) > 0 && win->instance[0] >= 'A' && win->instance[0] <= 'Z') {
            // Swap if instance starts with uppercase
            strcpy(display_instance, win->class_name);
            strcpy(display_class, win->instance);
        } else {
            strcpy(display_instance, win->instance);
            strcpy(display_class, win->class_name);
        }
        
        // Format each column
        char harpoon_col[HARPOON_WIDTH + 2]; // +2 for space after
        char desktop_col[DESKTOP_WIDTH + 1];
        char instance_col[INSTANCE_WIDTH + 1];
        char title_col[TITLE_WIDTH + 1];
        char class_col[CLASS_WIDTH + 1];
        char window_id[32];
        
        // Check if this window has a harpoon assignment
        int slot = get_window_slot(&app->harpoon, win->id);
        Window display_id = win->id;
        
        if (slot >= 0) {
            snprintf(harpoon_col, sizeof(harpoon_col), "%d ", slot);
            // Use the window ID from the harpoon slot since it may have been reassigned
            if (app->harpoon.slots[slot].assigned) {
                display_id = app->harpoon.slots[slot].id;
            }
        } else {
            strcpy(harpoon_col, "  ");
        }
        
        format_desktop_str(win->desktop, desktop_col);
        fit_column(display_instance, INSTANCE_WIDTH, instance_col);
        fit_column(win->title, TITLE_WIDTH, title_col);
        fit_column(display_class, CLASS_WIDTH, class_col);
        snprintf(window_id, sizeof(window_id), "0x%lx", display_id);
        
        // Build the line: harpoon desktop instance title class window_id
        g_string_append(text, harpoon_col);
        g_string_append(text, desktop_col);
        g_string_append(text, " ");
        g_string_append(text, instance_col);
        g_string_append(text, " ");
        g_string_append(text, title_col);
        g_string_append(text, " ");
        g_string_append(text, class_col);
        g_string_append(text, " ");
        g_string_append(text, window_id);
        g_string_append(text, "\n");
    }
    
    // If no windows, show message
    if (app->filtered_count == 0) {
        g_string_append(text, "No matching windows found\n");
    }
    
    // Set the text
    gtk_text_buffer_set_text(app->textbuffer, text->str, -1);
    g_string_free(text, TRUE);
}

// Send X11 client message (based on wmctrl implementation)
static int client_msg(Display *disp, Window win, const char *msg, 
    unsigned long data0, unsigned long data1, 
    unsigned long data2, unsigned long data3,
    unsigned long data4) {
    XEvent event;
    long mask = SubstructureRedirectMask | SubstructureNotifyMask;

    event.xclient.type = ClientMessage;
    event.xclient.serial = 0;
    event.xclient.send_event = True;
    event.xclient.message_type = XInternAtom(disp, msg, False);
    event.xclient.window = win;
    event.xclient.format = 32;
    event.xclient.data.l[0] = data0;
    event.xclient.data.l[1] = data1;
    event.xclient.data.l[2] = data2;
    event.xclient.data.l[3] = data3;
    event.xclient.data.l[4] = data4;

    if (XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event)) {
        return 0;
    }
    else {
        log_error("Cannot send %s event.", msg);
        return -1;
    }
}

// Activate window using direct X11 calls (replacing wmctrl)
void activate_window(Window window_id) {
    Display *disp = XOpenDisplay(NULL);
    if (!disp) {
        log_error("Cannot open display for window activation");
        return;
    }
    
    // Get the window's desktop
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *data = NULL;
    unsigned long desktop = 0;
    
    Atom desktop_atom = XInternAtom(disp, "_NET_WM_DESKTOP", False);
    if (XGetWindowProperty(disp, window_id, desktop_atom, 0, 1, False,
                          XA_CARDINAL, &actual_type, &actual_format,
                          &nitems, &bytes_after, &data) == Success && data) {
        desktop = *(unsigned long *)data;
        XFree(data);
        
        // Switch to the window's desktop
        client_msg(disp, DefaultRootWindow(disp), "_NET_CURRENT_DESKTOP", 
                  desktop, 0, 0, 0, 0);
    }
    
    // Send activation message
    client_msg(disp, window_id, "_NET_ACTIVE_WINDOW", 0, 0, 0, 0, 0);
    
    // Ensure window is mapped and raised
    XMapRaised(disp, window_id);
    
    // Flush X11 commands
    XFlush(disp);
    
    XCloseDisplay(disp);
}