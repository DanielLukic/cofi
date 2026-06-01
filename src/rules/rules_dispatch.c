#include "rules/rules_dispatch.h"
#include "commands/command_api.h"
#include "core/app/app_data.h"
#include "core/log/log.h"
#include "x11/window_list.h"
#include <glib.h>

static void execute_rule_match(AppData *app, Rule *rule, int rule_index,
                               WindowInfo *window, gint64 now_ms,
                               const char *commands) {
    if (!rule_breaker_should_fire(&app->rule_breaker, rule_index, window->id,
                                  now_ms, rule->pattern)) {
        return;
    }

    log_info("RULE: '%s' matched window 0x%lx '%s' — executing: %s",
             rule->pattern, window->id, window->title, commands);
    gboolean prev = app->in_rule_dispatch;
    app->in_rule_dispatch = TRUE;
    execute_command_background(commands, app, window);
    app->in_rule_dispatch = prev;
}

void rules_apply(AppData *app, RuleTrigger trigger,
                 const Window *new_window_ids, int new_window_count) {
    if (app->rules_config.count == 0) return;
    if (app->in_rule_dispatch) {
        log_debug("RULE: apply_rules_to_windows skipped — re-entry during rule dispatch");
        return;
    }

    gint64 now_ms = g_get_monotonic_time() / 1000;
    for (int i = 0; i < app->window_count; i++) {
        WindowInfo *w = &app->windows[i];
        bool is_new_window = window_id_in_list(w->id, new_window_ids, new_window_count);
        for (int r = 0; r < app->rules_config.count; r++) {
            Rule *rule = &app->rules_config.rules[r];
            if (!rule_trigger_allows(rule, trigger, is_new_window)) {
                continue;
            }
            RuleMatch match = check_rule_match(
                rule, &app->rule_state, r, &app->matching, w);
            if (match.should_fire) {
                if (!app->initial_window_population_done && !rule->run_at_start) {
                    continue;
                }

                execute_rule_match(app, rule, r, w, now_ms, match.commands);
            }
        }
    }
}

void rules_apply_for_title_change(AppData *app, Window window_id) {
    if (app->rules_config.count == 0) return;
    if (app->in_rule_dispatch) {
        log_debug("RULE: handle_window_title_change skipped — re-entry during rule dispatch");
        return;
    }

    WindowInfo *w = NULL;
    for (int i = 0; i < app->window_count; i++) {
        if (app->windows[i].id == window_id) {
            w = &app->windows[i];
            break;
        }
    }
    if (!w) return;

    gint64 now_ms = g_get_monotonic_time() / 1000;
    for (int r = 0; r < app->rules_config.count; r++) {
        Rule *rule = &app->rules_config.rules[r];
        if (!rule_trigger_allows(rule, RULE_TRIGGER_TITLE_CHANGE, false)) {
            continue;
        }
        RuleMatch match = check_rule_match(
            rule, &app->rule_state, r, &app->matching, w);
        if (match.should_fire) {
            execute_rule_match(app, rule, r, w, now_ms, match.commands);
        }
    }
}
