#include "overlay_projects.h"

#include <string.h>

#include "display.h"
#include "gtk_utils.h"
#include "log.h"
#include "overlay_manager.h"
#include "selection.h"
#include "projects.h"
#include "projects_remote_scope.h"
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

static void update_project_new_labels(AppData *app) {
    GtkWidget *title = g_object_get_data(G_OBJECT(app->dialog_container), "title_label");
    GtkWidget *backend_tabs = g_object_get_data(G_OBJECT(app->dialog_container), "backend_tabs_label");
    GtkWidget *instructions = g_object_get_data(G_OBJECT(app->dialog_container), "instructions_label");
    char backend_text[64];
    char instruction_text[160];

    if (app->project_new.backend == PROJECT_BACKEND_ZELLIJ) {
        g_strlcpy(backend_text, " Tmux   [ZELLIJ]", sizeof(backend_text));
    } else {
        g_strlcpy(backend_text, "[TMUX]   Zellij ", sizeof(backend_text));
    }
    g_snprintf(instruction_text, sizeof(instruction_text),
               "Enter=create in %s  Tab=toggle  Esc=cancel",
               app->project_new.start_dir[0] ? "folder" : "home");

    if (title) {
        gtk_label_set_text(GTK_LABEL(title), "New session");
    }
    if (backend_tabs) {
        gtk_label_set_text(GTK_LABEL(backend_tabs), backend_text);
    }
    if (instructions) {
        gtk_label_set_text(GTK_LABEL(instructions), instruction_text);
    }
}

static gboolean on_project_new_entry_key_press(GtkWidget *widget,
                                               GdkEventKey *event,
                                               gpointer user_data) {
    (void)widget;
    AppData *app = user_data;
    if (event->keyval != GDK_KEY_Tab && event->keyval != GDK_KEY_ISO_Left_Tab) {
        return FALSE;
    }

    app->project_new.backend = app->project_new.backend == PROJECT_BACKEND_ZELLIJ
        ? PROJECT_BACKEND_TMUX
        : PROJECT_BACKEND_ZELLIJ;
    update_project_new_labels(app);
    return TRUE;
}

void create_project_kill_overlay_content(GtkWidget *parent_container, AppData *app) {
    GtkWidget *vbox = create_session_form_box(parent_container);

    const char *title = "Delete?";
    const char *info_text = "";
    const char *instructions = "Y or Delete = confirm, N or Esc = cancel";
    char info[1024];
    info[0] = '\0';

    if (app->project_kill.action == PROJECT_DELETE_KILL_SESSION) {
        title = "Kill Session?";
        g_snprintf(info, sizeof(info), "Session: %s", app->project_kill.session_name);
        info_text = info;
        instructions = "Y or Delete = kill, N or Esc = cancel";
    } else if (app->project_kill.action == PROJECT_DELETE_FORGET_REMOTE) {
        title = "Forget Remote Entry?";
        g_snprintf(info, sizeof(info), "Entry: [REMOTE:%s] %s",
                   app->project_kill.remote_host,
                   app->project_kill.session_name);
        info_text = info;
    } else if (app->project_kill.action == PROJECT_DELETE_REMOVE_FOLDER) {
        title = "Remove Folder From zoxide?";
        g_snprintf(info, sizeof(info), "Folder: %s", app->project_kill.folder_path);
        info_text = info;
    }

    GtkWidget *title_label = gtk_label_new(title);
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    GtkWidget *info_label = gtk_label_new(info_text);
    gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

    GtkWidget *inst = create_centered_label(instructions);
    gtk_widget_set_opacity(inst, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst, FALSE, FALSE, 0);
}

void create_project_rename_overlay_content(GtkWidget *parent_container, AppData *app) {
    create_session_entry_form(parent_container, "Rename Session",
                           app->project_rename.session_name,
                           "Enter=rename  Esc=cancel");
}

