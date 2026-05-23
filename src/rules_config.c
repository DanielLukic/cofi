#include "rules_config.h"
#include "cofi_json_io.h"
#include "log.h"
#include "window_matcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
    r->run_at_start = 0;
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
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "rules");
    json_builder_begin_array(builder);
    for (int i = 0; i < config->count; i++) {
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "pattern");
        json_builder_add_string_value(builder, config->rules[i].pattern);
        json_builder_set_member_name(builder, "commands");
        json_builder_add_string_value(builder, config->rules[i].commands);
        json_builder_set_member_name(builder, "run_at_start");
        json_builder_add_boolean_value(builder, config->rules[i].run_at_start);
        json_builder_end_object(builder);
    }
    json_builder_end_array(builder);
    json_builder_end_object(builder);

    JsonNode *root = json_builder_get_root(builder);
    bool ok = cofi_json_save_root(path, root);
    json_node_unref(root);
    g_object_unref(builder);
    if (ok) {
        log_debug("Saved rules config to %s", path);
    }
    return ok ? 1 : 0;
}

int load_rules_config(RulesConfig *config) {
    if (!config) return 0;

    const char *path = get_rules_config_path();
    init_rules_config(config);
    JsonParser *parser = cofi_json_load_object_file(path);
    if (!parser) {
        return 1;
    }

    JsonObject *root = json_node_get_object(json_parser_get_root(parser));
    JsonArray *rules = cofi_json_obj_array(root, "rules");
    if (!rules) {
        g_object_unref(parser);
        return 1;
    }

    guint n = json_array_get_length(rules);
    for (guint i = 0; i < n; i++) {
        JsonNode *element = json_array_get_element(rules, i);
        if (!element || !JSON_NODE_HOLDS_OBJECT(element)) {
            log_warn("rules_config: skipping non-object rule");
            continue;
        }

        JsonObject *rule = json_node_get_object(element);
        gboolean has_pattern = FALSE;
        gboolean has_commands = FALSE;
        const char *pattern = cofi_json_obj_str_or(rule, "pattern", "", &has_pattern);
        const char *commands = cofi_json_obj_str_or(rule, "commands", "", &has_commands);
        if (!has_pattern || pattern[0] == '\0') {
            log_warn("rules_config: skipping rule with missing/empty pattern");
            continue;
        }
        if (!has_commands || commands[0] == '\0') {
            log_warn("rules_config: skipping rule with missing/empty commands");
            continue;
        }
        if (!add_rule(config, pattern, commands)) {
            log_warn("rules_config: skipping rule because rule limit was reached");
            continue;
        }
        config->rules[config->count - 1].run_at_start =
            cofi_json_obj_bool_or(rule, "run_at_start", FALSE, NULL);
    }

    g_object_unref(parser);
    log_info("Loaded %d rules from %s", config->count, path);
    return 1;
}

static bool commands_contain_rl(const char *commands) {
    const char *p = commands;
    while (*p) {
        while (*p == ' ' || *p == ',') p++;
        if (p[0] == 'r' && p[1] == 'l' && (p[2] == '\0' || p[2] == ','))
            return true;
        while (*p && *p != ',') p++;
    }
    return false;
}

bool rules_needs_restore_rule(const RulesConfig *config, const char *window_title) {
    if (!config || !window_title) return false;
    for (int i = 0; i < config->count; i++) {
        if (commands_contain_rl(config->rules[i].commands) &&
            wildcard_match(config->rules[i].pattern, window_title))
            return false;
    }
    return true;
}
