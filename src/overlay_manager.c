#include "overlay_manager.h"

#include "hotkeys.h"
#include "overlay_dispatch.h"
#include "overlay_harpoon.h"
#include "overlay_name.h"
extern void show_window(AppData *app);
static gboolean on_overlay_key_press(GtkWidget *widget,
                                     GdkEventKey *event,
                                     gpointer user_data) {
    (void)widget;
    return handle_overlay_key_press((AppData *)user_data, event);
}
static void clear_dialog_container(AppData *app) {
    gtk_container_foreach(GTK_CONTAINER(app->dialog_container),
                          (GtkCallback)gtk_widget_destroy, NULL);
}
static void set_main_focusability(AppData *app, gboolean can_focus) {
    if (app->entry) {
        gtk_widget_set_can_focus(app->entry, can_focus);
    }
    if (app->textview) {
        gtk_widget_set_can_focus(app->textview, can_focus);
    }
}
void init_overlay_system(AppData *app) {
    app->overlay_active = FALSE;
    app->current_overlay = OVERLAY_NONE;

    app->modal_background = gtk_event_box_new();
    gtk_widget_set_name(app->modal_background, "modal-background");
    gtk_widget_set_visible(app->modal_background, FALSE);
    gtk_widget_set_no_show_all(app->modal_background, TRUE);
    gtk_widget_set_can_focus(app->modal_background, TRUE);
    gtk_widget_add_events(app->modal_background,
                          GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);
    g_signal_connect(app->modal_background, "button-press-event",
                     G_CALLBACK(on_modal_background_button_press), app);
    g_signal_connect(app->modal_background, "key-press-event",
                     G_CALLBACK(on_overlay_key_press), app);

    app->dialog_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(app->dialog_container, "dialog-overlay");
    gtk_widget_set_visible(app->dialog_container, FALSE);
    gtk_widget_set_no_show_all(app->dialog_container, TRUE);
    gtk_widget_set_halign(app->dialog_container, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(app->dialog_container, GTK_ALIGN_CENTER);

    gtk_overlay_add_overlay(GTK_OVERLAY(app->main_overlay), app->modal_background);
    gtk_overlay_add_overlay(GTK_OVERLAY(app->main_overlay), app->dialog_container);
    gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(app->main_overlay),
                                         app->modal_background, TRUE);
}
void show_overlay(AppData *app, OverlayType type, gpointer data) {
    if (!app->window_visible) {
        show_window(app);
    }
    if (app->overlay_active) {
        hide_overlay(app);
    }

    clear_dialog_container(app);
    overlay_create_content(app, type, data);

    app->overlay_active = TRUE;
    app->current_overlay = type;

    gtk_widget_show(app->modal_background);
    gtk_widget_show(app->dialog_container);
    gtk_container_foreach(GTK_CONTAINER(app->dialog_container),
                          (GtkCallback)gtk_widget_show_all, NULL);
    gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(app->main_overlay),
                                         app->modal_background, FALSE);

    set_main_focusability(app, FALSE);
    if (type == OVERLAY_HARPOON_EDIT || type == OVERLAY_WORKSPACE_RENAME) {
        focus_harpoon_edit_entry_delayed(app);
        return;
    }
    if (overlay_should_focus_name_entry(type)) {
        focus_name_entry_delayed(app);
        return;
    }

    gtk_widget_grab_focus(app->modal_background);
    if (gtk_widget_get_realized(app->modal_background)) {
        gdk_window_focus(gtk_widget_get_window(app->modal_background),
                         GDK_CURRENT_TIME);
    }
}
void hide_overlay(AppData *app) {
    if (!app->overlay_active) {
        return;
    }

    gtk_widget_hide(app->modal_background);
    gtk_widget_hide(app->dialog_container);
    gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(app->main_overlay),
                                         app->modal_background, TRUE);
    clear_dialog_container(app);

    app->overlay_active = FALSE;
    app->current_overlay = OVERLAY_NONE;

    if (app->hotkey_capture_active) {
        app->hotkey_capture_active = FALSE;
        regrab_hotkeys(app);
    }

    set_main_focusability(app, TRUE);
    if (app->entry) {
        gtk_widget_grab_focus(app->entry);
    }
}

gboolean is_overlay_active(AppData *app) {
    return app->overlay_active;
}

gboolean handle_overlay_key_press(AppData *app, GdkEventKey *event) {
    if (!app->overlay_active) {
        return FALSE;
    }
    if (event->keyval == GDK_KEY_Escape) {
        hide_overlay(app);
        return TRUE;
    }

    gboolean handled = overlay_dispatch_key_press(app, event);
    if (handled && app->current_overlay == OVERLAY_WORKSPACE_RENAME) {
        hide_overlay(app);
    }
    return handled;
}

gboolean on_modal_background_button_press(GtkWidget *widget,
                                          GdkEventButton *event,
                                          gpointer user_data) {
    (void)widget;
    if (event->button == 1) {
        hide_overlay((AppData *)user_data);
        return TRUE;
    }
    return FALSE;
}
