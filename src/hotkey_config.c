#include "hotkey_config.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
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
    FILE *f = fopen(path, "w");
    if (!f) {
        log_error("Failed to write hotkeys: %s", path);
        return 0;
    }

    fprintf(f, "{\n  \"hotkeys\": [\n");
    for (int i = 0; i < config->count; i++) {
        fprintf(f, "    {\"key\": \"%s\", \"command\": \"%s\"}%s\n",
                config->bindings[i].key, config->bindings[i].command,
                (i < config->count - 1) ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);

    log_debug("Saved %d hotkey bindings to %s", config->count, path);
    return 1;
}

int load_hotkey_config(HotkeyConfig *config) {
    if (!config) return 0;
    init_hotkey_config(config);

    const char *path = get_hotkey_config_path();
    FILE *f = fopen(path, "r");
    if (!f) {
        if (errno != ENOENT)
            log_error("Failed to read hotkeys: %s", path);
        return 0;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *key_start = strstr(line, "\"key\": \"");
        char *cmd_start = strstr(line, "\"command\": \"");
        if (!key_start || !cmd_start) continue;

        key_start += 8;
        char *key_end = strchr(key_start, '"');
        if (!key_end) continue;

        cmd_start += 12;
        char *cmd_end = strchr(cmd_start, '"');
        if (!cmd_end) continue;

        char key[64] = {0}, cmd[256] = {0};
        size_t key_len = (size_t)(key_end - key_start);
        size_t cmd_len = (size_t)(cmd_end - cmd_start);
        if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
        if (cmd_len >= sizeof(cmd)) cmd_len = sizeof(cmd) - 1;
        memcpy(key, key_start, key_len);
        memcpy(cmd, cmd_start, cmd_len);

        add_hotkey_binding(config, key, cmd);
    }

    fclose(f);
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
