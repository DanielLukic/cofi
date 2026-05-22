#ifndef RULES_H
#define RULES_H

#include <X11/Xlib.h>
#include <stdbool.h>
#include "rules_config.h"

// Max windows to track state for
#define MAX_RULE_TRACKED_WINDOWS 256

// Per-window match state for a single rule
typedef struct {
    Window id;
    bool matched;       // true = last check was a match (suppress re-fire)
    bool pending_prune; // true = absent from _NET_CLIENT_LIST for one cycle
} RuleWindowState;

// State for all rules across all windows
typedef struct {
    RuleWindowState windows[MAX_RULE_TRACKED_WINDOWS];
    int count;
} RuleState;

// Result of checking a rule against a window title
typedef struct {
    bool should_fire;
    const char *commands;  // points into the Rule, valid while Rule exists
} RuleMatch;

void init_rule_state(RuleState *state);
RuleMatch check_rule_match(const Rule *rule, RuleState *state, Window id, const char *title);
void rule_state_remove_window(RuleState *state, Window id);

// Mark windows absent from live_windows for one cycle; remove those absent a second
// consecutive cycle. Windows that reappear have pending_prune cleared automatically.
// Call once per _NET_CLIENT_LIST change, after apply_rules_to_windows.
void rule_state_prune_absent(RuleState *state, const Window *live_windows, int live_count);

#endif // RULES_H
