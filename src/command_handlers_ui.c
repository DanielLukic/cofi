#include "command_handlers_ui.h"

#include "app_data.h"
#include "cofi_modal.h"
#include "command_availability.h"
#include "cofi_tab_provider.h"
#include "command_definitions.h"
#include "config.h"
#include "detach_launch.h"
#include "display.h"
#include "log.h"
#include "slot_store.h"
#include "tab_switching.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void dispatch_hotkey_mode(AppData *app, ShowMode mode);
extern void exit_command_mode(AppData *app);
extern void hide_window(AppData *app);
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
    if (strcmp(key, "disabled_providers") == 0) {
        cofi_apply_disabled_providers(app->config.disabled_providers);
        if (app->current_tab != TAB_WINDOWS && !cofi_get_provider_for_tab(app->current_tab)) {
            app->current_tab = TAB_WINDOWS;
        }
    }
    log_info("Config: %s = %s", key, value);
    exit_command_mode(app);
    surface_tab(app, TAB_CONFIG);
}

static void handle_set_error(AppData *app, const char *error_text) {
    char msg[512];
    snprintf(msg, sizeof(msg), "Error: %s\n\nType :config to see available keys.", error_text);
    show_error_in_display(app, msg);
}

static gboolean surface_provider_command(AppData *app, const char *command) {
    const CofiTabProvider *provider = cofi_get_provider_for_command(command);
    if (!provider) {
        show_error_in_display(app, "Usage: show <tab>");
        return FALSE;
    }
    exit_command_mode(app);
    if (app) {
        app->prefix_origin_tab = app->current_tab;
    }
    if (provider->on_surface) provider->on_surface(app);
    surface_tab(app, (TabMode)provider->tab_mode);
    return FALSE;
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

gboolean cmd_show(AppData *app, WindowInfo *window __attribute__((unused)), const char *args) {
    ShowMode mode = SHOW_MODE_WINDOWS;

    if (args && args[0] != '\0') {
        if (strcmp(args, "windows") == 0) mode = SHOW_MODE_WINDOWS;
        else if (strcmp(args, "command") == 0) mode = SHOW_MODE_COMMAND;
        else if (strcmp(args, "run") == 0) {
            if (!cofi_get_provider_for_prefix('!')) {
                show_error_in_display(app, "Run provider not available.");
                return FALSE;
            }
            mode = SHOW_MODE_RUN;
        }
        else {
            return surface_provider_command(app, args);
        }
    }

    exit_command_mode(app);
    dispatch_hotkey_mode(app, mode);
    return FALSE;
}

gboolean cmd_help(AppData *app, WindowInfo *window __attribute__((unused)),
                  const char *args __attribute__((unused))) {
    show_help_commands(app);
    return FALSE;
}

static gboolean is_wrap_boundary_char(char c) {
    return c == ' ' || c == '/' || c == '|';
}

static void append_wrapped_command_line(GString *out, const char *help_format,
                                        const char *description, int width) {
    const int desc_indent = 45; /* "  " + 40-char command column + " - " */
    const char *cont_prefix = "                                             ";
    const int desc_width = width - desc_indent;

    if (width <= 0 || desc_width <= 0) {
        g_string_append_printf(out, "  %-40s - %s\n", help_format, description);
        return;
    }

    g_string_append_printf(out, "  %-40s - ", help_format);

    const char *cursor = description;
    while (*cursor) {
        int chunk_len = 0;
        int last_boundary = -1;

        while (cursor[chunk_len] != '\0' && chunk_len < desc_width) {
            if (is_wrap_boundary_char(cursor[chunk_len])) {
                last_boundary = chunk_len;
            }
            chunk_len++;
        }

        if (cursor[chunk_len] == '\0') {
            g_string_append_len(out, cursor, (gssize)chunk_len);
            break;
        }

        if (last_boundary >= 0) {
            int emit_len = last_boundary + 1;
            g_string_append_len(out, cursor, (gssize)emit_len);
            cursor += emit_len;
            while (*cursor == ' ') {
                cursor++;
            }
        } else {
            g_string_append_len(out, cursor, (gssize)chunk_len);
            cursor += chunk_len;
        }

        g_string_append_c(out, '\n');
        g_string_append(out, cont_prefix);
    }

    g_string_append_c(out, '\n');
}

char *generate_command_help_text(HelpFormat format, int width) {
    size_t buffer_size = 1024;
    for (int i = 0; COMMAND_DEFINITIONS[i].primary != NULL; i++) {
        buffer_size += strlen(COMMAND_DEFINITIONS[i].help_format);
        buffer_size += strlen(COMMAND_DEFINITIONS[i].description);
        buffer_size += 100;
    }
    for (int i = 0; i < cofi_provider_count(); i++) {
        if (!cofi_provider_is_enabled(i)) continue;
        const CofiTabProvider *provider = cofi_get_provider(i);
        if (!provider || !provider->command_help_format ||
            !provider->command_description) {
            continue;
        }
        buffer_size += strlen(provider->command_help_format);
        buffer_size += strlen(provider->command_description);
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
    GString *commands = g_string_new(NULL);
    for (int i = 0; COMMAND_DEFINITIONS[i].primary != NULL; i++) {
        if (!command_primary_is_available(COMMAND_DEFINITIONS[i].primary)) {
            continue;
        }
        append_wrapped_command_line(commands,
                                    COMMAND_DEFINITIONS[i].help_format,
                                    COMMAND_DEFINITIONS[i].description,
                                    width);
    }
    for (int i = 0; i < cofi_provider_count(); i++) {
        if (!cofi_provider_is_enabled(i)) continue;
        const CofiTabProvider *provider = cofi_get_provider(i);
        if (!provider || !provider->command_help_format ||
            !provider->command_description) {
            continue;
        }
        append_wrapped_command_line(commands,
                                    provider->command_help_format,
                                    provider->command_description,
                                    width);
    }
    strcat(help_text, commands->str);
    g_string_free(commands, TRUE);

    strcat(help_text, "\nUsage:\n");
    strcat(help_text, "  Press ':' to enter command mode. Press Escape to cancel.\n");
    strcat(help_text, "  Type command and press Enter\n");
    strcat(help_text, "  Commands with arguments can be typed without spaces (e.g., 'cw2', 'j5', 'tL')\n");
    strcat(help_text, "  Chain multiple commands with commas (e.g., 'tc,vm' or 'cw2,tc')\n");
    strcat(help_text, "  Direct tiling: 'tr4' (right 75%), 'tl2' (left 50%), 'tc1' (center 33%)");

    return help_text;
}
