#include "geom_rule_sync.h"

#include <glib.h>
#include <stdbool.h>
#include <string.h>

#include "app_data.h"
#include "log.h"
#include "match_entry_config.h"
#include "rules_config.h"

static bool is_geom_restore_rule(const Rule *rule, const char *pattern) {
    if (!rule || !pattern || pattern[0] == '\0') {
        return false;
    }
    return strcmp(rule->tag, "geom") == 0 &&
           strcmp(rule->pattern, pattern) == 0 &&
           rule_commands_contain_segment(rule->commands, "rl");
}

static int find_geom_restore_rule_index(const RulesConfig *config, const char *pattern) {
    if (!config || !pattern || pattern[0] == '\0') {
        return -1;
    }
    for (int i = 0; i < config->count; i++) {
        if (is_geom_restore_rule(&config->rules[i], pattern)) {
            return i;
        }
    }
    return -1;
}

static bool pattern_has_enabled_layout(const AppData *app, const char *pattern) {
    if (!app || !pattern || pattern[0] == '\0') {
        return false;
    }
    for (int i = 0; i < app->layouts.count; i++) {
        const LayoutRecord *record = &app->layouts.records[i];
        if (record->disabled || record->match_id <= 0) {
            continue;
        }
        int idx = match_entry_find_index_by_match_id(&app->matching, record->match_id);
        if (idx < 0) {
            continue;
        }
        if (strcmp(app->matching.entries[idx].original_title, pattern) == 0) {
            return true;
        }
    }
    return false;
}

int geom_rule_sync_for_pattern(AppData *app, const char *pattern) {
    if (!app || !pattern || pattern[0] == '\0') {
        return 0;
    }

    bool has_enabled = pattern_has_enabled_layout(app, pattern);
    int existing_idx = find_geom_restore_rule_index(&app->rules_config, pattern);
    bool changed = false;

    if (has_enabled && existing_idx < 0) {
        if (add_rule(&app->rules_config, pattern, "rl")) {
            Rule *new_rule = &app->rules_config.rules[app->rules_config.count - 1];
            int match_id = matching_find_or_create_pattern_entry(&app->matching, pattern);
            if (match_id <= 0) {
                log_warn("geom sync: failed creating pattern entry for '%s'", pattern);
                remove_rule(&app->rules_config, app->rules_config.count - 1);
                return 0;
            }
            new_rule->match_id = match_id;
            g_strlcpy(new_rule->tag, "geom", sizeof(new_rule->tag));
            changed = true;
            log_info("geom sync: created tagged restore rule for pattern '%s'", pattern);
        } else {
            log_warn("geom sync: failed creating tagged restore rule for pattern '%s'", pattern);
        }
    } else if (!has_enabled && existing_idx >= 0) {
        if (remove_rule(&app->rules_config, existing_idx)) {
            changed = true;
            log_info("geom sync: removed tagged restore rule for pattern '%s'", pattern);
        } else {
            log_warn("geom sync: failed removing tagged restore rule for pattern '%s'", pattern);
        }
    }

    if (changed) {
        save_match_entries(&app->matching);
        save_rules_config(&app->rules_config, &app->matching);
    }
    return changed ? 1 : 0;
}

int geom_rule_sync_all_layout_patterns(AppData *app) {
    if (!app) {
        return 0;
    }

    char patterns[MAX_WINDOWS][MAX_TITLE_LEN];
    int pattern_count = 0;
    for (int i = 0; i < app->layouts.count && pattern_count < MAX_WINDOWS; i++) {
        const LayoutRecord *record = &app->layouts.records[i];
        if (record->match_id <= 0) {
            continue;
        }
        int idx = match_entry_find_index_by_match_id(&app->matching, record->match_id);
        if (idx < 0) {
            continue;
        }
        const char *pattern = app->matching.entries[idx].original_title;
        if (pattern[0] == '\0') {
            continue;
        }

        bool seen = false;
        for (int p = 0; p < pattern_count; p++) {
            if (strcmp(patterns[p], pattern) == 0) {
                seen = true;
                break;
            }
        }
        if (seen) {
            continue;
        }

        g_strlcpy(patterns[pattern_count], pattern, sizeof(patterns[pattern_count]));
        pattern_count++;
    }

    int changed = 0;
    for (int i = 0; i < pattern_count; i++) {
        changed += geom_rule_sync_for_pattern(app, patterns[i]);
    }

    for (int i = 0; i < app->rules_config.count; ) {
        Rule *rule = &app->rules_config.rules[i];
        if (strcmp(rule->tag, "geom") == 0 &&
            rule_commands_contain_segment(rule->commands, "rl") &&
            !pattern_has_enabled_layout(app, rule->pattern)) {
            if (remove_rule(&app->rules_config, i)) {
                changed++;
                continue;
            }
        }
        i++;
    }

    if (changed > 0) {
        save_match_entries(&app->matching);
        save_rules_config(&app->rules_config, &app->matching);
    }
    return changed;
}
