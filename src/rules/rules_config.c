#include "rules/rules_config.h"
#include "core/json/cofi_json_io.h"
#include "core/log/log.h"
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
    r->match_id = 0;
    r->run_at_start = 0;
    r->tag[0] = '\0';
    r->once = true;
    r->new_only = false;
    r->applied = 0;
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

int save_rules_config(const RulesConfig *config, const MatchEntryManager *manager) {
    if (!config) return 0;

    const char *path = get_rules_config_path();
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "rules");
    json_builder_begin_array(builder);
    for (int i = 0; i < config->count; i++) {
        const char *pattern_cache = config->rules[i].pattern;
        if (manager && config->rules[i].match_id > 0) {
            int entry_index = match_entry_find_index_by_match_id(manager, config->rules[i].match_id);
            if (entry_index >= 0) {
                pattern_cache = manager->entries[entry_index].original_title;
            }
        }
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "match_id");
        json_builder_add_int_value(builder, config->rules[i].match_id);
        json_builder_set_member_name(builder, "pattern");
        json_builder_add_string_value(builder, pattern_cache ? pattern_cache : "");
        json_builder_set_member_name(builder, "commands");
        json_builder_add_string_value(builder, config->rules[i].commands);
        json_builder_set_member_name(builder, "run_at_start");
        json_builder_add_boolean_value(builder, config->rules[i].run_at_start);
        json_builder_set_member_name(builder, "once");
        json_builder_add_boolean_value(builder, config->rules[i].once);
        json_builder_set_member_name(builder, "new_only");
        json_builder_add_boolean_value(builder, config->rules[i].new_only);
        if (config->rules[i].tag[0] != '\0') {
            json_builder_set_member_name(builder, "tag");
            json_builder_add_string_value(builder, config->rules[i].tag);
        }
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

int load_rules_config(RulesConfig *config, MatchEntryManager *manager) {
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
        gboolean has_match_id = FALSE;
        gboolean has_commands = FALSE;
        const char *pattern = cofi_json_obj_str_or(rule, "pattern", "", &has_pattern);
        int match_id = cofi_json_obj_int_or(rule, "match_id", 0, &has_match_id);
        const char *commands = cofi_json_obj_str_or(rule, "commands", "", &has_commands);
        if (!has_commands || commands[0] == '\0') {
            log_warn("rules_config: skipping rule with missing/empty commands");
            continue;
        }

        if (match_id <= 0) {
            if (!has_pattern || pattern[0] == '\0') {
                log_warn("rules_config: skipping rule with missing pattern and match_id");
                continue;
            }
            if (!manager) {
                log_warn("rules_config: cannot migrate legacy pattern '%s' without matching manager", pattern);
                continue;
            }
            match_id = matching_find_or_create_pattern_entry(manager, pattern);
            if (match_id <= 0) {
                log_warn("rules_config: failed to migrate legacy rule pattern '%s'", pattern);
                continue;
            }
        } else if (manager && match_entry_find_index_by_match_id(manager, match_id) < 0) {
            if (has_pattern && pattern[0] != '\0') {
                int fallback_match_id = matching_find_or_create_pattern_entry(manager, pattern);
                if (fallback_match_id > 0) {
                    match_id = fallback_match_id;
                } else {
                    log_warn("rules_config: skipping orphaned rule match_id=%d pattern='%s'", match_id, pattern);
                    continue;
                }
            } else {
                log_warn("rules_config: skipping orphaned rule with missing pattern fallback (match_id=%d)", match_id);
                continue;
            }
        }

        const char *stored_pattern = pattern;
        if ((!has_pattern || pattern[0] == '\0') && manager) {
            int idx = match_entry_find_index_by_match_id(manager, match_id);
            if (idx >= 0) {
                stored_pattern = manager->entries[idx].original_title;
            }
        }

        if (!add_rule(config, stored_pattern ? stored_pattern : "", commands)) {
            log_warn("rules_config: skipping rule because rule limit was reached");
            continue;
        }
        Rule *loaded_rule = &config->rules[config->count - 1];
        loaded_rule->match_id = match_id;
        loaded_rule->run_at_start = cofi_json_obj_bool_or(rule, "run_at_start", FALSE, NULL);
        loaded_rule->once = cofi_json_obj_bool_or(rule, "once", TRUE, NULL);
        loaded_rule->new_only = cofi_json_obj_bool_or(rule, "new_only", FALSE, NULL);
        const char *tag = cofi_json_obj_str_or(rule, "tag", "", NULL);
        g_strlcpy(loaded_rule->tag, tag, sizeof(loaded_rule->tag));
        loaded_rule->applied = 0;
        if (manager) {
            int idx = match_entry_find_index_by_match_id(manager, loaded_rule->match_id);
            if (idx >= 0) {
                g_strlcpy(loaded_rule->pattern,
                          manager->entries[idx].original_title,
                          sizeof(loaded_rule->pattern));
            }
        }
    }

    g_object_unref(parser);
    log_info("Loaded %d rules from %s", config->count, path);
    return 1;
}

bool rule_commands_contain_segment(const char *commands, const char *segment) {
    if (!commands || !segment || segment[0] == '\0') {
        return false;
    }

    const char *p = commands;
    while (*p != '\0') {
        while (*p == ',' || *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            p++;
        }
        if (*p == '\0') {
            break;
        }

        const char *end = p;
        while (*end != '\0' && *end != ',') {
            end++;
        }

        while (end > p && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) {
            end--;
        }

        size_t len = (size_t)(end - p);
        if (len > 0 && strlen(segment) == len && strncmp(p, segment, len) == 0) {
            return true;
        }

        p = (*end == ',') ? end + 1 : end;
    }
    return false;
}
