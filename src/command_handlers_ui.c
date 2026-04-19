#include "command_handlers_ui.h"

#include "app_data.h"
#include "command_definitions.h"
#include "config.h"
#include "display.h"
#include "hotkey_config.h"
#include "hotkeys.h"
#include "log.h"
#include "run_mode.h"
#include "tab_switching.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void dispatch_hotkey_mode(AppData *app, ShowMode mode);
extern void exit_command_mode(AppData *app);
extern void show_help_commands(AppData *app);

static void show_error_in_display(AppData *app, const char *msg) {
    if (app->textbuffer) {
        gtk_text_buffer_set_text(app->textbuffer, msg, -1);
    }
    app->command_mode.showing_help = TRUE;
}

static gboolean parse_set_assignment(const char *args, char *key, size_t key_size,
                                     const char **value_out) {
    if (!args || args[0] == '\0') {
        return FALSE;
    }

    const char *sep = args;
    while (*sep && *sep != ' ' && *sep != '=') {
        sep++;
    }

    size_t key_len = (size_t)(sep - args);
    if (key_len >= key_size) {
        key_len = key_size - 1;
    }

    memcpy(key, args, key_len);
    key[key_len] = '\0';
    while (*sep == ' ' || *sep == '=') {
        sep++;
    }

    *value_out = sep;
    return TRUE;
}

static void handle_set_success(AppData *app, const char *key, const char *value) {
    save_config(&app->config);
    log_info("Config: %s = %s", key, value);
    exit_command_mode(app);
    surface_tab(app, TAB_CONFIG);
}

static void handle_set_error(AppData *app, const char *error_text) {
    char msg[512];
    snprintf(msg, sizeof(msg), "Error: %s\n\nType :config to see available keys.", error_text);
    show_error_in_display(app, msg);
}

gboolean cmd_set_config(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    char key[64] = {0};
    const char *value = "";

    if (!parse_set_assignment(args, key, sizeof(key), &value)) {
        show_error_in_display(app, "Usage: set <key> <value>\n\nType :config to see available keys.");
        return FALSE;
    }

    if (value[0] == '\0') {
        char msg[256];
        snprintf(msg, sizeof(msg), "Missing value for '%s'.\n\nType :config to see current values.", key);
        show_error_in_display(app, msg);
        return FALSE;
    }

    char err[256] = {0};
    if (apply_config_setting(&app->config, key, value, err, sizeof(err))) {
        handle_set_success(app, key, value);
    } else {
        handle_set_error(app, err);
    }

    return FALSE;
}

gboolean cmd_show_config(AppData *app, WindowInfo *window __attribute__((unused)),
                         const char *args __attribute__((unused))) {
    exit_command_mode(app);
    surface_tab(app, TAB_CONFIG);
    return FALSE;
}

gboolean cmd_workspaces(AppData *app, WindowInfo *window __attribute__((unused)),
                        const char *args __attribute__((unused))) {
    exit_command_mode(app);
    surface_tab(app, TAB_WORKSPACES);
    return FALSE;
}

gboolean cmd_harpoon(AppData *app, WindowInfo *window __attribute__((unused)),
                     const char *args __attribute__((unused))) {
    exit_command_mode(app);
    surface_tab(app, TAB_HARPOON);
    return FALSE;
}

gboolean cmd_names(AppData *app, WindowInfo *window __attribute__((unused)),
                   const char *args __attribute__((unused))) {
    exit_command_mode(app);
    surface_tab(app, TAB_NAMES);
    return FALSE;
}

