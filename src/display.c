#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "app_data.h"
#include "display.h"
#include "window_info.h"
#include "log.h"
#include "constants.h"
#include "x11_utils.h"
#include "selection.h"
#include "harpoon.h"
#include "dynamic_display.h"
#include "named_window.h"

// Check if instance and class should be swapped for display
static gboolean should_swap_instance_class(const char *instance) {
    return (instance && strlen(instance) > 0 && instance[0] >= 'A' && instance[0] <= 'Z');
}

// Format tab header with active tab indication
static void format_tab_header(TabMode current_tab, GString *output) {
    g_string_append(output, "\n");
    g_string_append(output, "  ");
    
    if (current_tab == TAB_WINDOWS) {
        g_string_append(output, "[ WINDOWS ]");
    } else {
        g_string_append(output, "  Windows  ");
    }
    
    g_string_append(output, "    ");
    
    if (current_tab == TAB_WORKSPACES) {
        g_string_append(output, "[ WORKSPACES ]");
    } else {
        g_string_append(output, "  Workspaces  ");
    }
    
    g_string_append(output, "    ");
    
    if (current_tab == TAB_HARPOON) {
        g_string_append(output, "[ HARPOON ]");
    } else {
        g_string_append(output, "  Harpoon  ");
    }
    
    g_string_append(output, "    ");
    
    if (current_tab == TAB_NAMES) {
        g_string_append(output, "[ NAMES ]");
    } else {
        g_string_append(output, "  Names  ");
    }

    g_string_append(output, "    ");

    if (current_tab == TAB_CONFIG) {
        g_string_append(output, "[ CONFIG ]");
    } else {
        g_string_append(output, "  Config  ");
    }

    g_string_append(output, "    ");

    if (current_tab == TAB_HOTKEYS) {
        g_string_append(output, "[ HOTKEYS ]");
    } else {
        g_string_append(output, "  Hotkeys  ");
    }

    g_string_append(output, "\n");
}

// Format desktop string like Go code
static void format_desktop_str(int desktop, char *output) {
    if (desktop < 0 || desktop > 99) {
        strcpy(output, DESKTOP_STICKY_INDICATOR);
    } else {
        snprintf(output, 5, DESKTOP_FORMAT, desktop + 1);  // Display as 1-based
    }
}

// Clean text: replace non-ASCII and newlines with spaces, squash consecutive spaces
static void clean_text(const char *text, char *output, size_t output_size) {
    int j = 0;
    int last_was_space = 0;
    
    for (int i = 0; text[i] && j < (int)output_size - 1; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c < 32 || c > 126) {
            // Replace non-printable and non-ASCII with space
            if (!last_was_space) {
                output[j++] = ' ';
                last_was_space = 1;
            }
        } else if (c == ' ') {
            // Regular space
            if (!last_was_space) {
                output[j++] = ' ';
                last_was_space = 1;
            }
        } else {
            // Normal character
            output[j++] = text[i];
            last_was_space = 0;
        }
    }
    output[j] = '\0';
}

// Trim leading and trailing spaces from text
static char* trim_text(char *text) {
    // Trim leading spaces
    char *start = text;
    while (*start == ' ') start++;
    
    // Trim trailing spaces
    char *end = start + strlen(start) - 1;
    while (end > start && *end == ' ') end--;
    *(end + 1) = '\0';
    
    return start;
}

// Pad text to specified width
static void pad_text(const char *text, int width, char *output) {
    snprintf(output, width + 1, "%-*s", width, text);
}

// Fit text to column width like Go code
static void fit_column(const char *text, int width, char *output) {
    if (!text || text[0] == '\0') {
        // Fill with spaces if empty
        memset(output, ' ', width);
        output[width] = '\0';
        return;
    }
    
    // Clean text
    char clean_buffer[512];
    clean_text(text, clean_buffer, sizeof(clean_buffer));
    
    // Trim spaces
    char *trimmed = trim_text(clean_buffer);
    
    // Truncate if too long
    if (strlen(trimmed) > (size_t)width) {
        trimmed[width] = '\0';
    }
    
    // Left-align with padding
    pad_text(trimmed, width, output);
}

