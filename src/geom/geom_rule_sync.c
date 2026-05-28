#include "geom/geom_rule_sync.h"

#include <glib.h>
#include <stdbool.h>
#include <string.h>

#include "core/app/app_data.h"
#include "core/log/log.h"
#include "matching/match_entry_config.h"
#include "rules/rules_config.h"

static bool is_geom_restore_rule_for_match_id(const Rule *rule, int match_id) {
    if (!rule || match_id <= 0) {
        return false;
    }
    return strcmp(rule->tag, "geom") == 0 &&
           rule->match_id == match_id &&
           rule_commands_contain_segment(rule->commands, "rl");
}

static bool is_geom_restore_rule(const Rule *rule) {
    return rule &&
           strcmp(rule->tag, "geom") == 0 &&
           rule_commands_contain_segment(rule->commands, "rl");
}

static int find_geom_restore_rule_index(const RulesConfig *config, int match_id) {
    if (!config || match_id <= 0) {
        return -1;
    }
    for (int i = 0; i < config->count; i++) {
        if (is_geom_restore_rule_for_match_id(&config->rules[i], match_id)) {
            return i;
        }
    }
    return -1;
}

static const LayoutRecord *find_layout_record(const AppData *app, int match_id) {
    if (!app || match_id <= 0) {
        return NULL;
    }
    for (int i = 0; i < app->layouts.count; i++) {
        if (app->layouts.records[i].match_id == match_id) {
            return &app->layouts.records[i];
        }
    }
    return NULL;
}

static bool layout_is_enabled(const AppData *app, int match_id) {
    const LayoutRecord *record = find_layout_record(app, match_id);
    return record && !record->disabled;
}

int geom_rule_sync_for_layout(AppData *app, int match_id) {
    if (!app || match_id <= 0) {
        return 0;
    }

    const LayoutRecord *record = find_layout_record(app, match_id);
    int existing_idx = find_geom_restore_rule_index(&app->rules_config, match_id);
    bool enabled = record && !record->disabled;
    bool changed = false;

    int entry_idx = match_entry_find_index_by_match_id(&app->matching, match_id);
    const char *pattern = NULL;
    if (entry_idx >= 0 && app->matching.entries[entry_idx].original_title[0] != '\0') {
        pattern = app->matching.entries[entry_idx].original_title;
    }

    if (enabled && pattern && existing_idx < 0) {
        if (add_rule(&app->rules_config, pattern, "rl")) {
            Rule *new_rule = &app->rules_config.rules[app->rules_config.count - 1];
            new_rule->match_id = match_id;
            g_strlcpy(new_rule->tag, "geom", sizeof(new_rule->tag));
            changed = true;
            log_info("geom sync: created tagged restore rule for match_id=%d", match_id);
        } else {
            log_warn("geom sync: failed creating tagged restore rule for match_id=%d", match_id);
        }
    } else if (enabled && pattern && existing_idx >= 0) {
        Rule *rule = &app->rules_config.rules[existing_idx];
        if (strcmp(rule->pattern, pattern) != 0) {
            g_strlcpy(rule->pattern, pattern, sizeof(rule->pattern));
            changed = true;
            log_info("geom sync: updated tagged restore rule for match_id=%d", match_id);
        }
    } else if ((!enabled || !pattern) && existing_idx >= 0) {
        if (remove_rule(&app->rules_config, existing_idx)) {
            changed = true;
            log_info("geom sync: removed tagged restore rule for match_id=%d", match_id);
        } else {
            log_warn("geom sync: failed removing tagged restore rule for match_id=%d", match_id);
        }
    }

    if (changed) {
        save_match_entries(&app->matching);
        save_rules_config(&app->rules_config, &app->matching);
    }
    return changed ? 1 : 0;
}

static bool layout_pattern_matches(const AppData *app, int match_id, const char *pattern) {
    if (!app || match_id <= 0 || !pattern || pattern[0] == '\0') {
        return false;
    }
    int idx = match_entry_find_index_by_match_id(&app->matching, match_id);
    if (idx < 0) {
        return false;
    }
    return strcmp(app->matching.entries[idx].original_title, pattern) == 0;
}

int geom_rule_sync_for_pattern(AppData *app, const char *pattern) {
    if (!app || !pattern || pattern[0] == '\0') {
        return 0;
    }

    int changed = 0;
    for (int i = 0; i < app->layouts.count; i++) {
        int match_id = app->layouts.records[i].match_id;
        if (layout_pattern_matches(app, match_id, pattern)) {
            changed += geom_rule_sync_for_layout(app, match_id);
        }
    }

    for (int i = 0; i < app->rules_config.count; ) {
        Rule *rule = &app->rules_config.rules[i];
        if (is_geom_restore_rule(rule) && strcmp(rule->pattern, pattern) == 0) {
            int before_count = app->rules_config.count;
            changed += geom_rule_sync_for_layout(app, rule->match_id);
            if (app->rules_config.count < before_count) {
                continue;
            }
        }
        i++;
    }

    return changed;
}

int geom_rule_sync_all_layout_patterns(AppData *app) {
    if (!app) {
        return 0;
    }

    int changed = 0;
    for (int i = 0; i < app->rules_config.count; ) {
        Rule *rule = &app->rules_config.rules[i];
        if (is_geom_restore_rule(rule) && !layout_is_enabled(app, rule->match_id)) {
            if (remove_rule(&app->rules_config, i)) {
                changed++;
                continue;
            }
        }
        i++;
    }

    for (int i = 0; i < app->layouts.count; i++) {
        const LayoutRecord *record = &app->layouts.records[i];
        if (record->match_id <= 0) {
            continue;
        }
        changed += geom_rule_sync_for_layout(app, record->match_id);
    }

    if (changed > 0) {
        save_match_entries(&app->matching);
        save_rules_config(&app->rules_config, &app->matching);
    }
    return changed;
}
