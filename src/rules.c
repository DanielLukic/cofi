#include "rules.h"
#include "window_matcher.h"
#include <string.h>

void init_rule_state(RuleState *state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
}

// Find or create a window entry in the state
static RuleWindowState* find_or_add_window(RuleState *state, Window id) {
    // Find existing
    for (int i = 0; i < state->count; i++) {
        if (state->windows[i].id == id) {
            return &state->windows[i];
        }
    }
    // Add new
    if (state->count >= MAX_RULE_TRACKED_WINDOWS) return NULL;
    RuleWindowState *ws = &state->windows[state->count];
    ws->id = id;
    ws->matched = false;
    state->count++;
    return ws;
}

RuleMatch check_rule_match(const Rule *rule, RuleState *state, Window id, const char *title) {
    RuleMatch result = {false, NULL};
    if (!rule || !state || !title) return result;

    bool matches = wildcard_match(rule->pattern, title);
    RuleWindowState *ws = find_or_add_window(state, id);
    if (!ws) return result;

    if (matches && !ws->matched) {
        // Transition: not-matched → matched: FIRE
        ws->matched = true;
        result.should_fire = true;
        result.commands = rule->commands;
    } else if (matches && ws->matched) {
        // Still matching: suppress
        result.should_fire = false;
    } else if (!matches && ws->matched) {
        // Transition: matched → not-matched: reset
        ws->matched = false;
        result.should_fire = false;
    }
    // !matches && !ws->matched: no change

    return result;
}

void rule_state_remove_window(RuleState *state, Window id) {
    if (!state) return;
    for (int i = 0; i < state->count; i++) {
        if (state->windows[i].id == id) {
            state->windows[i] = state->windows[state->count - 1];
            state->count--;
            return;
        }
    }
}