// Get maximum display lines using dynamic calculation
int get_max_display_lines(void) {
    // For now, we need access to the app data to get the window
    // This is a temporary solution until we refactor the display system
    // to pass the app data or window context properly

    // TODO: This is a temporary approach - ideally we should pass AppData* to this function
    // For now, fall back to the constant but log that we need dynamic calculation
    static gboolean logged_warning = FALSE;
    if (!logged_warning) {
        log_debug("get_max_display_lines() called without app context - using fallback constant");
        logged_warning = TRUE;
    }

    return MAX_DISPLAY_LINES;
}

// Dynamic version that takes app data for proper calculation
int get_max_display_lines_dynamic(AppData *app) {
    if (!app) {
        log_warn("get_max_display_lines_dynamic called with NULL app data");
        return MAX_DISPLAY_LINES;
    }

    return get_dynamic_max_display_lines(app);
}

// Generate text-based scrollbar
void generate_scrollbar(int total_items, int visible_items, int scroll_offset, char *scrollbar, int scrollbar_height) {
    if (!scrollbar || scrollbar_height <= 0) return;

    if (total_items <= visible_items) {
        for (int i = 0; i < scrollbar_height; i++)
            scrollbar[i] = ' ';
        scrollbar[scrollbar_height] = '\0';
        return;
    }

    double visible_ratio = (double)visible_items / total_items;
    double position_ratio = (double)scroll_offset / (total_items - visible_items);

    int thumb_size = (int)(visible_ratio * scrollbar_height);
    if (thumb_size < 1) thumb_size = 1;
    if (thumb_size > scrollbar_height) thumb_size = scrollbar_height;
    if (scrollbar_height > 1 && thumb_size == scrollbar_height) thumb_size = scrollbar_height - 1;

    int thumb_start = (int)(position_ratio * (scrollbar_height - thumb_size));
    if (thumb_start < 0) thumb_start = 0;
    if (thumb_start + thumb_size > scrollbar_height) thumb_start = scrollbar_height - thumb_size;

    for (int i = 0; i < scrollbar_height; i++)
        scrollbar[i] = (i >= thumb_start && i < thumb_start + thumb_size) ? '#' : '.';
    scrollbar[scrollbar_height] = '\0';
}

// Overlay scrollbar indicators on the rightmost column of each line in text.
// Pads short lines with spaces, truncates long lines to target_columns.
// Modifies text in place. For bottom-up (fzf-style) display, caller should
// pass flipped offset: (total_items - visible_items) - scroll_offset.
void overlay_scrollbar(GString *text, int total_items, int visible_items, int scroll_offset, int target_columns) {
    if (total_items <= visible_items || target_columns <= 0) return;

    // Find max line length so scrollbar column is at least past all content
    int max_line_len = 0;
    const char *scan = text->str;
    while (*scan) {
        const char *nl = strchr(scan, '\n');
        int len = nl ? (int)(nl - scan) : (int)strlen(scan);
        if (len > max_line_len) max_line_len = len;
        if (!nl) break;
        scan = nl + 1;
    }
    // Content + 1 space gap + 1 scrollbar char
    int content_columns = max_line_len + 2;
    if (content_columns > target_columns) target_columns = content_columns;

    char sb[visible_items + 1];
    generate_scrollbar(total_items, visible_items, scroll_offset, sb, visible_items);

    // Rebuild text with each line padded/truncated to target_columns
    GString *result = g_string_new(NULL);
    const char *p = text->str;
    int line = 0;

    while (*p && line < visible_items) {
        const char *nl = strchr(p, '\n');
        int line_len = nl ? (int)(nl - p) : (int)strlen(p);

        if (line_len >= target_columns) {
            // Truncate to target_columns - 1, then scrollbar
            g_string_append_len(result, p, target_columns - 1);
        } else {
            // Append content, pad with spaces
            g_string_append_len(result, p, line_len);
            for (int i = line_len; i < target_columns - 1; i++)
                g_string_append_c(result, ' ');
        }
        g_string_append_c(result, sb[line]);
        g_string_append_c(result, '\n');

        line++;
        p = nl ? nl + 1 : p + line_len;
    }

    // Append remaining lines unchanged (if any)
    if (*p) g_string_append(result, p);

    g_string_assign(text, result->str);
    g_string_free(result, TRUE);
}

