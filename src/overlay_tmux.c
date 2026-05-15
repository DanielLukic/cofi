#include "overlay_tmux.h"

#include <string.h>

#include "display.h"
#include "gtk_utils.h"
#include "log.h"
#include "overlay_manager.h"
#include "selection.h"
#include "tmux.h"
#include "window_lifecycle.h"

static GtkWidget *create_tmux_entry_form(GtkWidget *parent_container,
                                         const char *title,
                                         const char *initial_text,
                                         const char *instructions) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title_label = gtk_label_new(title);
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), initial_text ? initial_text : "");
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
    gtk_widget_set_size_request(entry, 360, -1);
    gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);

    GtkWidget *inst = create_centered_label(instructions);
    gtk_widget_set_opacity(inst, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(parent_container), "name_entry", entry);
    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
    return entry;
}

void create_tmux_kill_overlay_content(GtkWidget *parent_container, AppData *app) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title_label = gtk_label_new("Kill Session?");
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    char info[512];
    g_snprintf(info, sizeof(info), "Session: %s", app->tmux_kill.session_name);
    GtkWidget *info_label = gtk_label_new(info);
    gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

    GtkWidget *inst = create_centered_label("Y or Delete = kill, N or Esc = cancel");
    gtk_widget_set_opacity(inst, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
}

void create_tmux_rename_overlay_content(GtkWidget *parent_container, AppData *app) {
    create_tmux_entry_form(parent_container, "Rename Tmux Session",
                           app->tmux_rename.session_name,
                           "Enter=rename  Esc=cancel");
}

void create_tmux_new_overlay_content(GtkWidget *parent_container, AppData *app) {
    (void)app;
    create_tmux_entry_form(parent_container, "New Tmux Session", "",
                           "Enter=create in home  Esc=cancel");
}

static void refresh_tmux_tab(AppData *app) {
    tmux_refresh(app);
    validate_selection(app);
    update_scroll_position(app);
    update_display(app);
}

gboolean handle_tmux_kill_key_press(AppData *app, GdkEventKey *event) {
    gboolean confirm = event->keyval == GDK_KEY_y || event->keyval == GDK_KEY_Y ||
                       event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete;
    if (confirm) {
        char session_name[MAX_TMUX_SESSION_NAME_LEN];
        TmuxSessionBackend backend = app->tmux_kill.backend;
        g_strlcpy(session_name, app->tmux_kill.session_name, sizeof(session_name));
        CofiActionStatus status = tmux_kill_session(app, session_name, backend);
        app->tmux_kill.pending_kill = FALSE;
        app->tmux_kill.backend = TMUX_SESSION_TMUX;
        app->tmux_kill.session_name[0] = '\0';
        hide_overlay(app);
        if (status == COFI_HANDLED_REFRESH) {
            refresh_tmux_tab(app);
        } else {
            update_display(app);
        }
        return TRUE;
    }

    if (event->keyval == GDK_KEY_n || event->keyval == GDK_KEY_N) {
        app->tmux_kill.pending_kill = FALSE;
        app->tmux_kill.backend = TMUX_SESSION_TMUX;
        app->tmux_kill.session_name[0] = '\0';
        hide_overlay(app);
        update_display(app);
        return TRUE;
    }

    return FALSE;
}

gboolean handle_tmux_rename_key_press(AppData *app, GdkEventKey *event) {
    if (event->keyval != GDK_KEY_Return && event->keyval != GDK_KEY_KP_Enter) {
        return FALSE;
    }

    GtkWidget *entry = g_object_get_data(G_OBJECT(app->dialog_container), "name_entry");
    if (!entry) {
        hide_overlay(app);
        return TRUE;
    }

    const char *new_name = gtk_entry_get_text(GTK_ENTRY(entry));
    if (!new_name || new_name[0] == '\0') {
        hide_overlay(app);
        return TRUE;
    }

    char old_name[MAX_TMUX_SESSION_NAME_LEN];
    g_strlcpy(old_name, app->tmux_rename.session_name, sizeof(old_name));
    CofiActionStatus status = tmux_rename_session(app, old_name, new_name);
    app->tmux_rename.pending_rename = FALSE;
    app->tmux_rename.session_name[0] = '\0';
    hide_overlay(app);
    if (status == COFI_HANDLED_REFRESH) {
        refresh_tmux_tab(app);
    } else {
        update_display(app);
    }
    return TRUE;
}

gboolean handle_tmux_new_key_press(AppData *app, GdkEventKey *event) {
    if (event->keyval != GDK_KEY_Return && event->keyval != GDK_KEY_KP_Enter) {
        return FALSE;
    }

    GtkWidget *entry = g_object_get_data(G_OBJECT(app->dialog_container), "name_entry");
    if (!entry) {
        hide_overlay(app);
        return TRUE;
    }

    const char *session_name = gtk_entry_get_text(GTK_ENTRY(entry));
    if (!session_name || session_name[0] == '\0') {
        hide_overlay(app);
        return TRUE;
    }

    CofiActionStatus status = tmux_new_session(app, session_name);
    hide_overlay(app);
    if (status == COFI_HANDLED_HIDE) {
        hide_window(app);
    } else {
        refresh_tmux_tab(app);
    }
    return TRUE;
}
