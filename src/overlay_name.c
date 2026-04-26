#include "overlay_name.h"

#include <string.h>

#include "display.h"
#include "filter.h"
#include "filter_names.h"
#include "log.h"
#include "named_window.h"
#include "named_window_config.h"
#include "overlay_manager.h"

static gboolean focus_name_entry_timeout(gpointer user_data) {
    AppData *app = (AppData *)user_data;
    GtkWidget *name_entry = g_object_get_data(G_OBJECT(app->dialog_container), "name_entry");

    if (name_entry && gtk_widget_get_visible(name_entry)) {
        gtk_widget_grab_focus(name_entry);
        log_debug("Focused name entry widget");
    } else {
        log_debug("Name entry widget not found or not visible");
    }

    return FALSE;
}

void focus_name_entry_delayed(AppData *app) {
    g_timeout_add(50, focus_name_entry_timeout, app);
}

void create_name_assign_overlay_content(GtkWidget *parent_container, AppData *app) {
    if (app->current_tab != TAB_WINDOWS || app->filtered_count == 0) {
        GtkWidget *error_label = gtk_label_new("No window selected for name assignment");
        gtk_box_pack_start(GTK_BOX(parent_container), error_label, FALSE, FALSE, 10);
        return;
    }

    WindowInfo *selected = &app->filtered[app->selection.window_index];
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title_label = gtk_label_new("Assign Custom Name");
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    pango_attr_list_insert(attrs, pango_attr_scale_new(1.2));
    gtk_label_set_attributes(GTK_LABEL(title_label), attrs);
    pango_attr_list_unref(attrs);
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    char window_info[512];
    snprintf(window_info, sizeof(window_info), "Window: %s [%s]",
             selected->title, selected->class_name);
    GtkWidget *info_label = gtk_label_new(window_info);
    gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

    GtkWidget *name_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(name_entry), "Enter custom name...");
    gtk_widget_set_size_request(name_entry, 300, -1);
    gtk_box_pack_start(GTK_BOX(vbox), name_entry, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(parent_container), "name_entry", name_entry);

    GtkWidget *inst_label = gtk_label_new("Press Enter to assign name, Escape to cancel");
    gtk_widget_set_opacity(inst_label, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
    log_info("Name assignment overlay created for window: %s", selected->title);
}

void create_name_edit_overlay_content(GtkWidget *parent_container, AppData *app) {
    if (app->current_tab != TAB_NAMES || app->filtered_names_count == 0) {
        GtkWidget *error_label = gtk_label_new("No named window selected for editing");
        gtk_box_pack_start(GTK_BOX(parent_container), error_label, FALSE, FALSE, 10);
        return;
    }

    NamedWindow *selected = &app->filtered_names[app->selection.names_index];
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title_label = gtk_label_new("Edit Custom Name");
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    char window_info[512];
    snprintf(window_info, sizeof(window_info), "Window: %s [%s]",
             selected->original_title, selected->class_name);
    GtkWidget *info_label = gtk_label_new(window_info);
    gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

    GtkWidget *name_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(name_entry), selected->custom_name);
    gtk_editable_select_region(GTK_EDITABLE(name_entry), 0, -1);
    gtk_widget_set_size_request(name_entry, 300, -1);
    gtk_box_pack_start(GTK_BOX(vbox), name_entry, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(parent_container), "name_entry", name_entry);
    g_object_set_data(G_OBJECT(parent_container), "named_window_index",
                      GINT_TO_POINTER(app->selection.names_index));

    GtkWidget *inst_label = gtk_label_new("Press Enter to save changes, Escape to cancel");
    gtk_widget_set_opacity(inst_label, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
    log_info("Name edit overlay created for window: %s", selected->original_title);
}

