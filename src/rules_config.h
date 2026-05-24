#ifndef RULES_CONFIG_H
#define RULES_CONFIG_H

#include <stdbool.h>

#define MAX_RULES 64
#define MAX_PATTERN_LEN 256
#define MAX_COMMANDS_LEN 256
#define MAX_RULE_TAG_LEN 64

typedef struct {
    char pattern[MAX_PATTERN_LEN];    // wildcard pattern for window title
    char commands[MAX_COMMANDS_LEN];  // comma-separated cofi commands
    int run_at_start;                 // allow this rule to fire during startup scan
    char tag[MAX_RULE_TAG_LEN];       // optional subsystem tag for UI filtering
} Rule;

typedef struct {
    Rule rules[MAX_RULES];
    int count;
} RulesConfig;

void init_rules_config(RulesConfig *config);
int save_rules_config(const RulesConfig *config);
int load_rules_config(RulesConfig *config);
int add_rule(RulesConfig *config, const char *pattern, const char *commands);
int remove_rule(RulesConfig *config, int index);
bool rules_needs_restore_rule(const RulesConfig *config, const char *window_title);

#endif // RULES_CONFIG_H
