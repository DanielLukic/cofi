#include "rules_replay.h"

#include "command_api.h"
#include "log.h"
#include "window_matcher.h"

static int rule_index_from_filtered(AppData *app) {
    if (!app || app->selection.rules_index < 0 ||
        app->selection.rules_index >= app->filtered_rules_count) {
        return -1;
    }

    return app->filtered_rule_indices[app->selection.rules_index];
}

int replay_rule_against_open_windows(AppData *app, const Rule *rule) {
    if (!app || !rule) {
        return 0;
    }

    int replayed = 0;
    for (int i = 0; i < app->window_count; i++) {
        WindowInfo *window = &app->windows[i];
        if (!wildcard_match(rule->pattern, window->title)) {
            continue;
        }

        log_info("RULE REPLAY: pattern '%s' matched 0x%lx '%s' -> %s",
                 rule->pattern, window->id, window->title, rule->commands);
        execute_command_background(rule->commands, app, window);
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
