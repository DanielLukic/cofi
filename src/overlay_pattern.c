#include "overlay_pattern.h"

#include <string.h>

#include "display.h"
#include "filter_matching.h"
#include "geom_provider.h"
#include "geom_rule_sync.h"
#include "harpoon_provider.h"
#include "log.h"
#include "match_entry.h"
#include "match_entry_config.h"
#include "matching_provider.h"
#include "overlay_manager.h"
#include "overlay_confirm.h"
#include "rules_provider.h"

static void show_pattern_edit_unavailable(AppData *app, const char *message) {
    if (!app) return;
    show_confirm_overlay(app, "Cannot Edit Pattern", message, NULL);
}

static void refresh_active_tab(AppData *app) {
    const char *query = app->entry ? gtk_entry_get_text(GTK_ENTRY(app->entry)) : "";
    if (app->current_tab == matching_tab_mode()) {
        filter_matching(app, query);
    } else if (app->current_tab == rules_tab_mode()) {
        filter_rules(app, query);
    } else if (app->current_tab == geom_tab_mode()) {
        geom_on_query_changed(app, query);
    } else if (app->current_tab == harpoon_tab_mode()) {
        filter_harpoon(app, query);
    }
}

static gboolean rules_config_has_reference(const RulesConfig *config, int match_id) {
    if (!config || match_id <= 0) return FALSE;
    for (int i = 0; i < config->count; i++) {
        if (config->rules[i].match_id == match_id) {
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean layout_store_has_reference(const LayoutStore *store, int match_id) {
    if (!store || match_id <= 0) return FALSE;
    for (int i = 0; i < store->count; i++) {
        if (store->records[i].match_id == match_id) {
            return TRUE;
        }
    }
    return FALSE;
}

int selected_match_id_for_pattern_edit(AppData *app) {
    if (!app) return 0;

    if (app->current_tab == matching_tab_mode()) {
        MatchEntry *entry = matching_selected_entry(app);
        return entry ? entry->match_id : 0;
    }
    if (app->current_tab == rules_tab_mode()) {
        Rule *rule = rules_selected_rule(app);
        return rule ? rule->match_id : 0;
    }
    if (app->current_tab == geom_tab_mode()) {
        if (app->filtered_geom_count <= 0) return 0;
        int idx = app->selection.provider_index;
        if (idx < 0) idx = 0;
        if (idx >= app->filtered_geom_count) idx = app->filtered_geom_count - 1;
        int layout_idx = app->filtered_geom[idx];
        if (layout_idx < 0 || layout_idx >= app->layouts.count) return 0;
        return app->layouts.records[layout_idx].match_id;
    }
    if (app->current_tab == harpoon_tab_mode()) {
        int actual_slot = -1;
        HarpoonSlot *slot = harpoon_selected_slot(app, &actual_slot);
        (void)actual_slot;
        return slot ? slot->match_id : 0;
    }
    return 0;
}

gboolean show_pattern_edit_overlay(AppData *app, int match_id, const char *context_line) {
    if (!app) return FALSE;

    if (match_id <= 0) {
        if (app->current_tab == rules_tab_mode()) {
            log_warn("Pattern edit blocked: selected rule is orphaned (match_id=0)");
            show_pattern_edit_unavailable(app, "Selected rule is orphaned and has no pattern target.");
        } else {
            log_warn("Pattern edit blocked: no selected match entry");
            show_pattern_edit_unavailable(app, "No matching entry is selected for pattern editing.");
        }
        return FALSE;
    }

    int idx = match_entry_find_index_by_match_id(&app->matching, match_id);
    if (idx < 0) {
        if (app->current_tab == rules_tab_mode()) {
            log_warn("Pattern edit blocked: orphaned rule match_id=%d", match_id);
            show_pattern_edit_unavailable(app, "Selected rule is orphaned and cannot edit a missing pattern.");
        } else {
            log_warn("Pattern edit blocked: missing match entry for match_id=%d", match_id);
            show_pattern_edit_unavailable(app, "The selected entry no longer exists.");
        }
        return FALSE;
    }

    app->pattern_edit.target_match_id = match_id;
    g_strlcpy(app->pattern_edit.context_line, context_line ? context_line : "",
              sizeof(app->pattern_edit.context_line));
    show_overlay(app, OVERLAY_MATCH_PATTERN_EDIT, NULL);
    return TRUE;
}

void create_pattern_edit_overlay_content(GtkWidget *parent_container, AppData *app) {
    if (!parent_container || !app) return;

    int match_id = app->pattern_edit.target_match_id;
    int idx = match_entry_find_index_by_match_id(&app->matching, match_id);
    if (match_id <= 0 || idx < 0) {
        GtkWidget *error_label = gtk_label_new("No matching entry selected for pattern editing");
        gtk_box_pack_start(GTK_BOX(parent_container), error_label, FALSE, FALSE, 10);
        return;
    }

    MatchEntry *selected = &app->matching.entries[idx];

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_left(vbox, 20);
    gtk_widget_set_margin_right(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *title_label = gtk_label_new("Edit Match Pattern");
    gtk_widget_set_name(title_label, "overlay-title");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);

    if (app->pattern_edit.context_line[0]) {
        GtkWidget *info_label = gtk_label_new(app->pattern_edit.context_line);
        gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
        gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);
    }

    GtkWidget *name_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(name_entry), selected->original_title);
    gtk_editable_select_region(GTK_EDITABLE(name_entry), 0, -1);
    gtk_widget_set_size_request(name_entry, 360, -1);
    gtk_box_pack_start(GTK_BOX(vbox), name_entry, FALSE, FALSE, 0);

    char anchors[512];
    g_snprintf(anchors, sizeof(anchors), "Class: %s\nInstance: %s\nType: %s",
               selected->class_name[0] ? selected->class_name : "(any)",
               selected->instance[0] ? selected->instance : "(any)",
               selected->type[0] ? selected->type : "(any)");
    GtkWidget *anchors_label = gtk_label_new(anchors);
    gtk_widget_set_halign(anchors_label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(anchors_label), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(anchors_label), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), anchors_label, FALSE, FALSE, 0);

    GtkWidget *inst_label = gtk_label_new("Pattern (* wildcard, . single-char) — Enter to save, Esc to cancel");
    gtk_widget_set_opacity(inst_label, 0.7);
    gtk_box_pack_start(GTK_BOX(vbox), inst_label, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(parent_container), "name_entry", name_entry);
    gtk_box_pack_start(GTK_BOX(parent_container), vbox, TRUE, FALSE, 0);
}

