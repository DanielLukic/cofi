#include "sessions/overlay_sessions.h"

#include "sessions/sessions_provider.h"
#include "ui/display.h"
#include "ui/gtk_utils.h"
#include "core/log/log.h"
#include "ui/overlay_confirm.h"
#include "ui/overlay_manager.h"

static GtkWidget *create_left_label(const char *text) {
    GtkWidget *label = gtk_label_new(text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    return label;
}

static char s_delete_source[16];
static char s_delete_session_id[SESSION_ID_LEN];
static char s_delete_path[SESSION_PATH_LEN];

void create_session_rename_overlay_content(GtkWidget *parent_container,
                                                 AppData *app) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title = gtk_label_new("Rename Session");
    gtk_widget_set_name(title, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    char info[512];
    g_snprintf(info, sizeof(info), "Session: %s",
               app->session_rename.session_id);
    GtkWidget *info_label = create_left_label(info);
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), app->session_rename.current_name);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Session name...");
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
    gtk_widget_set_size_request(entry, 360, -1);
    gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);

    GtkWidget *inst = create_centered_label("Enter=rename  Esc=cancel");
    gtk_widget_set_opacity(inst, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(parent_container), "name_entry", entry);
    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
}

static void perform_session_delete(AppData *app) {
    char path[SESSION_PATH_LEN];
    g_strlcpy(path, s_delete_path, sizeof(path));

    gboolean deleted = sessions_delete_path(path);
    if (deleted) {
        sessions_provider_remove_path(app, path);
    } else {
        log_warn("Session delete failed for '%s'", path);
        update_display(app);
    }
}

void show_session_delete_confirm(AppData *app,
                                 const char *source,
                                 const char *session_id,
                                 const char *path) {
    g_strlcpy(s_delete_source, source ? source : "", sizeof(s_delete_source));
    g_strlcpy(s_delete_session_id, session_id ? session_id : "", sizeof(s_delete_session_id));
    g_strlcpy(s_delete_path, path ? path : "", sizeof(s_delete_path));

    char *escaped_source = g_markup_escape_text(s_delete_source, -1);
    char *escaped_session = g_markup_escape_text(s_delete_session_id, -1);
    char *escaped_path = g_markup_escape_text(s_delete_path, -1);

    char info[2048];
    g_snprintf(info, sizeof(info),
               "<b>Source:</b> %s\n<b>Session:</b> %s\n<b>File:</b> %s",
               escaped_source, escaped_session, escaped_path);

    g_free(escaped_source);
    g_free(escaped_session);
    g_free(escaped_path);

    show_confirm_overlay(app, "Delete Session?", info, perform_session_delete);
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

    const char *name = gtk_entry_get_text(GTK_ENTRY(entry));
    char updated_name[SESSION_NAME_LEN];
    g_strlcpy(updated_name, name ? name : "", sizeof(updated_name));
    g_strstrip(updated_name);
    if (updated_name[0] == '\0') {
        hide_overlay(app);
        update_display(app);
        return TRUE;
    }

    SessionResult result = {0};
    g_strlcpy(result.source, app->session_rename.source, sizeof(result.source));
    g_strlcpy(result.session_id, app->session_rename.session_id,
              sizeof(result.session_id));
    g_strlcpy(result.path, app->session_rename.path, sizeof(result.path));

    char path[SESSION_PATH_LEN];
    g_strlcpy(path, result.path, sizeof(path));

    gboolean renamed = sessions_rename_result(&result, updated_name);
    hide_overlay(app);
    if (renamed) {
        sessions_provider_rename_path(app, path, updated_name);
    } else {
        log_warn("Session rename failed for '%s'", path);
        update_display(app);
    }
    return TRUE;
}
