#include "rules_replay.h"

#include "command_api.h"
#include "log.h"
#include "rules_provider.h"
#include "rules.h"

static int rule_index_from_filtered(AppData *app) {
    return rules_selected_config_index(app);
}

int replay_rule_against_open_windows(AppData *app, const Rule *rule) {
    if (!app || !rule) {
        return 0;
    }

    int replayed = 0;
    for (int i = 0; i < app->window_count; i++) {
        WindowInfo *window = &app->windows[i];
        const char *pattern = NULL;
        if (rule->once && rule->applied != 0) {
            continue;
        }
        if (!rule_matches_window(rule, &app->matching, window, &pattern)) {
            continue;
        }

        log_info("RULE REPLAY: pattern '%s' matched 0x%lx '%s' -> %s",
                 pattern ? pattern : rule->pattern, window->id, window->title, rule->commands);
        execute_command_background(rule->commands, app, window);
        ((Rule *)rule)->applied = window->id;
        replayed++;
    }

    return replayed;
}

int replay_all_rules_against_open_windows(AppData *app) {
    if (!app) {
        return 0;
    }

    int replayed = 0;
    for (int i = 0; i < app->rules_config.count; i++) {
        replayed += replay_rule_against_open_windows(app, &app->rules_config.rules[i]);
    }

    log_info("RULE REPLAY: executed all rules, total matches=%d", replayed);
    return replayed;
}

gboolean replay_selected_filtered_rule(AppData *app) {
    if (!app || app->filtered_rules_count <= 0) {
        return FALSE;
    }

    int rule_index = rule_index_from_filtered(app);
    if (rule_index < 0 || rule_index >= app->rules_config.count) {
        return FALSE;
    }

    replay_rule_against_open_windows(app, &app->rules_config.rules[rule_index]);
    return TRUE;
}