gboolean handle_pattern_edit_key_press(AppData *app, GdkEventKey *event) {
    if (!app || !event) return FALSE;
    if (event->keyval != GDK_KEY_Return && event->keyval != GDK_KEY_KP_Enter) {
        return FALSE;
    }

    GtkWidget *name_entry = g_object_get_data(G_OBJECT(app->dialog_container), "name_entry");
    if (!name_entry) {
        log_error("Pattern edit widgets missing");
        hide_overlay(app);
        return TRUE;
    }

    const char *new_pattern = gtk_entry_get_text(GTK_ENTRY(name_entry));
    if (!new_pattern || new_pattern[0] == '\0') {
        log_info("Empty pattern entered, canceling pattern edit");
        hide_overlay(app);
        return TRUE;
    }

    int match_id = app->pattern_edit.target_match_id;
    int manager_index = match_entry_find_index_by_match_id(&app->matching, match_id);
    if (manager_index < 0 || manager_index >= app->matching.count) {
        log_error("Pattern edit target missing (match_id=%d)", match_id);
        hide_overlay(app);
        return TRUE;
    }

    MatchEntry *entry = &app->matching.entries[manager_index];
    char previous_pattern[MAX_TITLE_LEN] = {0};
    g_strlcpy(previous_pattern, entry->original_title, sizeof(previous_pattern));
    g_strlcpy(entry->original_title, new_pattern, sizeof(entry->original_title));

    save_match_entries(&app->matching);
    if (rules_config_has_reference(&app->rules_config, match_id)) {
        save_rules_config(&app->rules_config, &app->matching);
    }
    if (layout_store_has_reference(&app->layouts, match_id)) {
        if (previous_pattern[0] != '\0') {
            geom_rule_sync_for_pattern(app, previous_pattern);
        }
        geom_rule_sync_for_pattern(app, entry->original_title);
    }

    refresh_active_tab(app);
    hide_overlay(app);
    update_display(app);
    log_info("Updated match pattern for match_id=%d to '%s'",
             match_id, new_pattern);
    return TRUE;
}
