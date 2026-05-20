#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "app_data.h"
#include "cofi_tab_provider.h"
#include "display.h"
#include "window_info.h"
#include "log.h"
#include "constants.h"
#include "x11_utils.h"
#include "selection.h"
#include "harpoon.h"
#include "dynamic_display.h"
#include "named_window.h"
#include "display_pipeline.h"
#include "tab_switching.h"
#include "tab_metadata.h"
#include "tab_header.h"
#include "slot_store.h"
#include "utf8_columns.h"

#define UTF8_FIT_BUFFER_SIZE 4096

// Check if instance and class should be swapped for display
static gboolean should_swap_instance_class(const char *instance) {
    return (instance && strlen(instance) > 0 && instance[0] >= 'A' && instance[0] <= 'Z');
}

static void format_candidate_strip(AppData *app, GString *output) {
    if (!app || !output) {
        return;
    }

    const char **candidates = app->command_mode.candidates;
    int candidate_count = app->command_mode.candidate_count;
    int highlight = app->command_mode.candidate_highlight;

    if (candidate_count == 0) {
        return;
    }

    for (int i = 0; i < candidate_count; i++) {
        if (i > 0) {
            g_string_append(output, "  ");
        }

        if (i == highlight) {
            g_string_append_printf(output, "[ %s ]", candidates[i]);
        } else {
            g_string_append_printf(output, "  %s  ", candidates[i]);
        }
    }
}

