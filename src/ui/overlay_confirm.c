#include "ui/overlay_confirm.h"

#include "ui/gtk_utils.h"
#include "ui/overlay_manager.h"

static gboolean is_confirm_key(const GdkEventKey *event) {
    if (!event) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_y || event->keyval == GDK_KEY_Y ||
        event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete) {
        return TRUE;
    }

    if ((event->state & GDK_CONTROL_MASK) &&
        (event->keyval == GDK_KEY_d || event->keyval == GDK_KEY_D)) {
        return TRUE;
    }

    return FALSE;
}

static gboolean is_cancel_key(const GdkEventKey *event) {
    if (!event) {
        return FALSE;
    }

    return event->keyval == GDK_KEY_n || event->keyval == GDK_KEY_N ||
           event->keyval == GDK_KEY_Escape;
}

void clear_confirm_overlay_state(AppData *app) {
    if (!app) {
        return;
    }

    g_free(app->confirm_overlay.title);
    g_free(app->confirm_overlay.info);
    app->confirm_overlay.title = NULL;
    app->confirm_overlay.info = NULL;
    app->confirm_overlay.active = FALSE;
    app->confirm_overlay.on_confirm = NULL;
}

void show_confirm_overlay(AppData *app,
                          const char *title,
                          const char *info,
                          void (*on_confirm)(AppData *)) {
    if (!app) {
        return;
    }

    if (app->overlay_active) {
        hide_overlay(app);
    }

    clear_confirm_overlay_state(app);
    app->confirm_overlay.active = TRUE;
    app->confirm_overlay.title = g_strdup(title ? title : "");
    app->confirm_overlay.info = g_strdup(info ? info : "");
    app->confirm_overlay.on_confirm = on_confirm;

    show_overlay(app, OVERLAY_CONFIRM, NULL);
}

void create_confirm_overlay_content(GtkWidget *parent_container, AppData *app) {
    if (!parent_container || !app) {
        return;
    }

    char *header = g_strdup_printf("<b>%s</b>",
                                   app->confirm_overlay.title ? app->confirm_overlay.title : "");
    GtkWidget *header_label = create_markup_label(header, TRUE);
    g_free(header);
    gtk_box_pack_start(GTK_BOX(parent_container), header_label, FALSE, FALSE, 10);

    add_horizontal_separator(parent_container);

    GtkWidget *info_label = create_markup_label(app->confirm_overlay.info ? app->confirm_overlay.info : "", TRUE);
    gtk_box_pack_start(GTK_BOX(parent_container), info_label, FALSE, FALSE, 10);

    add_horizontal_separator(parent_container);

    GtkWidget *instructions = create_centered_label("[Y or Ctrl+D to confirm, N or Esc to cancel]");
    gtk_box_pack_start(GTK_BOX(parent_container), instructions, FALSE, FALSE, 10);
}

gboolean handle_confirm_overlay_key_press(AppData *app, GdkEventKey *event) {
    if (!app || !event) {
        return FALSE;
    }

    if (is_confirm_key(event)) {
        void (*on_confirm)(AppData *) = app->confirm_overlay.on_confirm;
        if (on_confirm) {
            on_confirm(app);
        }
        hide_overlay(app);
        return TRUE;
    }

    if (is_cancel_key(event)) {
        hide_overlay(app);
        return TRUE;
    }

    return FALSE;
}
