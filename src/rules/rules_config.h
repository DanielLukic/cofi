#ifndef RULES_CONFIG_H
#define RULES_CONFIG_H

#include <stdbool.h>
#include "matching/match_entry.h"

#define MAX_RULES 64
#define MAX_PATTERN_LEN 256
#define MAX_COMMANDS_LEN 256
#define MAX_RULE_TAG_LEN 64

typedef struct {
    int match_id;                     // authoritative match entry reference (0 = unresolved legacy)
    char pattern[MAX_PATTERN_LEN];    // wildcard pattern for window title
    char commands[MAX_COMMANDS_LEN];  // comma-separated cofi commands
    int run_at_start;                 // allow this rule to fire during startup scan
    char tag[MAX_RULE_TAG_LEN];       // optional subsystem tag for UI filtering
    bool once;                        // in-memory only; default true
    Window applied;                   // in-memory only; 0 when not applied
} Rule;

typedef struct {
    Rule rules[MAX_RULES];
    int count;
} RulesConfig;

void init_rules_config(RulesConfig *config);
int save_rules_config(const RulesConfig *config, const MatchEntryManager *manager);
int load_rules_config(RulesConfig *config, MatchEntryManager *manager);
int add_rule(RulesConfig *config, const char *pattern, const char *commands);
int remove_rule(RulesConfig *config, int index);
bool rule_commands_contain_segment(const char *commands, const char *segment);
bool rules_needs_restore_rule(const RulesConfig *config, const char *window_title);

#endif // RULES_CONFIG_H
