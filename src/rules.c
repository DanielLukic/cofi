#include "rules.h"
#include "log.h"
#include <string.h>

void init_rule_state(RuleState *state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
}

// Find or create the entry for a (rule_index, window_id) pair
static RuleWindowState* find_or_add_entry(RuleState *state, int rule_index, Window id) {
    for (int i = 0; i < state->count; i++) {
        if (state->windows[i].rule_index == rule_index && state->windows[i].id == id) {
            return &state->windows[i];
        }
    }
    if (state->count >= MAX_RULE_STATE_ENTRIES) return NULL;
    RuleWindowState *ws = &state->windows[state->count];
    ws->rule_index    = rule_index;
    ws->id            = id;
    ws->matched       = false;
    state->count++;
    return ws;
}

bool rule_matches_window(const Rule *rule, const MatchEntryManager *manager,
                         const WindowInfo *window, const char **resolved_pattern) {
    if (resolved_pattern) *resolved_pattern = "";
    if (!rule || !manager || !window || rule->match_id <= 0) {
        return false;
    }

    int entry_index = match_entry_find_index_by_match_id(manager, rule->match_id);
    if (entry_index < 0) {
        log_warn("rules: rule match_id=%d missing matching entry", rule->match_id);
        if (resolved_pattern) *resolved_pattern = rule->pattern;
        return false;
    }

    const MatchEntry *entry = &manager->entries[entry_index];
    if (resolved_pattern) *resolved_pattern = entry->original_title;
    return match_entry_matches_window(entry, window);
}

RuleMatch check_rule_match(const Rule *rule, RuleState *state, int rule_index,
                           const MatchEntryManager *manager, const WindowInfo *window) {
    RuleMatch result = {false, NULL};
    if (!rule || !state || !window) return result;

    bool matches = rule_matches_window(rule, manager, window, NULL);
    RuleWindowState *ws = find_or_add_entry(state, rule_index, window->id);
    if (!ws) return result;

    if (matches) {
        if (rule->once) {
            if (rule->applied != 0) {
                return result;
            }
            ((Rule *)rule)->applied = window->id;
        }
        ws->matched = true;
        result.should_fire = true;
        result.commands = rule->commands;
    } else if (ws->matched) {
        ws->matched = false;
    }

    return result;
}

void rule_toggle_once(Rule *rule) {
    if (!rule) return;
    rule->once = !rule->once;
    rule->applied = 0;
}

void rules_clear_applied_for_dead_windows(RulesConfig *config, const Window *live_windows, int live_count) {
    if (!config) return;
    for (int i = 0; i < config->count; i++) {
        Window applied = config->rules[i].applied;
        bool found = false;
        if (applied == 0) continue;
        for (int j = 0; j < live_count; j++) if (live_windows[j] == applied) { found = true; break; }
        if (!found) config->rules[i].applied = 0;
    }
}

void rule_state_remove_window(RuleState *state, Window id) {
    if (!state) return;
    // Remove all (*, id) entries — there is one per rule that checked this window.
    int write = 0;
    for (int i = 0; i < state->count; i++) {
        if (state->windows[i].id != id) {
            state->windows[write++] = state->windows[i];
        }
    }
    state->count = write;
}

void init_rule_breaker(RuleBreakerState *breaker) {
    if (!breaker) return;
    memset(breaker, 0, sizeof(*breaker));
}

bool rule_breaker_should_fire(RuleBreakerState *breaker, int rule_index,
                               Window window_id, int64_t now_ms, const char *pattern) {
    if (!breaker) return true;

    RuleBreakerEntry *entry = NULL;
    for (int i = 0; i < breaker->count; i++) {
        if (breaker->entries[i].rule_index == rule_index &&
            breaker->entries[i].window_id == window_id) {
            entry = &breaker->entries[i];
            break;
        }
    }

    if (!entry) {
        if (breaker->count >= RULE_BREAKER_MAX_ENTRIES) return true; // full: fail open
        entry = &breaker->entries[breaker->count++];
        entry->rule_index    = rule_index;
        entry->window_id     = window_id;
        entry->burst_start_ms = now_ms;
        entry->burst_count   = 0;
        entry->last_fire_ms  = 0;
        entry->suppressed    = false;
        entry->logged        = false;
    }

    if (entry->suppressed) {
        if (now_ms - entry->last_fire_ms > RULE_BREAKER_QUIET_RESET_MS) {
            entry->suppressed    = false;
            entry->logged        = false;
            entry->burst_count   = 0;
            entry->burst_start_ms = now_ms;
        } else {
            return false;
        }
    }

    if (now_ms - entry->burst_start_ms > RULE_BREAKER_BURST_WINDOW_MS) {
        entry->burst_count    = 0;
        entry->burst_start_ms = now_ms;
    }

    entry->burst_count++;
    entry->last_fire_ms = now_ms;

    if (entry->burst_count > RULE_BREAKER_BURST_LIMIT) {
        entry->suppressed = true;
        if (!entry->logged) {
            log_error("RULE BREAKER: '%s' on 0x%lx exceeded %d fires/s — suppressing"
                      " (likely feedback loop)",
                      pattern ? pattern : "?", window_id, RULE_BREAKER_BURST_LIMIT);
            entry->logged = true;
        }
        return false;
    }

    return true;
}

void rule_state_prune_absent(RuleState *state, const Window *live_windows, int live_count) {
    if (!state) return;
    int i = 0;
    while (i < state->count) {
        Window id = state->windows[i].id;
        bool found = false;
        for (int j = 0; j < live_count; j++) {
            if (live_windows[j] == id) { found = true; break; }
        }
        if (found) {
            i++;
        } else {
            state->windows[i] = state->windows[state->count - 1];
            state->count--;
        }
    }
}
