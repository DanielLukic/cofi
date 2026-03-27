#include "workspace_overlay.h"
#include "app_data.h"
#include "log.h"
#include "selection.h"
#include "x11_utils.h"
#include "workspace_utils.h"
#include "gtk_utils.h"
#include <gtk/gtk.h>

// Forward declarations
static GtkWidget* create_workspace_widget_overlay(int workspace_num, const char *workspace_name,
                                                  gboolean is_current, gboolean is_user_current);
extern void hide_window(AppData *app); // From main.c

// Create workspace jump overlay content
void create_workspace_jump_overlay_content(GtkWidget *parent_container, AppData *app) {
    // Header
    GtkWidget *header_label = create_markup_label("<b>Jump to Workspace</b>", TRUE);
    gtk_box_pack_start(GTK_BOX(parent_container), header_label, FALSE, FALSE, 0);

    // Separator
    add_horizontal_separator(parent_container);

    // Get workspace information
    int workspace_count = get_limited_workspace_count(app->display);

    // Get workspace names
    WorkspaceNames *names = get_workspace_names(app->display);

    // Get current desktop (where the user currently is)
    int user_current_desktop = get_current_desktop(app->display);

    // Layout: arrow diamond on left | separator | workspace grid on right
    GtkWidget *ws_container = create_workspace_layout_with_arrows(parent_container);

    // Create workspace display based on configuration
    if (app->config.workspaces_per_row > 0) {
        // Grid layout
        GtkWidget *grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
        gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(ws_container), grid, TRUE, TRUE, 0);

        int per_row = app->config.workspaces_per_row;

        for (int i = 0; i < workspace_count; i++) {
            int row = i / per_row;
            int col = i % per_row;

            const char *workspace_name = get_workspace_name_or_default(names, i);

            GtkWidget *ws_widget = create_workspace_widget_overlay(
                i + 1, workspace_name,
                FALSE, (i == user_current_desktop)
            );
            gtk_grid_attach(GTK_GRID(grid), ws_widget, col, row, 1, 1);
        }
    } else {
        // Linear list layout
        GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request(scrolled, 400, 200);
        gtk_box_pack_start(GTK_BOX(ws_container), scrolled, TRUE, TRUE, 0);

        GtkWidget *text_view = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
        gtk_container_add(GTK_CONTAINER(scrolled), text_view);

        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
        GString *display_text = g_string_new("");

        for (int i = 0; i < workspace_count; i++) {
            const char *workspace_name = get_workspace_name_or_default(names, i);
            if (i == user_current_desktop) {
                g_string_append_printf(display_text, "◆%d◆ %s (current)\n", i + 1, workspace_name);
            } else {
                g_string_append_printf(display_text, "[%d] %s\n", i + 1, workspace_name);
            }
        }

        gtk_text_buffer_set_text(buffer, display_text->str, -1);
        g_string_free(display_text, TRUE);
    }

    free_workspace_names(names);

    // Instructions
    create_workspace_instructions(parent_container);
}

