#ifndef RULES_CONFIG_H
#define RULES_CONFIG_H

#define MAX_RULES 64
#define MAX_PATTERN_LEN 256
#define MAX_COMMANDS_LEN 256

typedef struct {
    char pattern[MAX_PATTERN_LEN];    // wildcard pattern for window title
    char commands[MAX_COMMANDS_LEN];  // comma-separated cofi commands
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

#endif // RULES_CONFIG_H
