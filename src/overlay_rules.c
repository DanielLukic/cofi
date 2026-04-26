#include "overlay_rules.h"

#include <string.h>

#include "command_parser.h"
#include "display.h"
#include "log.h"
#include "overlay_manager.h"
#include "selection.h"
#include "tab_switching.h"

static GtkWidget *create_message_label(const char *text) {
    GtkWidget *label = gtk_label_new(text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_opacity(label, 0.8);
    return label;
}

static gboolean validate_rule_commands(const char *commands, char *err, size_t err_size) {
    if (!commands || commands[0] == '\0') {
        g_snprintf(err, err_size, "commands required");
        return FALSE;
    }

    char local[512] = {0};
    char primary[128] = {0};
    char resolved[128] = {0};
    char arg[256] = {0};
    strncpy(local, commands, sizeof(local) - 1);

    char *cursor = local;
    char segment[256] = {0};
    while (next_command_segment(&cursor, segment, sizeof(segment))) {
        if (!parse_command_for_execution(segment, primary, arg, sizeof(primary), sizeof(arg))) {
            g_snprintf(err, err_size, "parse failed for '%s'", segment);
            return FALSE;
        }

        if (!resolve_command_primary(primary, resolved, sizeof(resolved))) {
            g_snprintf(err, err_size, "unknown command '%s'", segment);
            return FALSE;
        }
    }

    return TRUE;
}

static void refresh_rules_tab(AppData *app) {
    const char *filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
    filter_rules(app, filter);
    validate_selection(app);
    update_scroll_position(app);
    update_display(app);
}

static gboolean save_rule_values(AppData *app, int rule_index,
                                 const char *pattern, const char *commands) {
    GtkWidget *error_label = g_object_get_data(G_OBJECT(app->dialog_container), "error_label");
    if (!pattern || pattern[0] == '\0') {
        if (error_label) {
            gtk_label_set_text(GTK_LABEL(error_label), "Pattern required");
        }
        return FALSE;
    }

    char validation_error[128] = {0};
    if (!validate_rule_commands(commands, validation_error, sizeof(validation_error))) {
        log_warn("Rules: rejected invalid command string '%s' (%s)",
                 commands ? commands : "", validation_error);
        if (error_label) {
            char msg[160];
            g_snprintf(msg, sizeof(msg), "Invalid commands: %s", validation_error);
            gtk_label_set_text(GTK_LABEL(error_label), msg);
        }
        return FALSE;
    }

    if (rule_index < 0) {
        if (!add_rule(&app->rules_config, pattern, commands)) {
            if (error_label) {
                gtk_label_set_text(GTK_LABEL(error_label), "Cannot add: max rules reached");
            }
            return FALSE;
        }
    } else {
        g_strlcpy(app->rules_config.rules[rule_index].pattern,
                  pattern, sizeof(app->rules_config.rules[rule_index].pattern));
        g_strlcpy(app->rules_config.rules[rule_index].commands,
                  commands, sizeof(app->rules_config.rules[rule_index].commands));
    }

    save_rules_config(&app->rules_config);
    return TRUE;
}

static void create_rule_overlay_form(GtkWidget *parent_container, const char *title,
                                     const char *pattern, const char *commands,
                                     int rule_index) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title_label = gtk_label_new(title);
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    GtkWidget *pattern_label = create_message_label("Pattern (* wildcard, . single-char):");
    gtk_box_pack_start(GTK_BOX(vbox), pattern_label, FALSE, FALSE, 0);

    GtkWidget *pattern_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(pattern_entry), pattern ? pattern : "");
    gtk_widget_set_size_request(pattern_entry, 360, -1);
    gtk_box_pack_start(GTK_BOX(vbox), pattern_entry, FALSE, FALSE, 0);

    GtkWidget *commands_label = create_message_label("Commands (comma-separated cofi commands):");
    gtk_box_pack_start(GTK_BOX(vbox), commands_label, FALSE, FALSE, 0);

    GtkWidget *commands_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(commands_entry), commands ? commands : "");
    gtk_widget_set_size_request(commands_entry, 360, -1);
    gtk_box_pack_start(GTK_BOX(vbox), commands_entry, FALSE, FALSE, 0);

    GtkWidget *error_label = gtk_label_new("");
    gtk_widget_set_halign(error_label, GTK_ALIGN_START);
    gtk_widget_set_name(error_label, "overlay-error");
    gtk_box_pack_start(GTK_BOX(vbox), error_label, FALSE, FALSE, 0);

    GtkWidget *inst = create_message_label("Enter=save  Tab=next field  Esc=cancel");
    gtk_box_pack_start(GTK_BOX(vbox), inst, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(parent_container), "name_entry", pattern_entry);
    g_object_set_data(G_OBJECT(parent_container), "rule_pattern_entry", pattern_entry);
    g_object_set_data(G_OBJECT(parent_container), "rule_commands_entry", commands_entry);
    g_object_set_data(G_OBJECT(parent_container), "error_label", error_label);
    g_object_set_data(G_OBJECT(parent_container), "rule_index", GINT_TO_POINTER(rule_index));

    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
}