// Create workspace move overlay content
void create_workspace_move_overlay_content(GtkWidget *parent_container, AppData *app) {
    // Get selected window using centralized selection management
    WindowInfo *selected_window = get_selected_window(app);
    if (!selected_window) {
        show_no_window_error(parent_container, "workspace move");
        return;
    }

    // Header with window title
    char *escaped_title = g_markup_escape_text(selected_window->title, -1);
    char header_text[512];
    snprintf(header_text, sizeof(header_text),
             "<b>Move Window to Workspace:</b> %s", escaped_title);

    GtkWidget *header_label = create_markup_label(header_text, TRUE);
    gtk_box_pack_start(GTK_BOX(parent_container), header_label, FALSE, FALSE, 0);

    g_free(escaped_title);

    // Separator
    add_horizontal_separator(parent_container);

    // Get workspace information
    int workspace_count = get_limited_workspace_count(app->display);

    // Get workspace names
    WorkspaceNames *names = get_workspace_names(app->display);

    // Get current desktop (where the user currently is)
    int user_current_desktop = get_current_desktop(app->display);

    // Get window's current desktop
    int window_current_desktop = selected_window->desktop;

    // Layout: arrow diamond on left | separator | workspace grid on right
    GtkWidget *ws_container = create_workspace_layout_with_arrows(parent_container);

    // Create workspace display based on configuration
    if (app->config.workspaces_per_row > 0) {
        GtkWidget *grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
        gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(ws_container), grid, TRUE, TRUE, 0);

        int per_row = app->config.workspaces_per_row;

        for (int i = 0; i < workspace_count; i++) {
            int row = i / per_row;
            int col = i % per_row;

            const char *workspace_name = get_workspace_name_or_default(names, i);
            GtkWidget *ws_widget = create_workspace_widget_overlay(
                i + 1, workspace_name,
                (i == window_current_desktop), (i == user_current_desktop)
            );
            gtk_grid_attach(GTK_GRID(grid), ws_widget, col, row, 1, 1);
        }
    } else {
        GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request(scrolled, 400, 200);
        gtk_box_pack_start(GTK_BOX(ws_container), scrolled, TRUE, TRUE, 0);

        GtkWidget *text_view = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
        gtk_container_add(GTK_CONTAINER(scrolled), text_view);

        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
        GString *display_text = g_string_new("");

        for (int i = 0; i < workspace_count; i++) {
            const char *workspace_name = get_workspace_name_or_default(names, i);
            if (i == window_current_desktop && i == user_current_desktop) {
                g_string_append_printf(display_text, "★%d★ %s (window & user here)\n", i + 1, workspace_name);
            } else if (i == window_current_desktop) {
                g_string_append_printf(display_text, "●%d● %s (window here)\n", i + 1, workspace_name);
            } else if (i == user_current_desktop) {
                g_string_append_printf(display_text, "◆%d◆ %s (user here)\n", i + 1, workspace_name);
            } else {
                g_string_append_printf(display_text, "[%d] %s\n", i + 1, workspace_name);
            }
        }

        gtk_text_buffer_set_text(buffer, display_text->str, -1);
        g_string_free(display_text, TRUE);
    }

    free_workspace_names(names);
    create_workspace_instructions(parent_container);
}

// Create a single workspace widget for overlay grid layout
static GtkWidget* create_workspace_widget_overlay(int workspace_num, const char *workspace_name,
                                                  gboolean is_current, gboolean is_user_current) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(box, 120, 80);

    // Number label with different indicators for different states
    GtkWidget *number_label = gtk_label_new(NULL);
    char number_text[64];
    if (is_current && is_user_current) {
        // Both window and user are on this workspace
        snprintf(number_text, sizeof(number_text), "<b>★%d★</b>", workspace_num);
    } else if (is_current) {
        // Window is on this workspace
        snprintf(number_text, sizeof(number_text), "<b>●%d●</b>", workspace_num);
    } else if (is_user_current) {
        // User is on this workspace
        snprintf(number_text, sizeof(number_text), "<b>◆%d◆</b>", workspace_num);
    } else {
        // Neither window nor user is on this workspace
        snprintf(number_text, sizeof(number_text), "<b>[%d]</b>", workspace_num);
    }
    gtk_label_set_markup(GTK_LABEL(number_label), number_text);
    gtk_box_pack_start(GTK_BOX(box), number_label, FALSE, FALSE, 0);

    // Name label
    GtkWidget *name_label = gtk_label_new(workspace_name);
    gtk_label_set_line_wrap(GTK_LABEL(name_label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(name_label), 15);
    gtk_box_pack_start(GTK_BOX(box), name_label, FALSE, FALSE, 0);

    // Current indicator
    if (is_current) {
        GtkWidget *current_label = gtk_label_new("(current)");
        gtk_box_pack_start(GTK_BOX(box), current_label, FALSE, FALSE, 0);
    }

    // Style the box based on different states
    GtkCssProvider *provider = gtk_css_provider_new();
    GtkStyleContext *context = gtk_widget_get_style_context(box);

    if (is_current && is_user_current) {
        // Both window and user are here - bright highlight
        gtk_widget_set_name(box, "workspace-both");
        gtk_css_provider_load_from_data(provider,
            "#workspace-both { background-color: #666666; border: 2px solid #888888; padding: 8px; }", -1, NULL);
    } else if (is_current) {
        // Window is here - medium highlight
        gtk_widget_set_name(box, "workspace-window");
        gtk_css_provider_load_from_data(provider,
            "#workspace-window { background-color: #444444; border: 1px solid #666666; padding: 9px; }", -1, NULL);
    } else if (is_user_current) {
        // User is here - subtle highlight
        gtk_widget_set_name(box, "workspace-user");
        gtk_css_provider_load_from_data(provider,
            "#workspace-user { background-color: #333333; border: 1px dashed #555555; padding: 9px; }", -1, NULL);
    } else {
        // Neither - no special styling
        gtk_widget_set_name(box, "workspace-normal");
        gtk_css_provider_load_from_data(provider,
            "#workspace-normal { padding: 10px; }", -1, NULL);
    }

    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

    return box;
}

