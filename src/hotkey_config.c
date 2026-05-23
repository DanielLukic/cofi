#include "hotkey_config.h"
#include "cofi_json_io.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

static const char* get_hotkey_config_path(void) {
    static char path[512];
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(path, sizeof(path), "%s/.config", home);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi", home);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/cofi/hotkeys.json", home);
    return path;
}

void init_hotkey_config(HotkeyConfig *config) {
    if (!config) return;
    memset(config, 0, sizeof(HotkeyConfig));
}

void init_default_hotkey_config(HotkeyConfig *config) {
    init_hotkey_config(config);
    add_hotkey_binding(config, "Mod1+Tab", "show windows!");
    add_hotkey_binding(config, "Mod1+grave", "show command!");
    add_hotkey_binding(config, "Mod1+BackSpace", "show workspaces!");
}

int find_hotkey_binding(const HotkeyConfig *config, const char *key) {
    if (!config || !key) return -1;
    for (int i = 0; i < config->count; i++) {
        if (strcmp(config->bindings[i].key, key) == 0) return i;
    }
    return -1;
}

int add_hotkey_binding(HotkeyConfig *config, const char *key, const char *command) {
    if (!config || !key || !command) return 0;

    int idx = find_hotkey_binding(config, key);
    if (idx >= 0) {
        strncpy(config->bindings[idx].command, command, sizeof(config->bindings[idx].command) - 1);
        return 1;
    }

    if (config->count >= MAX_HOTKEY_BINDINGS) {
        log_error("Maximum hotkey bindings reached (%d)", MAX_HOTKEY_BINDINGS);
        return 0;
    }

    strncpy(config->bindings[config->count].key, key,
            sizeof(config->bindings[config->count].key) - 1);
    strncpy(config->bindings[config->count].command, command,
            sizeof(config->bindings[config->count].command) - 1);
    config->count++;
    return 1;
}

int remove_hotkey_binding(HotkeyConfig *config, const char *key) {
    if (!config || !key) return 0;

    int idx = find_hotkey_binding(config, key);
    if (idx < 0) return 0;

    for (int i = idx; i < config->count - 1; i++) {
        config->bindings[i] = config->bindings[i + 1];
    }
    config->count--;
    return 1;
}

int save_hotkey_config(const HotkeyConfig *config) {
    if (!config) return 0;

    const char *path = get_hotkey_config_path();
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "hotkeys");
    json_builder_begin_array(builder);
    for (int i = 0; i < config->count; i++) {
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "key");
        json_builder_add_string_value(builder, config->bindings[i].key);
        json_builder_set_member_name(builder, "command");
        json_builder_add_string_value(builder, config->bindings[i].command);
        json_builder_end_object(builder);
    }
    json_builder_end_array(builder);
    json_builder_end_object(builder);

    JsonNode *root = json_builder_get_root(builder);
    bool ok = cofi_json_save_root(path, root);
    json_node_unref(root);
    g_object_unref(builder);
    if (ok) {
        log_debug("Saved %d hotkey bindings to %s", config->count, path);
    }
    return ok ? 1 : 0;
}

int load_hotkey_config(HotkeyConfig *config) {
    if (!config) return 0;
    init_hotkey_config(config);

    const char *path = get_hotkey_config_path();
    JsonParser *parser = cofi_json_load_object_file(path);
    if (!parser) {
        return 0;
    }

    JsonObject *root = json_node_get_object(json_parser_get_root(parser));
    JsonArray *hotkeys = cofi_json_obj_array(root, "hotkeys");
    if (!hotkeys) {
        g_object_unref(parser);
        return 0;
    }

    guint n = json_array_get_length(hotkeys);
    for (guint i = 0; i < n; i++) {
        JsonNode *element = json_array_get_element(hotkeys, i);
        if (!element || !JSON_NODE_HOLDS_OBJECT(element)) {
            log_warn("hotkey_config: skipping non-object binding");
            continue;
        }

        JsonObject *binding = json_node_get_object(element);
        gboolean has_key = FALSE;
        gboolean has_command = FALSE;
        const char *key = cofi_json_obj_str_or(binding, "key", "", &has_key);
        const char *command = cofi_json_obj_str_or(binding, "command", "", &has_command);
        if (!has_key || key[0] == '\0') {
            log_warn("hotkey_config: skipping binding with missing/empty key");
            continue;
        }
        if (!has_command || command[0] == '\0') {
            log_warn("hotkey_config: skipping binding with missing/empty command");
            continue;
        }
        add_hotkey_binding(config, key, command);
    }

    g_object_unref(parser);
    log_info("Loaded %d hotkey bindings from %s", config->count, path);
    return 1;
}

int format_hotkey_display(const HotkeyConfig *config, char *buf, size_t buf_size) {
    if (!config || !buf || buf_size == 0) return 0;

    if (config->count == 0) {
        return snprintf(buf, buf_size, "No hotkey bindings configured.\n\n"
                        "Use :hotkeys <key> <command> to add one.\n"
                        "Example: :hotkeys Mod4+1 jw 1\n");
    }

    int written = 0;
    for (int i = 0; i < config->count; i++) {
        int n = snprintf(buf + written, buf_size - written,
                         "%-20s %s\n",
                         config->bindings[i].key,
                         config->bindings[i].command);
        if (n < 0 || written + n >= (int)buf_size) break;
        written += n;
    }
    return written;
}

int parse_hotkey_command(const char *args, char *key_out, size_t key_size,
                         char *cmd_out, size_t cmd_size) {
    key_out[0] = '\0';
    cmd_out[0] = '\0';

    if (!args || args[0] == '\0') return 0;

    char buf[512] = {0};
    strncpy(buf, args, sizeof(buf) - 1);
    char *p = buf;
    while (*p && isspace((unsigned char)*p)) p++;

    const char *keywords[] = {"list", "set", "add", "del", "rm", "remove", NULL};
    for (int i = 0; keywords[i]; i++) {
        size_t len = strlen(keywords[i]);
        if (strncmp(p, keywords[i], len) == 0 &&
            (p[len] == '\0' || isspace((unsigned char)p[len]))) {
            if (strcmp(keywords[i], "list") == 0) return 0;
            p += len;
            while (*p && isspace((unsigned char)*p)) p++;
            break;
        }
    }

    if (*p == '\0') return 0;

    char *space = p;
    while (*space && !isspace((unsigned char)*space)) space++;

    size_t key_len = (size_t)(space - p);
    if (key_len >= key_size) key_len = key_size - 1;
    memcpy(key_out, p, key_len);
    key_out[key_len] = '\0';

    while (*space && isspace((unsigned char)*space)) space++;

    if (*space == '\0') return 2;

    strncpy(cmd_out, space, cmd_size - 1);
    cmd_out[cmd_size - 1] = '\0';
    return 1;
}