void create_rule_add_overlay_content(GtkWidget *parent_container, AppData *app) {
    (void)app;
    create_rule_overlay_form(parent_container, "Add Rule", "", "", -1);
}

void create_rule_edit_overlay_content(GtkWidget *parent_container, AppData *app) {
    if (app->current_tab != TAB_RULES || app->filtered_rules_count <= 0) {
        GtkWidget *error_label = gtk_label_new("No rule selected");
        gtk_box_pack_start(GTK_BOX(parent_container), error_label, FALSE, FALSE, 10);
        return;
    }

    int filtered_index = app->selection.rules_index;
    int config_index = app->filtered_rule_indices[filtered_index];
    Rule *rule = &app->rules_config.rules[config_index];

    create_rule_overlay_form(parent_container, "Edit Rule",
                             rule->pattern, rule->commands, config_index);
}

void create_rule_delete_overlay_content(GtkWidget *parent_container, AppData *app) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title_label = gtk_label_new("Delete Rule?");
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    if (app->rules_delete.rule_index >= 0 && app->rules_delete.rule_index < app->rules_config.count) {
        Rule *rule = &app->rules_config.rules[app->rules_delete.rule_index];
        char info[512];
        g_snprintf(info, sizeof(info), "Pattern: %s\nCommands: %s", rule->pattern, rule->commands);
        GtkWidget *info_label = gtk_label_new(info);
        gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
        gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);
    }

    GtkWidget *inst = create_message_label("Y or Ctrl+D = delete, N or Esc = cancel");
    gtk_box_pack_start(GTK_BOX(vbox), inst, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
}

static gboolean handle_rule_form_key_press(AppData *app, GdkEventKey *event) {
    if (event->keyval == GDK_KEY_Tab) {
        GtkWidget *pattern_entry = g_object_get_data(G_OBJECT(app->dialog_container), "rule_pattern_entry");
        GtkWidget *commands_entry = g_object_get_data(G_OBJECT(app->dialog_container), "rule_commands_entry");
        GtkWidget *focused = gtk_window_get_focus(GTK_WINDOW(app->window));

        if (focused == pattern_entry && commands_entry) {
            gtk_widget_grab_focus(commands_entry);
        } else if (pattern_entry) {
            gtk_widget_grab_focus(pattern_entry);
        }
        return TRUE;
    }

    if (event->keyval != GDK_KEY_Return && event->keyval != GDK_KEY_KP_Enter) {
        return FALSE;
    }

    GtkWidget *pattern_entry = g_object_get_data(G_OBJECT(app->dialog_container), "rule_pattern_entry");
    GtkWidget *commands_entry = g_object_get_data(G_OBJECT(app->dialog_container), "rule_commands_entry");
    int rule_index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(app->dialog_container), "rule_index"));

    if (!pattern_entry || !commands_entry) {
        return TRUE;
    }

    const char *pattern = gtk_entry_get_text(GTK_ENTRY(pattern_entry));
    const char *commands = gtk_entry_get_text(GTK_ENTRY(commands_entry));

    if (!save_rule_values(app, rule_index, pattern, commands)) {
        return TRUE;
    }

    hide_overlay(app);
    refresh_rules_tab(app);
    return TRUE;
}

gboolean handle_rule_add_key_press(AppData *app, GdkEventKey *event) {
    return handle_rule_form_key_press(app, event);
}

gboolean handle_rule_edit_key_press(AppData *app, GdkEventKey *event) {
    return handle_rule_form_key_press(app, event);
}

gboolean handle_rule_delete_key_press(AppData *app, GdkEventKey *event) {
    gboolean confirm = event->keyval == GDK_KEY_y || event->keyval == GDK_KEY_Y ||
                       ((event->state & GDK_CONTROL_MASK) &&
                        (event->keyval == GDK_KEY_d || event->keyval == GDK_KEY_D));
    if (confirm) {
        if (app->rules_delete.rule_index >= 0 && app->rules_delete.rule_index < app->rules_config.count) {
            remove_rule(&app->rules_config, app->rules_delete.rule_index);
            save_rules_config(&app->rules_config);
        }
        app->rules_delete.pending_delete = FALSE;
        app->rules_delete.rule_index = -1;
        hide_overlay(app);
        refresh_rules_tab(app);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_n || event->keyval == GDK_KEY_N) {
        app->rules_delete.pending_delete = FALSE;
        app->rules_delete.rule_index = -1;
        hide_overlay(app);
        return TRUE;
    }

    return FALSE;
}