// Handle key press events for workspace jump overlay
gboolean handle_workspace_jump_key_press(AppData *app, GdkEventKey *event) {
    int target = resolve_workspace_from_key(app->display, event, app->config.workspaces_per_row);
    if (target < 0) return FALSE;

    int current = get_current_desktop(app->display);
    if (target != current) {
        switch_to_desktop(app->display, target);
        log_info("USER: Jumped from workspace %d to workspace %d", current + 1, target + 1);
    }
    hide_window(app);
    return TRUE;
}

// Handle key press events for workspace move overlay
gboolean handle_workspace_move_key_press(AppData *app, GdkEventKey *event) {
    WindowInfo *selected_window = get_selected_window(app);
    if (!selected_window) {
        log_error("No window selected for workspace move");
        return TRUE;
    }

    int target = resolve_workspace_from_key(app->display, event, app->config.workspaces_per_row);
    if (target < 0) return FALSE;

    move_window_to_desktop(app->display, selected_window->id, target);
    log_info("USER: Moved window '%s' to workspace %d", selected_window->title, target + 1);
    hide_window(app);
    return TRUE;
}

// Create workspace move all overlay content
void create_workspace_move_all_overlay_content(GtkWidget *parent_container, AppData *app) {
    // Header with count of windows to move
    char header_text[512];
    snprintf(header_text, sizeof(header_text),
             "<b>Move All Windows from Current Workspace</b>\n%d windows will be moved",
             app->windows_to_move_count);

    GtkWidget *header_label = gtk_label_new(NULL);
    gtk_widget_set_halign(header_label, GTK_ALIGN_CENTER);
    gtk_label_set_markup(GTK_LABEL(header_label), header_text);
    gtk_label_set_line_wrap(GTK_LABEL(header_label), TRUE);
    gtk_box_pack_start(GTK_BOX(parent_container), header_label, FALSE, FALSE, 0);

    // Separator
    add_horizontal_separator(parent_container);

    // Get workspace information
    int workspace_count = get_limited_workspace_count(app->display);

    // Get workspace names
    WorkspaceNames *names = get_workspace_names(app->display);

    // Get current desktop (where the user currently is)
    int user_current_desktop = get_current_desktop(app->display);

    // Layout: arrow diamond on left | separator | workspace grid on right
    GtkWidget *ws_container = create_workspace_layout_with_arrows(parent_container);

    if (app->config.workspaces_per_row > 0) {
        GtkWidget *grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
        gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(ws_container), grid, TRUE, TRUE, 0);

        int per_row = app->config.workspaces_per_row;

        for (int i = 0; i < workspace_count; i++) {
            int row = i / per_row;
            int col = i % per_row;

            const char *workspace_name = get_workspace_name_or_default(names, i);
            GtkWidget *ws_widget = create_workspace_widget_overlay(
                i + 1, workspace_name,
                FALSE, (i == user_current_desktop)
            );
            gtk_grid_attach(GTK_GRID(grid), ws_widget, col, row, 1, 1);
        }
    } else {
        GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request(scrolled, 400, 200);
        gtk_box_pack_start(GTK_BOX(ws_container), scrolled, TRUE, TRUE, 0);

        GtkWidget *text_view = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
        gtk_container_add(GTK_CONTAINER(scrolled), text_view);

        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
        GString *display_text = g_string_new("");

        for (int i = 0; i < workspace_count; i++) {
            const char *workspace_name = get_workspace_name_or_default(names, i);
            if (i == user_current_desktop) {
                g_string_append_printf(display_text, "◆%d◆ %s (source)\n", i + 1, workspace_name);
            } else {
                g_string_append_printf(display_text, "[%d] %s\n", i + 1, workspace_name);
            }
        }

        gtk_text_buffer_set_text(buffer, display_text->str, -1);
        g_string_free(display_text, TRUE);
    }

    free_workspace_names(names);

    // Instructions
    create_workspace_instructions(parent_container);
}

// Handle key press events for workspace move all overlay
gboolean handle_workspace_move_all_key_press(AppData *app, GdkEventKey *event) {
    int target = resolve_workspace_from_key(app->display, event, app->config.workspaces_per_row);
    if (target < 0) return FALSE;

    int current = get_current_desktop(app->display);
    if (target == current) {
        hide_window(app);
        return TRUE;
    }

    for (int i = 0; i < app->windows_to_move_count; i++)
        move_window_to_desktop(app->display, app->windows_to_move[i], target);
    switch_to_desktop(app->display, target);

    log_info("USER: Moved %d windows from workspace %d to %d",
             app->windows_to_move_count, current + 1, target + 1);
    hide_window(app);
    return TRUE;
}