gboolean cmd_show(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    ShowMode mode = SHOW_MODE_WINDOWS;

    if (args && args[0] != '\0') {
        if (strcmp(args, "windows") == 0) mode = SHOW_MODE_WINDOWS;
        else if (strcmp(args, "command") == 0) mode = SHOW_MODE_COMMAND;
        else if (strcmp(args, "run") == 0) mode = SHOW_MODE_RUN;
        else if (strcmp(args, "workspaces") == 0) mode = SHOW_MODE_WORKSPACES;
        else if (strcmp(args, "harpoon") == 0) mode = SHOW_MODE_HARPOON;
        else if (strcmp(args, "names") == 0) {
            exit_command_mode(app);
            surface_tab(app, TAB_NAMES);
            return FALSE;
        } else if (strcmp(args, "config") == 0) {
            exit_command_mode(app);
            surface_tab(app, TAB_CONFIG);
            return FALSE;
        } else if (strcmp(args, "apps") == 0) {
            exit_command_mode(app);
            surface_tab(app, TAB_APPS);
            return FALSE;
        } else {
            show_error_in_display(app, "Usage: show [windows|command|run|workspaces|harpoon|names|config|apps]");
            return FALSE;
        }
    }

    exit_command_mode(app);
    dispatch_hotkey_mode(app, mode);
    return FALSE;
}

gboolean cmd_hotkeys(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    char key[64] = {0};
    char cmd[256] = {0};
    int action = parse_hotkey_command(args, key, sizeof(key), cmd, sizeof(cmd));

    if (action == 1) {
        add_hotkey_binding(&app->hotkey_config, key, cmd);
        save_hotkey_config(&app->hotkey_config);
        regrab_hotkeys(app);
        log_info("Hotkey bound: %s → %s", key, cmd);
    } else if (action == 2) {
        if (remove_hotkey_binding(&app->hotkey_config, key)) {
            save_hotkey_config(&app->hotkey_config);
            regrab_hotkeys(app);
            log_info("Hotkey unbound: %s", key);
        } else {
            log_warn("No hotkey binding for: %s", key);
        }
    }

    char buf[4096] = {0};
    format_hotkey_display(&app->hotkey_config, buf, sizeof(buf));
    gtk_text_buffer_set_text(app->textbuffer, buf, -1);
    app->command_mode.showing_help = TRUE;
    return FALSE;
}

gboolean cmd_help(AppData *app, WindowInfo *window __attribute__((unused)),
                  const char *args __attribute__((unused))) {
    show_help_commands(app);
    return FALSE;
}

char *generate_command_help_text(HelpFormat format) {
    size_t buffer_size = 1024;
    for (int i = 0; COMMAND_DEFINITIONS[i].primary != NULL; i++) {
        buffer_size += strlen(COMMAND_DEFINITIONS[i].help_format);
        buffer_size += strlen(COMMAND_DEFINITIONS[i].description);
        buffer_size += 100;
    }

    char *help_text = malloc(buffer_size);
    if (!help_text) {
        return NULL;
    }

    if (format == HELP_FORMAT_CLI) {
        strcpy(help_text, "COFI Command Mode Help\n");
        strcat(help_text, "======================\n\n");
    } else {
        strcpy(help_text, "");
    }

    strcat(help_text, "Available commands:\n\n");
    for (int i = 0; COMMAND_DEFINITIONS[i].primary != NULL; i++) {
        char line[256];
        snprintf(line, sizeof(line), "  %-40s - %s\n",
                 COMMAND_DEFINITIONS[i].help_format,
                 COMMAND_DEFINITIONS[i].description);
        strcat(help_text, line);
    }

    strcat(help_text, "\nUsage:\n");
    strcat(help_text, "  Press ':' to enter command mode. Press Escape to cancel.\n");
    strcat(help_text, "  Type command and press Enter\n");
    strcat(help_text, "  Commands with arguments can be typed without spaces (e.g., 'cw2', 'j5', 'tL')\n");
    strcat(help_text, "  Chain multiple commands with commas (e.g., 'tc,vm' or 'cw2,tc')\n");
    strcat(help_text, "  Direct tiling: 'tr4' (right 75%), 'tl2' (left 50%), 'tc1' (center 33%)");

    return help_text;
}