// Format desktop string like Go code
static void format_desktop_str(int desktop, char *output) {
    if (desktop < 0 || desktop > 99) {
        strcpy(output, DESKTOP_STICKY_INDICATOR);
    } else {
        snprintf(output, 5, DESKTOP_FORMAT, desktop + 1);  // Display as 1-based
    }
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

    // Keep the scrollbar within the fixed display width so the final
    // utf8_clip_lines_to_columns pass cannot clip it off.
    char sb[visible_items + 1];
    generate_scrollbar(total_items, visible_items, scroll_offset, sb, visible_items);

    // Rebuild text with each line padded/truncated to target_columns
    GString *result = g_string_new(NULL);
    const char *p = text->str;
    int line = 0;

    while (*p && line < visible_items) {
        const char *nl = strchr(p, '\n');
        int line_len = nl ? (int)(nl - p) : (int)strlen(p);
        GString *line_text = g_string_new_len(p, line_len);
        utf8_clip_lines_to_columns(line_text, target_columns - 1);
        int clipped_cols = utf8_text_columns(line_text->str);
        g_string_append(result, line_text->str);
        for (int i = clipped_cols; i < target_columns - 1; i++) {
            g_string_append_c(result, ' ');
        }
        g_string_free(line_text, TRUE);
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

// Shared overlay adapter for display pipeline
static void overlay_scrollbar_adapter(gpointer context, GString *text,
                                      gint total_items, gint visible_items,
                                      gint scroll_offset, gint target_columns) {
    (void)context;
    overlay_scrollbar(text, total_items, visible_items,
                      scroll_offset, target_columns);
}

static void render_windows_item(gpointer context, gint index,
                                gint selected_idx, GString *text) {
    AppData *app = (AppData *)context;
    WindowInfo *win = &app->filtered[index];
    // Reclaim the space previously used by the hidden hex window ID column.
    enum { WINDOWS_TITLE_WIDTH = DISPLAY_TITLE_WIDTH + 12 };

    g_string_append(text,
                    (index == selected_idx) ? SELECTION_INDICATOR
                                            : NO_SELECTION_INDICATOR);

    char display_instance[MAX_CLASS_LEN];
    char display_class[MAX_CLASS_LEN];
    if (should_swap_instance_class(win->instance)) {
        strcpy(display_instance, win->class_name);
        strcpy(display_class, win->instance);
    } else {
        strcpy(display_instance, win->instance);
        strcpy(display_class, win->class_name);
    }

    char harpoon_col[DISPLAY_HARPOON_WIDTH + 2];
    char desktop_col[DISPLAY_DESKTOP_WIDTH + 1];
    char instance_col[UTF8_FIT_BUFFER_SIZE];
    char title_col[UTF8_FIT_BUFFER_SIZE];
    char class_col[UTF8_FIT_BUFFER_SIZE];

    gint slot = get_window_slot(&app->harpoon, win->id);

    if (slot >= 0) {
        if (slot <= HARPOON_LAST_NUMBER) {
            snprintf(harpoon_col, sizeof(harpoon_col), "%d ", slot);
        } else {
            snprintf(harpoon_col, sizeof(harpoon_col), "%c ",
                     'a' + (slot - HARPOON_FIRST_LETTER));
        }
    } else {
        strcpy(harpoon_col, "  ");
    }

    format_desktop_str(win->desktop, desktop_col);
    utf8_fit_columns(display_instance, DISPLAY_INSTANCE_WIDTH, instance_col, sizeof(instance_col));

    char display_title[MAX_TITLE_LEN];
    strncpy(display_title, win->title, sizeof(display_title) - 1);
    display_title[sizeof(display_title) - 1] = '\0';

    utf8_fit_columns(display_title, WINDOWS_TITLE_WIDTH, title_col, sizeof(title_col));
    utf8_fit_columns(display_class, DISPLAY_CLASS_WIDTH, class_col, sizeof(class_col));

    g_string_append(text, harpoon_col);
    g_string_append(text, desktop_col);
    g_string_append(text, " ");
    g_string_append(text, instance_col);
    g_string_append(text, " ");
    g_string_append(text, title_col);
    g_string_append(text, " ");
    g_string_append(text, class_col);
    g_string_append(text, "\n");
}

typedef struct {
    AppData *app;
    const CofiTabProvider *provider;
    int target_columns;
} ProviderRenderContext;

static void render_provider_item(gpointer context, gint index,
                                 gint selected_idx, GString *text) {
    ProviderRenderContext *provider_ctx = (ProviderRenderContext *)context;
    AppData *app = provider_ctx->app;
    const CofiTabProvider *p = provider_ctx->provider;
    int target_cols = provider_ctx->target_columns;

    CofiRowCells row;
    memset(&row, 0, sizeof(row));
    p->format_row(app, index, &row);

    int line_cols = 2;
    g_string_append(text, (index == selected_idx) ? "> " : "  ");
    if (p->slot_store_enabled && p->slot_payload_for &&
        (row.row_flags & COFI_ROW_SLOTTABLE)) {
        const char *payload = p->slot_payload_for(app, index);
        char slot = slot_for_payload(&app->harpoon.store, p->id, payload);
        if (slot != '\0') {
            g_string_append_printf(text, "[%c] ", slot);
        } else {
            g_string_append(text, "    ");
        }
        line_cols += 4;
    }

    for (int c = 0; c < row.cell_count; c++) {
        const char *t = row.cells[c].text ? row.cells[c].text : "";
        int w = row.cells[c].width_hint;
        if (c > 0) {
            g_string_append_c(text, ' ');
            line_cols++;
        }
        int remaining = target_cols - line_cols;
        if (target_cols > 0 && remaining <= 0) {
            break;
        }
        int col_width = w > 0 ? w : remaining;
        if (target_cols > 0 && col_width > remaining) {
            col_width = remaining;
        }
        if (col_width <= 0) {
            continue;
        }
        char col[UTF8_FIT_BUFFER_SIZE];
        int fit = col_width < 255 ? col_width : 255;
        utf8_fit_columns_aligned(t, fit, row.cells[c].align == 1, col, sizeof(col));
        g_string_append(text, col);
        line_cols += col_width;
    }
    g_string_append_c(text, '\n');
}

static void format_windows_display(AppData *app, GString *text, gint selected_idx) {
    if (app->filtered_count == 0) {
        g_string_append(text, "No matching windows found\n");
        return;
    }

    DisplayPipelineRequest request = {
        .total_count = app->filtered_count,
        .max_lines = get_max_display_lines_dynamic(app),
        .scroll_offset = get_scroll_offset(app),
        .selected_idx = selected_idx,
        .target_columns = get_display_columns(app),
        .context = app,
        .overlay_scrollbar = overlay_scrollbar_adapter,
    };
    request.render_item = render_windows_item;

    render_display_pipeline(&request, text);
}

static void format_provider_display(AppData *app, GString *text, gint selected_idx,
                                     int tab_mode) {
    const CofiTabProvider *p = cofi_get_provider_for_tab(tab_mode);
    if (!p || !p->row_count || !p->format_row) return;

    int count = p->row_count(app);
    if (count == 0)
        return;

    int provider_id = cofi_get_provider_id_for_tab(tab_mode);
    int raw_map[1024];
    int map_count = count < 1024 ? count : 1024;
    for (int i = 0; i < map_count; i++) {
        raw_map[i] = i;
    }
    cofi_set_filtered_map(provider_id, raw_map, map_count);

    ProviderRenderContext provider_ctx = {
        .app = app,
        .provider = p,
        .target_columns = get_display_columns(app),
    };

    DisplayPipelineRequest request = {
        .total_count = count,
        .max_lines = get_max_display_lines_dynamic(app),
        .scroll_offset = get_scroll_offset(app),
        .selected_idx = selected_idx,
        .target_columns = provider_ctx.target_columns,
        .context = &provider_ctx,
        .overlay_context = app,
        .render_item = render_provider_item,
        .overlay_scrollbar = overlay_scrollbar_adapter,
    };
    render_display_pipeline(&request, text);

    const char *shortcut_hint = p->get_shortcut_hint
        ? p->get_shortcut_hint(app)
        : p->shortcut_hint;
    if (shortcut_hint && shortcut_hint[0] != '\0') {
        g_string_append_c(text, '\n');
        g_string_append(text, shortcut_hint);
        g_string_append_c(text, '\n');
    }
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
    
    // Format content based on current tab: registered provider tabs first, then built-ins
    if (cofi_get_provider_for_tab(app->current_tab)) {
        format_provider_display(app, text, selected_idx, app->current_tab);
    } else {
        switch (app->current_tab) {
            case TAB_WINDOWS:
                format_windows_display(app, text, selected_idx);
                break;
            default:
                break;
        }
    }
    
    // Add tab header at the bottom
    tab_header_format(app, app->current_tab, get_display_columns(app), text);

    format_candidate_strip(app, text);
    utf8_clip_lines_to_columns(text, get_display_columns(app));
    
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
