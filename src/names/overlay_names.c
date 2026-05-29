#include "names/overlay_names.h"

#include <string.h>

#include "core/log/log.h"
#include "core/selection/selection.h"
#include "ui/window_filter.h"
#include "matching/match_entry.h"
#include "matching/match_entry_config.h"
#include "names/names_provider.h"
#include "names/names_store.h"
#include "ui/display.h"
#include "ui/overlay_confirm.h"
#include "ui/overlay_manager.h"

static int s_name_delete_match_id = -1;
static char s_name_delete_custom_name[MAX_TITLE_LEN] = {0};

static gboolean focus_name_entry_timeout(gpointer user_data) {
    AppData *app = (AppData *)user_data;
    GtkWidget *name_entry = g_object_get_data(G_OBJECT(app->dialog_container), "name_entry");
    if (name_entry && gtk_widget_get_visible(name_entry)) {
        gtk_widget_grab_focus(name_entry);
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
}

void create_name_edit_overlay_content(GtkWidget *parent_container, AppData *app) {
    NameRecord *selected = names_selected_record(app);
    if (app->current_tab != names_tab_mode() || !selected) {
        GtkWidget *error_label = gtk_label_new("No named window selected for editing");
        gtk_box_pack_start(GTK_BOX(parent_container), error_label, FALSE, FALSE, 10);
        return;
    }

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title_label = gtk_label_new("Edit Custom Name");
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    GtkWidget *name_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(name_entry), selected->custom_name);
    gtk_editable_select_region(GTK_EDITABLE(name_entry), 0, -1);
    gtk_widget_set_size_request(name_entry, 300, -1);
    gtk_box_pack_start(GTK_BOX(vbox), name_entry, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(parent_container), "name_entry", name_entry);

    GtkWidget *inst_label = gtk_label_new("Press Enter to save changes, Escape to cancel");
    gtk_widget_set_opacity(inst_label, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
}

gboolean handle_name_assign_key_press(AppData *app, GdkEventKey *event) {
    if (event->keyval != GDK_KEY_Return && event->keyval != GDK_KEY_KP_Enter) return FALSE;

    GtkWidget *name_entry = g_object_get_data(G_OBJECT(app->dialog_container), "name_entry");
    const char *custom_name = name_entry ? gtk_entry_get_text(GTK_ENTRY(name_entry)) : NULL;
    if (!custom_name || custom_name[0] == '\0' ||
        app->current_tab != TAB_WINDOWS || app->filtered_count == 0) {
        hide_overlay(app);
        return TRUE;
    }

    WindowInfo *selected = &app->filtered[app->selection.window_index];
    names_assign_window(app, selected, custom_name);
    hide_overlay(app);
    if (app->current_tab == TAB_WINDOWS) {
        const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
        preserve_selection(app);
        filter_windows(app, current_filter);
        restore_selection(app);
        update_display(app);
    }
    return TRUE;
}

gboolean handle_name_edit_key_press(AppData *app, GdkEventKey *event) {
    if (event->keyval != GDK_KEY_Return && event->keyval != GDK_KEY_KP_Enter) return FALSE;

    GtkWidget *name_entry = g_object_get_data(G_OBJECT(app->dialog_container), "name_entry");
    const char *new_name = name_entry ? gtk_entry_get_text(GTK_ENTRY(name_entry)) : NULL;
    int store_index = names_selected_store_index(app);
    if (!new_name || new_name[0] == '\0' || store_index < 0 || store_index >= app->names.count) {
        hide_overlay(app);
        return TRUE;
    }

    names_store_set(&app->names, app->names.records[store_index].match_id, new_name);
    names_store_save(&app->names);

    const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
    preserve_selection(app);
    names_on_query_changed(app, current_filter);
    restore_selection(app);
    hide_overlay(app);
    update_display(app);
    return TRUE;
}

static void clear_name_delete_state(void) {
    s_name_delete_match_id = -1;
    s_name_delete_custom_name[0] = '\0';
}

static void perform_name_delete(AppData *app) {
    int match_id = s_name_delete_match_id;
    if (match_id <= 0) {
        NameRecord *record = names_store_find_by_custom_name(&app->names, s_name_delete_custom_name);
        if (record) match_id = record->match_id;
    }

    if (match_id > 0 && names_store_remove_by_match_id(&app->names, match_id)) {
        names_store_save(&app->names);
        match_entry_delete_by_match_id(&app->matching, match_id);
        save_match_entries(&app->matching);
    } else {
        log_warn("Delete target unresolved for '%s'", s_name_delete_custom_name);
    }

    const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
    preserve_selection(app);
    names_on_query_changed(app, current_filter);
    restore_selection(app);
    clear_name_delete_state();
    update_display(app);
}

void show_name_delete_confirm(AppData *app, const char *custom_name, int match_id) {
    s_name_delete_match_id = match_id;
    g_strlcpy(s_name_delete_custom_name, custom_name ? custom_name : "",
              sizeof(s_name_delete_custom_name));

    char *escaped_name = g_markup_escape_text(s_name_delete_custom_name, -1);
    char info[1024];
    g_snprintf(info, sizeof(info), "<b>Name:</b> %s", escaped_name);
    g_free(escaped_name);
    show_confirm_overlay(app, "Delete Custom Name?", info, perform_name_delete);
}
