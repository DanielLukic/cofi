#include "rules/overlay_rules.h"

#include <string.h>

#include "commands/command_parser.h"
#include "ui/display.h"
#include "core/log/log.h"
#include "matching/match_entry_config.h"
#include "ui/overlay_confirm.h"
#include "ui/overlay_manager.h"
#include "rules/rules_provider.h"
#include "core/selection/selection.h"

static GtkWidget *create_message_label(const char *text) {
    GtkWidget *label = gtk_label_new(text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_opacity(label, 0.8);
    return label;
}

static int s_pending_rule_delete_index = -1;

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

    int match_id = matching_find_or_create_pattern_entry(&app->matching, pattern);
    if (match_id <= 0) {
        if (error_label) {
            gtk_label_set_text(GTK_LABEL(error_label), "Cannot create pattern entry");
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
        rule_index = app->rules_config.count - 1;
        app->rules_config.rules[rule_index].once = true;
        app->rules_config.rules[rule_index].applied = 0;
    } else {
        g_strlcpy(app->rules_config.rules[rule_index].pattern,
                  pattern, sizeof(app->rules_config.rules[rule_index].pattern));
        g_strlcpy(app->rules_config.rules[rule_index].commands,
                  commands, sizeof(app->rules_config.rules[rule_index].commands));
    }

    app->rules_config.rules[rule_index].match_id = match_id;
    save_match_entries(&app->matching);
    if (!save_rules_config(&app->rules_config, &app->matching)) {
        if (error_label) {
            gtk_label_set_text(GTK_LABEL(error_label), "Failed to save rules");
        }
        return FALSE;
    }
    return TRUE;
}

static gboolean save_rule_commands_only(AppData *app, int rule_index, const char *commands) {
    GtkWidget *error_label = g_object_get_data(G_OBJECT(app->dialog_container), "error_label");
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
    if (rule_index < 0 || rule_index >= app->rules_config.count) {
        if (error_label) {
            gtk_label_set_text(GTK_LABEL(error_label), "No rule selected");
        }
        return FALSE;
    }
    g_strlcpy(app->rules_config.rules[rule_index].commands,
              commands, sizeof(app->rules_config.rules[rule_index].commands));
    if (!save_rules_config(&app->rules_config, &app->matching)) {
        if (error_label) {
            gtk_label_set_text(GTK_LABEL(error_label), "Failed to save rules");
        }
        return FALSE;
    }
    return TRUE;
}

static void create_rule_add_overlay_form(GtkWidget *parent_container,
                                         const char *pattern, const char *commands) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title_label = gtk_label_new("Add Rule");
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
    g_object_set_data(G_OBJECT(parent_container), "rule_index", GINT_TO_POINTER(-1));

    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
}

static void create_rule_edit_overlay_form(GtkWidget *parent_container,
                                          const char *commands, int rule_index) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title_label = gtk_label_new("Edit Commands");
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

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

    GtkWidget *inst = create_message_label("Enter=save  Esc=cancel");
    gtk_box_pack_start(GTK_BOX(vbox), inst, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(parent_container), "name_entry", commands_entry);
    g_object_set_data(G_OBJECT(parent_container), "rule_pattern_entry", NULL);
    g_object_set_data(G_OBJECT(parent_container), "rule_commands_entry", commands_entry);
    g_object_set_data(G_OBJECT(parent_container), "error_label", error_label);
    g_object_set_data(G_OBJECT(parent_container), "rule_index", GINT_TO_POINTER(rule_index));

    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
}

void create_rule_add_overlay_content(GtkWidget *parent_container, AppData *app) {
    (void)app;
    create_rule_add_overlay_form(parent_container, "", "");
}

void create_rule_edit_overlay_content(GtkWidget *parent_container, AppData *app) {
    int config_index = rules_selected_config_index(app);
    if (app->current_tab != rules_tab_mode() || config_index < 0) {
        GtkWidget *error_label = gtk_label_new("No rule selected");
        gtk_box_pack_start(GTK_BOX(parent_container), error_label, FALSE, FALSE, 10);
        return;
    }

    Rule *rule = &app->rules_config.rules[config_index];

    create_rule_edit_overlay_form(parent_container, rule->commands, config_index);
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

    GtkWidget *commands_entry = g_object_get_data(G_OBJECT(app->dialog_container), "rule_commands_entry");
    int rule_index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(app->dialog_container), "rule_index"));

    if (!commands_entry) {
        return TRUE;
    }

    const char *commands = gtk_entry_get_text(GTK_ENTRY(commands_entry));

    gboolean ok = FALSE;
    if (rule_index < 0) {
        GtkWidget *pattern_entry = g_object_get_data(G_OBJECT(app->dialog_container), "rule_pattern_entry");
        const char *pattern = pattern_entry ? gtk_entry_get_text(GTK_ENTRY(pattern_entry)) : "";
        ok = save_rule_values(app, rule_index, pattern, commands);
    } else {
        ok = save_rule_commands_only(app, rule_index, commands);
    }
    if (!ok) {
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

static void perform_rule_delete(AppData *app) {
    if (s_pending_rule_delete_index >= 0 && s_pending_rule_delete_index < app->rules_config.count) {
        remove_rule(&app->rules_config, s_pending_rule_delete_index);
        save_rules_config(&app->rules_config, &app->matching);
    }
    s_pending_rule_delete_index = -1;
    refresh_rules_tab(app);
}

void show_rule_delete_confirm(AppData *app, int rule_index) {
    s_pending_rule_delete_index = rule_index;

    char info[2048];
    if (rule_index >= 0 && rule_index < app->rules_config.count) {
        Rule *rule = &app->rules_config.rules[rule_index];
        char *escaped_pattern = g_markup_escape_text(rule->pattern, -1);
        char *escaped_commands = g_markup_escape_text(rule->commands, -1);
        g_snprintf(info, sizeof(info),
                   "<b>Pattern:</b> %s\n<b>Commands:</b> %s",
                   escaped_pattern, escaped_commands);
        g_free(escaped_pattern);
        g_free(escaped_commands);
    } else {
        g_snprintf(info, sizeof(info), "<b>Rule:</b> (missing)");
    }

    show_confirm_overlay(app, "Delete Rule?", info, perform_rule_delete);
}
