#ifndef RULES_H
#define RULES_H

#include <X11/Xlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "rules_config.h"
#include "types.h"
#include "match_entry.h"

// One entry per (rule_index, window_id) pair.
// Worst case: MAX_RULES rules each monitoring MAX_WINDOWS windows.
#define MAX_RULE_STATE_ENTRIES (MAX_RULES * MAX_WINDOWS)

// Circuit-breaker constants
#define RULE_BREAKER_MAX_ENTRIES   512
#define RULE_BREAKER_BURST_LIMIT   10
#define RULE_BREAKER_BURST_WINDOW_MS 1000LL
#define RULE_BREAKER_QUIET_RESET_MS  2000LL

// Per-(rule_index, window_id) match state
typedef struct {
    int    rule_index;
    Window id;
    bool   matched;       // true = last check was a match (suppress re-fire)
} RuleWindowState;

// State for all rules across all windows
typedef struct {
    RuleWindowState windows[MAX_RULE_STATE_ENTRIES];
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
// rule_index must be the index into RulesConfig.rules[] so each rule has
// independent fire-once state per window (prevents cross-rule state stomping).
RuleMatch check_rule_match(const Rule *rule, RuleState *state, int rule_index,
                           const MatchEntryManager *manager, const WindowInfo *window);
bool rule_matches_window(const Rule *rule, const MatchEntryManager *manager,
                         const WindowInfo *window, const char **resolved_pattern);
void rule_toggle_once(Rule *rule);
void rules_clear_applied_for_dead_windows(RulesConfig *config, const Window *live_windows, int live_count);
// Remove all (*, id) entries — one per rule that has ever checked this window.
void rule_state_remove_window(RuleState *state, Window id);

void init_rule_breaker(RuleBreakerState *breaker);
// Returns true if the fire should proceed; false if suppressed by rate limiter.
// pattern is used only for the log_error message.
bool rule_breaker_should_fire(RuleBreakerState *breaker, int rule_index,
                               Window window_id, int64_t now_ms, const char *pattern);

// Remove state for any window not present in live_windows.
// Call once per _NET_CLIENT_LIST change, after apply_rules_to_windows.
void rule_state_prune_absent(RuleState *state, const Window *live_windows, int live_count);

#endif // RULES_H