// Format and display windows tab content
static void format_windows_display(AppData *app, GString *text, int selected_idx) {
    int total_count = app->filtered_count;
    int max_lines = get_max_display_lines_dynamic(app);

    // If no windows, show message
    if (app->filtered_count == 0) {
        g_string_append(text, "No matching windows found\n");
        return;
    }

    int scroll_offset = get_scroll_offset(app);

    // Calculate visible range
    int start_idx = scroll_offset;
    int end_idx = start_idx + max_lines;
    if (end_idx > total_count) {
        end_idx = total_count;
    }

    // Display windows in reverse order (best matches at bottom, fzf-style)
    // But we need to account for scrolling
    int display_line = 0;
    for (int i = end_idx - 1; i >= start_idx && display_line < max_lines; i--, display_line++) {
        WindowInfo *win = &app->filtered[i];
        
        gboolean is_selected = (i == selected_idx);
        
        // Selection indicator
        if (is_selected) {
            g_string_append(text, SELECTION_INDICATOR);
        } else {
            g_string_append(text, NO_SELECTION_INDICATOR);
        }
        
        // Apply instance/class swapping rule like Go code (line 82-84)
        char display_instance[MAX_CLASS_LEN];
        char display_class[MAX_CLASS_LEN];
        
        if (should_swap_instance_class(win->instance)) {
            // Swap if instance starts with uppercase
            strcpy(display_instance, win->class_name);
            strcpy(display_class, win->instance);
        } else {
            strcpy(display_instance, win->instance);
            strcpy(display_class, win->class_name);
        }
        
        // Format each column
        char harpoon_col[DISPLAY_HARPOON_WIDTH + 2]; // +2 for space after
        char desktop_col[DISPLAY_DESKTOP_WIDTH + 1];
        char instance_col[DISPLAY_INSTANCE_WIDTH + 1];
        char title_col[DISPLAY_TITLE_WIDTH + 1];
        char class_col[DISPLAY_CLASS_WIDTH + 1];
        char window_id[32];
        
        // Check if this window has a harpoon assignment
        int slot = get_window_slot(&app->harpoon, win->id);
        Window display_id = win->id;
        
        if (slot >= 0) {
            if (slot <= HARPOON_LAST_NUMBER) {
                snprintf(harpoon_col, sizeof(harpoon_col), "%d ", slot);
            } else {
                snprintf(harpoon_col, sizeof(harpoon_col), "%c ", 'a' + (slot - HARPOON_FIRST_LETTER));
            }
            // Use the window ID from the harpoon slot since it may have been reassigned
            if (app->harpoon.slots[slot].assigned) {
                display_id = app->harpoon.slots[slot].id;
            }
        } else {
            strcpy(harpoon_col, "  ");
        }
        
        format_desktop_str(win->desktop, desktop_col);
        fit_column(display_instance, DISPLAY_INSTANCE_WIDTH, instance_col);
        
        // Use the window title directly (custom names are already included by filtering)
        char display_title[MAX_TITLE_LEN];
        strncpy(display_title, win->title, sizeof(display_title) - 1);
        display_title[sizeof(display_title) - 1] = '\0';
        
        fit_column(display_title, DISPLAY_TITLE_WIDTH, title_col);
        fit_column(display_class, DISPLAY_CLASS_WIDTH, class_col);
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

    // Overlay scrollbar on last column (flipped offset for bottom-up display)
    int flipped = (total_count > max_lines) ? (total_count - max_lines) - scroll_offset : 0;
    overlay_scrollbar(text, total_count, max_lines, flipped, get_display_columns(app));
}

// Format and display workspaces tab content
static void format_workspaces_display(AppData *app, GString *text, int selected_idx) {
    int total_count = app->filtered_workspace_count;
    int max_lines = get_max_display_lines_dynamic(app);

    // If no workspaces, show message
    if (app->filtered_workspace_count == 0) {
        g_string_append(text, "No matching workspaces found\n");
        return;
    }

    int scroll_offset = get_scroll_offset(app);

    // Calculate visible range
    int start_idx = scroll_offset;
    int end_idx = start_idx + max_lines;
    if (end_idx > total_count) {
        end_idx = total_count;
    }

    // Display workspaces in reverse order (entry 0 at bottom like windows)
    int display_line = 0;
    for (int i = end_idx - 1; i >= start_idx && display_line < max_lines; i--, display_line++) {
        WorkspaceInfo *ws = &app->filtered_workspaces[i];

        gboolean is_selected = (i == selected_idx);

        // Selection indicator
        if (is_selected) {
            g_string_append(text, "> ");
        } else {
            g_string_append(text, "  ");
        }

        // Current workspace indicator
        if (ws->is_current) {
            g_string_append(text, "* ");
        } else {
            g_string_append(text, "  ");
        }

        // Format: [ID] Name
        g_string_append_printf(text, "[%d] %s", ws->id + 1, ws->name);

        g_string_append(text, "\n");
    }

    int flipped = (total_count > max_lines) ? (total_count - max_lines) - scroll_offset : 0;
    overlay_scrollbar(text, total_count, max_lines, flipped, get_display_columns(app));
}

// Format and display harpoon tab content
static void format_harpoon_display(AppData *app, GString *text, int selected_idx) {
    int total_count = app->filtered_harpoon_count;
    int max_lines = get_max_display_lines_dynamic(app);

    int scroll_offset = get_scroll_offset(app);

    // Calculate visible range
    int start_idx = scroll_offset;
    int end_idx = start_idx + max_lines;
    if (end_idx > total_count) {
        end_idx = total_count;
    }

    // Display harpoon slots in reverse order (entry 0 at bottom like windows)
    int display_line = 0;
    for (int i = end_idx - 1; i >= start_idx && display_line < max_lines; i--, display_line++) {
        HarpoonSlot *slot = &app->filtered_harpoon[i];

        gboolean is_selected = (i == selected_idx);

        // Selection indicator
        if (is_selected) {
            g_string_append(text, "> ");
        } else {
            g_string_append(text, "  ");
        }

        // Format slot name (0-9, a-z)
        char slot_name[4];
        int slot_idx = app->filtered_harpoon_indices[i];
        if (slot_idx < 10) {
            snprintf(slot_name, sizeof(slot_name), "%d", slot_idx);
        } else {
            snprintf(slot_name, sizeof(slot_name), "%c", 'a' + (slot_idx - 10));
        }

        // Format slot display
        if (slot->assigned) {
            char title_col[56], class_col[19], instance_col[21], type_col[9];
            fit_column(slot->title, 55, title_col);
            fit_column(slot->class_name, 18, class_col);
            fit_column(slot->instance, 20, instance_col);
            fit_column(slot->type, 8, type_col);

            g_string_append_printf(text, "%-4s %s %s %s %s",
                slot_name, title_col, class_col, instance_col, type_col);
        } else {
            g_string_append_printf(text, "%-4s %-55s %-18s %-20s %-8s",
                slot_name, "* EMPTY *", "-", "-", "-");
        }

        g_string_append(text, "\n");
    }

    int flipped = (total_count > max_lines) ? (total_count - max_lines) - scroll_offset : 0;
    overlay_scrollbar(text, total_count, max_lines, flipped, get_display_columns(app));

    // Shortcuts footer
    g_string_append(text, "\n");
    g_string_append(text, "Shortcuts: Ctrl+E=Edit pattern  Ctrl+D=Delete  (patterns: * = any, . = single char)\n");
}

// Format and display names tab content
static void format_names_display(AppData *app, GString *text, int selected_idx) {
    int total_count = app->filtered_names_count;
    int max_lines = get_max_display_lines_dynamic(app);

    // If no named windows, show message and shortcuts
    if (app->filtered_names_count == 0) {
        g_string_append(text, "No named windows found\n");
        g_string_append(text, "\n");
        g_string_append(text, "Shortcuts: Ctrl+E=Edit name  Ctrl+D=Delete name\n");
        return;
    }

    int scroll_offset = get_scroll_offset(app);

    // Calculate visible range
    int start_idx = scroll_offset;
    int end_idx = start_idx + max_lines;
    if (end_idx > total_count) {
        end_idx = total_count;
    }

    // Display named windows in reverse order (entry 0 at bottom like windows)
    int display_line = 0;
    for (int i = end_idx - 1; i >= start_idx && display_line < max_lines; i--, display_line++) {
        NamedWindow *named = &app->filtered_names[i];

        gboolean is_selected = (i == selected_idx);

        // Selection indicator
        if (is_selected) {
            g_string_append(text, "> ");
        } else {
            g_string_append(text, "  ");
        }

        // Format columns — sized to fit within ~115 char budget (matching windows tab)
        char custom_name_col[21], original_title_col[46], class_col[19];
        char window_id[12];

        fit_column(named->custom_name, 20, custom_name_col);
        fit_column(named->original_title, 45, original_title_col);
        fit_column(named->class_name, 18, class_col);

        if (named->assigned) {
            snprintf(window_id, sizeof(window_id), "0x%lx", named->id);
        } else {
            strcpy(window_id, "* NONE *");
        }

        // Build the line: custom_name original_title class window_id
        g_string_append(text, custom_name_col);
        g_string_append(text, " ");
        g_string_append(text, original_title_col);
        g_string_append(text, " ");
        g_string_append(text, class_col);
        g_string_append(text, " ");
        g_string_append(text, window_id);

        g_string_append(text, "\n");
    }

    int flipped = (total_count > max_lines) ? (total_count - max_lines) - scroll_offset : 0;
    overlay_scrollbar(text, total_count, max_lines, flipped, get_display_columns(app));

    // Add shortcuts footer
    g_string_append(text, "\n");
    g_string_append(text, "Shortcuts: Ctrl+E=Edit name  Ctrl+D=Delete name\n");
}

// Format and display config tab content
static void format_config_display_tab(AppData *app, GString *text, int selected_idx) {
    int total_count = app->filtered_config_count;
    int max_lines = get_max_display_lines_dynamic(app);

    if (total_count == 0) {
        g_string_append(text, "No matching config options found\n");
        g_string_append(text, "\n");
        g_string_append(text, "Shortcuts: Ctrl+T=Toggle bool  Ctrl+E=Edit value\n");
        return;
    }

    int scroll_offset = get_scroll_offset(app);

    int start_idx = scroll_offset;
    int end_idx = start_idx + max_lines;
    if (end_idx > total_count) {
        end_idx = total_count;
    }

    int display_line = 0;
    for (int i = end_idx - 1; i >= start_idx && display_line < max_lines; i--, display_line++) {
        ConfigEntry *entry = &app->filtered_config[i];

        gboolean is_selected = (i == selected_idx);

        if (is_selected) {
            g_string_append(text, "> ");
        } else {
            g_string_append(text, "  ");
        }

        char key_col[33], value_col[61];
        fit_column(entry->key, 32, key_col);
        fit_column(entry->value, 60, value_col);

        g_string_append(text, key_col);
        g_string_append(text, " ");
        g_string_append(text, value_col);

        g_string_append(text, "\n");
    }

    int flipped = (total_count > max_lines) ? (total_count - max_lines) - scroll_offset : 0;
    overlay_scrollbar(text, total_count, max_lines, flipped, get_display_columns(app));

    g_string_append(text, "\n");
    g_string_append(text, "Shortcuts: Ctrl+T=Toggle bool  Ctrl+E=Edit value\n");
}

// Format and display hotkeys tab content
static void format_hotkeys_display(AppData *app, GString *text, int selected_idx) {
    int total_count = app->filtered_hotkeys_count;
    int max_lines = get_max_display_lines_dynamic(app);

    if (total_count == 0) {
        g_string_append(text, "No hotkey bindings found\n");
        g_string_append(text, "\n");
        g_string_append(text, "Shortcuts: Ctrl+A=Add binding\n");
        return;
    }

    int scroll_offset = get_scroll_offset(app);

    int start_idx = scroll_offset;
    int end_idx = start_idx + max_lines;
    if (end_idx > total_count) {
        end_idx = total_count;
    }

    int display_line = 0;
    for (int i = end_idx - 1; i >= start_idx && display_line < max_lines; i--, display_line++) {
        HotkeyBinding *binding = &app->filtered_hotkeys[i];

        gboolean is_selected = (i == selected_idx);

        if (is_selected) {
            g_string_append(text, "> ");
        } else {
            g_string_append(text, "  ");
        }

        char key_col[25], cmd_col[71];
        fit_column(binding->key, 24, key_col);
        fit_column(binding->command, 70, cmd_col);

        g_string_append(text, key_col);
        g_string_append(text, " ");
        g_string_append(text, cmd_col);

        g_string_append(text, "\n");
    }

    int flipped = (total_count > max_lines) ? (total_count - max_lines) - scroll_offset : 0;
    overlay_scrollbar(text, total_count, max_lines, flipped, get_display_columns(app));

    g_string_append(text, "\n");
    g_string_append(text, "Shortcuts: Ctrl+A=Add binding  Ctrl+E=Edit command  Ctrl+D=Delete binding\n");
}

// Update the text display with proper 5-column format like Go code
void update_display(AppData *app) {
    int selected_idx = get_selected_index(app);
    log_debug("update_display() - filtered_count=%d, selected_index=%d",
            app->filtered_count, selected_idx);
    
    // Don't update display if help is being shown in command mode
    if (app->command_mode.state == CMD_MODE_COMMAND && app->command_mode.showing_help) {
        log_debug("Skipping display update - help is being shown");
        return;
    }
    
    // Log first few windows to understand ordering
    if (app->filtered_count > 0) {
        log_trace("Data order - [0]: '%s' (0x%lx), [1]: '%s' (0x%lx)", 
                app->filtered[0].title, app->filtered[0].id,
                (app->filtered_count > 1) ? app->filtered[1].title : "(none)",
                (app->filtered_count > 1) ? app->filtered[1].id : 0);
        log_trace("Selected index: %d (displaying '%s')",
                selected_idx,
                (selected_idx < app->filtered_count) ? app->filtered[selected_idx].title : "(none)");
    }
    
    GString *text = g_string_new("");
    
    // Format content based on current tab
    switch (app->current_tab) {
        case TAB_WINDOWS:
            format_windows_display(app, text, selected_idx);
            break;
        case TAB_WORKSPACES:
            format_workspaces_display(app, text, selected_idx);
            break;
        case TAB_HARPOON:
            format_harpoon_display(app, text, selected_idx);
            break;
        case TAB_NAMES:
            format_names_display(app, text, selected_idx);
            break;
        case TAB_CONFIG:
            format_config_display_tab(app, text, selected_idx);
            break;
        case TAB_HOTKEYS:
            format_hotkeys_display(app, text, selected_idx);
            break;
    }
    
    // Add tab header at the bottom
    format_tab_header(app->current_tab, text);
    
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

// Activate window using direct X11 calls
void activate_window(Display *disp, Window window_id) {
    // Get the window's desktop
    int actual_format;
    unsigned long n_items;
    unsigned char *data = NULL;
    unsigned long desktop = 0;
    
    Atom desktop_atom = XInternAtom(disp, "_NET_WM_DESKTOP", False);
    if (get_x11_property(disp, window_id, desktop_atom, XA_CARDINAL,
                        1, NULL, &actual_format, &n_items, &data) == COFI_SUCCESS) {
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
}