void create_project_new_overlay_content(GtkWidget *parent_container, AppData *app) {
    GtkWidget *vbox = create_session_form_box(parent_container);

    GtkWidget *title_label = gtk_label_new("");
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    GtkWidget *backend_tabs = create_centered_label("");
    PangoFontDescription *tab_font = pango_font_description_from_string("monospace 12");
    gtk_widget_override_font(backend_tabs, tab_font);
    pango_font_description_free(tab_font);
    gtk_box_pack_start(GTK_BOX(vbox), backend_tabs, FALSE, FALSE, 0);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), app->project_new.session_name);
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
    gtk_widget_set_size_request(entry, 360, -1);
    gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);

    GtkWidget *inst = create_centered_label("");
    gtk_widget_set_opacity(inst, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(parent_container), "name_entry", entry);
    g_object_set_data(G_OBJECT(parent_container), "title_label", title_label);
    g_object_set_data(G_OBJECT(parent_container), "backend_tabs_label", backend_tabs);
    g_object_set_data(G_OBJECT(parent_container), "instructions_label", inst);
    g_signal_connect(entry, "key-press-event",
                     G_CALLBACK(on_project_new_entry_key_press), app);
    update_project_new_labels(app);
}

void create_project_remote_host_overlay_content(GtkWidget *parent_container, AppData *app) {
    create_session_entry_form(parent_container, "Remote Host",
                              app->project_remote.host,
                              "Enter=fetch remote sessions/folders  Esc=cancel");
}

static void refresh_projects_tab(AppData *app) {
    projects_refresh(app);
    validate_selection(app);
    update_scroll_position(app);
    update_display(app);
}

gboolean handle_project_kill_key_press(AppData *app, GdkEventKey *event) {
    gboolean confirm = event->keyval == GDK_KEY_y || event->keyval == GDK_KEY_Y ||
                       event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete;
    if (confirm) {
        CofiActionStatus status = COFI_ACTION_ERROR;
        if (app->project_kill.action == PROJECT_DELETE_KILL_SESSION) {
            char session_name[MAX_PROJECT_SESSION_NAME_LEN];
            ProjectBackend backend = app->project_kill.backend;
            g_strlcpy(session_name, app->project_kill.session_name, sizeof(session_name));
            status = projects_kill_session(app, session_name, backend);
        } else if (app->project_kill.action == PROJECT_DELETE_FORGET_REMOTE) {
            gboolean removed = projects_forget_remote_entry(app->project_kill.remote_host,
                                                            app->project_kill.backend,
                                                            app->project_kill.session_name,
                                                            app->project_kill.remote_cwd);
            status = removed ? COFI_HANDLED_REFRESH : COFI_ACTION_ERROR;
        } else if (app->project_kill.action == PROJECT_DELETE_REMOVE_FOLDER) {
            status = projects_remove_folder_entry(app,
                                                  app->project_kill.folder_path,
                                                  app->project_kill.folder_is_remote,
                                                  app->project_kill.remote_host);
        }
        hide_overlay(app);
        if (status == COFI_HANDLED_REFRESH) {
            refresh_projects_tab(app);
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

gboolean handle_project_rename_key_press(AppData *app, GdkEventKey *event) {
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

    char old_name[MAX_PROJECT_SESSION_NAME_LEN];
    g_strlcpy(old_name, app->project_rename.session_name, sizeof(old_name));
    CofiActionStatus status = projects_rename_tmux_session(app, old_name, new_name);
    hide_overlay(app);
    if (status == COFI_HANDLED_REFRESH) {
        refresh_projects_tab(app);
    } else {
        update_display(app);
    }
    return TRUE;
}

gboolean handle_project_new_key_press(AppData *app, GdkEventKey *event) {
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

    CofiActionStatus status = projects_new_session(app, session_name,
                                                   app->project_new.backend,
                                                   app->project_new.start_dir);
    hide_overlay(app);
    if (status == COFI_HANDLED_HIDE) {
        hide_window(app);
    } else {
        refresh_projects_tab(app);
    }
    return TRUE;
}

gboolean handle_project_remote_host_key_press(AppData *app, GdkEventKey *event) {
    if (event->keyval != GDK_KEY_Return && event->keyval != GDK_KEY_KP_Enter) {
        return FALSE;
    }

    GtkWidget *entry = g_object_get_data(G_OBJECT(app->dialog_container), "name_entry");
    if (!entry) {
        hide_overlay(app);
        return TRUE;
    }

    const char *host = gtk_entry_get_text(GTK_ENTRY(entry));
    if (!host || host[0] == '\0') {
        hide_overlay(app);
        return TRUE;
    }

    g_strlcpy(app->project_remote.host, host, sizeof(app->project_remote.host));
    projects_remote_scope_begin_fetch(app, host);
    hide_overlay(app);
    projects_refresh(app);
    reset_selection(app);
    update_scroll_position(app);
    update_display(app);
    return TRUE;
}
