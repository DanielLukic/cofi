#ifndef RULES_H
#define RULES_H

#include <X11/Xlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "rules_config.h"

// Max windows to track state for
#define MAX_RULE_TRACKED_WINDOWS 256

// Circuit-breaker constants
#define RULE_BREAKER_MAX_ENTRIES   512
#define RULE_BREAKER_BURST_LIMIT   10
#define RULE_BREAKER_BURST_WINDOW_MS 1000LL
#define RULE_BREAKER_QUIET_RESET_MS  2000LL

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

// Per-(rule_index, window_id) rate-limiting entry
typedef struct {
    int      rule_index;
    Window   window_id;
    int64_t  burst_start_ms;  // start of the current burst window
    int      burst_count;     // fires counted in this burst window
    int64_t  last_fire_ms;    // time of the most recent fire
    bool     suppressed;      // currently rate-limited
    bool     logged;          // error already emitted for this suppression
} RuleBreakerEntry;

// Circuit-breaker state (one instance per AppData)
typedef struct {
    RuleBreakerEntry entries[RULE_BREAKER_MAX_ENTRIES];
    int count;
} RuleBreakerState;

// Result of checking a rule against a window title
typedef struct {
    bool should_fire;
    const char *commands;  // points into the Rule, valid while Rule exists
} RuleMatch;

void init_rule_state(RuleState *state);
RuleMatch check_rule_match(const Rule *rule, RuleState *state, Window id, const char *title);
void rule_state_remove_window(RuleState *state, Window id);

void init_rule_breaker(RuleBreakerState *breaker);
// Returns true if the fire should proceed; false if suppressed by rate limiter.
// pattern is used only for the log_error message.
bool rule_breaker_should_fire(RuleBreakerState *breaker, int rule_index,
                               Window window_id, int64_t now_ms, const char *pattern);

// Mark windows absent from live_windows for one cycle; remove those absent a second
// consecutive cycle. Windows that reappear have pending_prune cleared automatically.
// Call once per _NET_CLIENT_LIST change, after apply_rules_to_windows.
void rule_state_prune_absent(RuleState *state, const Window *live_windows, int live_count);

#endif // RULES_H
