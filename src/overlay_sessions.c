#include "overlay_sessions.h"

#include <string.h>

#include "display.h"
#include "gtk_utils.h"
#include "log.h"
#include "overlay_manager.h"
#include "selection.h"
#include "sessions.h"
#include "window_lifecycle.h"

static GtkWidget *create_session_form_box(GtkWidget *parent_container) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);
    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
    return vbox;
}

static GtkWidget *create_session_entry_form(GtkWidget *parent_container,
                                         const char *title,
                                         const char *initial_text,
                                         const char *instructions) {
    GtkWidget *vbox = create_session_form_box(parent_container);

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
    return entry;
}

static const char *session_backend_name(SessionBackend backend) {
    return backend == SESSION_BACKEND_ZELLIJ ? "zellij" : "tmux";
}

static void update_session_new_labels(AppData *app) {
    GtkWidget *title = g_object_get_data(G_OBJECT(app->dialog_container), "title_label");
    GtkWidget *instructions = g_object_get_data(G_OBJECT(app->dialog_container), "instructions_label");
    char title_text[64];
    char instruction_text[160];
    const char *backend = session_backend_name(app->session_new.backend);

    g_snprintf(title_text, sizeof(title_text), "New %s session", backend);
    g_snprintf(instruction_text, sizeof(instruction_text),
               "Enter=create in %s  Tab=%s  Esc=cancel",
               app->session_new.start_dir[0] ? "folder" : "home",
               app->session_new.backend == SESSION_BACKEND_ZELLIJ ? "tmux" : "zellij");

    if (title) {
        gtk_label_set_text(GTK_LABEL(title), title_text);
    }
    if (instructions) {
        gtk_label_set_text(GTK_LABEL(instructions), instruction_text);
    }
}

static gboolean on_session_new_entry_key_press(GtkWidget *widget,
                                               GdkEventKey *event,
                                               gpointer user_data) {
    (void)widget;
    AppData *app = user_data;
    if (event->keyval != GDK_KEY_Tab && event->keyval != GDK_KEY_ISO_Left_Tab) {
        return FALSE;
    }

    app->session_new.backend = app->session_new.backend == SESSION_BACKEND_ZELLIJ
        ? SESSION_BACKEND_TMUX
        : SESSION_BACKEND_ZELLIJ;
    update_session_new_labels(app);
    return TRUE;
}

void create_session_kill_overlay_content(GtkWidget *parent_container, AppData *app) {
    GtkWidget *vbox = create_session_form_box(parent_container);

    GtkWidget *title_label = gtk_label_new("Kill Session?");
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    char info[512];
    g_snprintf(info, sizeof(info), "Session: %s", app->session_kill.session_name);
    GtkWidget *info_label = gtk_label_new(info);
    gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

    GtkWidget *inst = create_centered_label("Y or Delete = kill, N or Esc = cancel");
    gtk_widget_set_opacity(inst, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst, FALSE, FALSE, 0);
}

void create_session_rename_overlay_content(GtkWidget *parent_container, AppData *app) {
    create_session_entry_form(parent_container, "Rename Session",
                           app->session_rename.session_name,
                           "Enter=rename  Esc=cancel");
}

void create_session_new_overlay_content(GtkWidget *parent_container, AppData *app) {
    GtkWidget *vbox = create_session_form_box(parent_container);

    GtkWidget *title_label = gtk_label_new("");
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), app->session_new.session_name);
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
    gtk_widget_set_size_request(entry, 360, -1);
    gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);

    GtkWidget *inst = create_centered_label("");
    gtk_widget_set_opacity(inst, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(parent_container), "name_entry", entry);
    g_object_set_data(G_OBJECT(parent_container), "title_label", title_label);
    g_object_set_data(G_OBJECT(parent_container), "instructions_label", inst);
    g_signal_connect(entry, "key-press-event",
                     G_CALLBACK(on_session_new_entry_key_press), app);
    update_session_new_labels(app);
}

static void refresh_sessions_tab(AppData *app) {
    sessions_refresh(app);
    validate_selection(app);
    update_scroll_position(app);
    update_display(app);
}

gboolean handle_session_kill_key_press(AppData *app, GdkEventKey *event) {
    gboolean confirm = event->keyval == GDK_KEY_y || event->keyval == GDK_KEY_Y ||
                       event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete;
    if (confirm) {
        char session_name[MAX_SESSION_NAME_LEN];
        SessionBackend backend = app->session_kill.backend;
        g_strlcpy(session_name, app->session_kill.session_name, sizeof(session_name));
        CofiActionStatus status = sessions_kill_session(app, session_name, backend);
        hide_overlay(app);
        if (status == COFI_HANDLED_REFRESH) {
            refresh_sessions_tab(app);
        } else {
            update_display(app);
        }
        return TRUE;
    }

    if (event->keyval == GDK_KEY_n || event->keyval == GDK_KEY_N) {
        hide_overlay(app);
        update_display(app);
        return TRUE;
    }

    return FALSE;
}

gboolean handle_session_rename_key_press(AppData *app, GdkEventKey *event) {
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

    char old_name[MAX_SESSION_NAME_LEN];
    g_strlcpy(old_name, app->session_rename.session_name, sizeof(old_name));
    CofiActionStatus status = sessions_rename_tmux_session(app, old_name, new_name);
    hide_overlay(app);
    if (status == COFI_HANDLED_REFRESH) {
        refresh_sessions_tab(app);
    } else {
        update_display(app);
    }
    return TRUE;
}

gboolean handle_session_new_key_press(AppData *app, GdkEventKey *event) {
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

    CofiActionStatus status = sessions_new_session(app, session_name,
                                                   app->session_new.backend,
                                                   app->session_new.start_dir);
    hide_overlay(app);
    if (status == COFI_HANDLED_HIDE) {
        hide_window(app);
    } else {
        refresh_sessions_tab(app);
    }
    return TRUE;
}