void create_name_delete_overlay_content(GtkWidget *parent_container, AppData *app) {
    log_info("Name delete overlay content created (pending=%d, mgr_idx=%d, name='%s')",
             app->name_delete.pending_delete,
             app->name_delete.manager_index,
             app->name_delete.custom_name);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title_label = gtk_label_new("Delete Custom Name?");
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    char info[512];
    snprintf(info, sizeof(info), "Name: %s", app->name_delete.custom_name);
    GtkWidget *info_label = gtk_label_new(info);
    gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

    GtkWidget *inst_label = gtk_label_new("Press Y or Ctrl+D to confirm, N or Esc to cancel");
    gtk_widget_set_opacity(inst_label, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
}

static void clear_name_delete_state(AppData *app) {
    app->name_delete.pending_delete = FALSE;
    app->name_delete.manager_index = -1;
    app->name_delete.custom_name[0] = '\0';
}

gboolean handle_name_assign_key_press(AppData *app, GdkEventKey *event) {
    if (event->keyval != GDK_KEY_Return && event->keyval != GDK_KEY_KP_Enter) {
        return FALSE;
    }

    GtkWidget *name_entry = g_object_get_data(G_OBJECT(app->dialog_container), "name_entry");
    if (!name_entry) {
        log_error("Name entry widget not found");
        hide_overlay(app);
        return TRUE;
    }

    const char *custom_name = gtk_entry_get_text(GTK_ENTRY(name_entry));
    if (!custom_name || strlen(custom_name) == 0) {
        log_info("Empty name entered, canceling assignment");
        hide_overlay(app);
        return TRUE;
    }

    if (app->current_tab != TAB_WINDOWS || app->filtered_count == 0) {
        log_error("No window selected for name assignment");
        hide_overlay(app);
        return TRUE;
    }

    WindowInfo *selected = &app->filtered[app->selection.window_index];
    assign_custom_name(&app->names, selected, custom_name);
    save_named_windows(&app->names);
    log_info("Assigned custom name '%s' to window: %s", custom_name, selected->title);

    hide_overlay(app);
    if (app->current_tab == TAB_WINDOWS) {
        const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
        filter_windows(app, current_filter);
        update_display(app);
    }

    return TRUE;
}

gboolean handle_name_edit_key_press(AppData *app, GdkEventKey *event) {
    if (event->keyval != GDK_KEY_Return && event->keyval != GDK_KEY_KP_Enter) {
        return FALSE;
    }

    GtkWidget *name_entry = g_object_get_data(G_OBJECT(app->dialog_container), "name_entry");
    int named_index = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(app->dialog_container), "named_window_index"));

    if (!name_entry) {
        log_error("Name entry widget not found");
        hide_overlay(app);
        return TRUE;
    }

    const char *new_name = gtk_entry_get_text(GTK_ENTRY(name_entry));
    if (!new_name || strlen(new_name) == 0) {
        log_info("Empty name entered, canceling edit");
        hide_overlay(app);
        return TRUE;
    }

    if (named_index < 0 || named_index >= app->filtered_names_count) {
        log_error("Invalid named window index: %d", named_index);
        hide_overlay(app);
        return TRUE;
    }

    NamedWindow *named_window = &app->filtered_names[named_index];
    int manager_index = find_named_window_index(&app->names, named_window->id);
    if (manager_index < 0) {
        manager_index = find_named_window_by_name(&app->names, named_window->custom_name);
    }
    if (manager_index < 0) {
        log_error("Named window not found in manager");
        hide_overlay(app);
        return TRUE;
    }

    update_custom_name(&app->names, manager_index, new_name);
    save_named_windows(&app->names);

    const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
    filter_names(app, current_filter);

    hide_overlay(app);
    update_display(app);

    log_info("USER: Updated custom name to '%s'", new_name);
    return TRUE;
}

gboolean handle_name_delete_key_press(AppData *app, GdkEventKey *event) {
    gboolean is_confirm =
        event->keyval == GDK_KEY_y || event->keyval == GDK_KEY_Y ||
        ((event->state & GDK_CONTROL_MASK) &&
         (event->keyval == GDK_KEY_d || event->keyval == GDK_KEY_D));

    if (is_confirm) {
        int manager_index = app->name_delete.manager_index;
        if (manager_index < 0 || manager_index >= app->names.count) {
            manager_index = find_named_window_by_name(
                &app->names, app->name_delete.custom_name);
        }

        if (manager_index < 0 || manager_index >= app->names.count) {
            log_warn("Delete target unresolved for '%s'", app->name_delete.custom_name);
        } else {
            delete_custom_name(&app->names, manager_index);
            save_named_windows(&app->names);
            log_info("USER: Deleted custom name '%s'", app->name_delete.custom_name);
        }

        const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
        filter_names(app, current_filter);
        if (app->selection.names_index >= app->filtered_names_count && app->filtered_names_count > 0) {
            app->selection.names_index = app->filtered_names_count - 1;
        }
        clear_name_delete_state(app);
        hide_overlay(app);
        update_display(app);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_n || event->keyval == GDK_KEY_N) {
        const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
        filter_names(app, current_filter);
        if (app->selection.names_index >= app->filtered_names_count && app->filtered_names_count > 0) {
            app->selection.names_index = app->filtered_names_count - 1;
        }
        clear_name_delete_state(app);
        hide_overlay(app);
        update_display(app);
        return TRUE;
    }

    return FALSE;
}
