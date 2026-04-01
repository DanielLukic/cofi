#include "rules_config.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

static const char* get_rules_config_path(void) {
    static char path[512];
    const char *home = getenv("HOME");
    if (!home) home = ".";

    snprintf(path, sizeof(path), "%s/.config", home);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi", home);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi/rules.json", home);
    return path;
}

void init_rules_config(RulesConfig *config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));
}

int add_rule(RulesConfig *config, const char *pattern, const char *commands) {
    if (!config || !pattern || !commands) return 0;
    if (config->count >= MAX_RULES) return 0;

    Rule *r = &config->rules[config->count];
    strncpy(r->pattern, pattern, MAX_PATTERN_LEN - 1);
    r->pattern[MAX_PATTERN_LEN - 1] = '\0';
    strncpy(r->commands, commands, MAX_COMMANDS_LEN - 1);
    r->commands[MAX_COMMANDS_LEN - 1] = '\0';
    config->count++;
    return 1;
}

int remove_rule(RulesConfig *config, int index) {
    if (!config || index < 0 || index >= config->count) return 0;

    for (int i = index; i < config->count - 1; i++) {
        config->rules[i] = config->rules[i + 1];
    }
    config->count--;
    return 1;
}

int save_rules_config(const RulesConfig *config) {
    if (!config) return 0;

    const char *path = get_rules_config_path();
    FILE *file = fopen(path, "w");
    if (!file) {
        log_error("Failed to open rules config for writing: %s", path);
        return 0;
    }

    fprintf(file, "{\n");
    fprintf(file, "  \"rules\": [\n");

    for (int i = 0; i < config->count; i++) {
        if (i > 0) fprintf(file, ",\n");
        fprintf(file, "    {\n");
        fprintf(file, "      \"pattern\": \"%s\",\n", config->rules[i].pattern);
        fprintf(file, "      \"commands\": \"%s\"\n", config->rules[i].commands);
        fprintf(file, "    }");
    }

    fprintf(file, "\n  ]\n");
    fprintf(file, "}\n");

    fclose(file);
    log_debug("Saved rules config to %s", path);
    return 1;
}

int load_rules_config(RulesConfig *config) {
    if (!config) return 0;

    const char *path = get_rules_config_path();
    FILE *file = fopen(path, "r");
    if (!file) {
        if (errno == ENOENT) {
            log_debug("No rules config found at %s", path);
            return 1;  // not an error, just no rules yet
        }
        log_error("Failed to open rules config: %s", path);
        return 0;
    }

    char line[1024];
    char pattern[MAX_PATTERN_LEN] = {0};
    char commands[MAX_COMMANDS_LEN] = {0};
    int in_rules = 0;

    while (fgets(line, sizeof(line), file)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        if (strstr(p, "\"rules\":")) {
            in_rules = 1;
        } else if (in_rules && strstr(p, "}")) {
            if (pattern[0] && commands[0]) {
                add_rule(config, pattern, commands);
                pattern[0] = '\0';
                commands[0] = '\0';
            }
        }

        if (in_rules) {
            if (strstr(p, "\"pattern\":")) {
                char *start = strchr(p, ':');
                if (start) {
                    start = strchr(start + 1, '"');
                    if (start) {
                        start++;
                        char *end = strchr(start, '"');
                        if (end) {
                            int len = end - start;
                            if (len >= MAX_PATTERN_LEN) len = MAX_PATTERN_LEN - 1;
                            strncpy(pattern, start, len);
                            pattern[len] = '\0';
                        }
                    }
                }
            } else if (strstr(p, "\"commands\":")) {
                char *start = strchr(p, ':');
                if (start) {
                    start = strchr(start + 1, '"');
                    if (start) {
                        start++;
                        char *end = strchr(start, '"');
                        if (end) {
                            int len = end - start;
                            if (len >= MAX_COMMANDS_LEN) len = MAX_COMMANDS_LEN - 1;
                            strncpy(commands, start, len);
                            commands[len] = '\0';
                        }
                    }
                }
            }
        }
    }

    fclose(file);
    log_info("Loaded %d rules from %s", config->count, path);
    return 1;
}
