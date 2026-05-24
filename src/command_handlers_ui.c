#include "command_handlers_ui.h"

#include "app_data.h"
#include "cofi_modal.h"
#include "command_availability.h"
#include "cofi_tab_provider.h"
#include "command_registry.h"
#include "config.h"
#include "config_provider.h"
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
    surface_tab(app, config_tab_mode());
}

static void handle_set_error(AppData *app, const char *error_text) {
    char msg[512];
    snprintf(msg, sizeof(msg), "Error: %s\n\nType :config to see available keys.", error_text);
    show_error_in_display(app, msg);
}

gboolean cofi_surface_provider_command(AppData *app, const char *command) {
    const CommandSpec *spec = cofi_command_for_token(command);
    if (!spec || !spec->owner_provider_id ||
        strcmp(spec->owner_provider_id, COMMAND_OWNER_CORE) == 0) {
        show_error_in_display(app, "Usage: show <tab>");
        return FALSE;
    }

    int provider_id = cofi_get_provider_id(spec->owner_provider_id);
    const CofiTabProvider *provider = cofi_get_provider(provider_id);
    if (!provider || !cofi_provider_is_enabled(provider_id)) {
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
    int quoted_empty = (strcmp(value, "\"\"") == 0 || strcmp(value, "''") == 0);
    if (quoted_empty) {
        value = "";
    }

    if (value[0] == '\0' && !quoted_empty) {
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
            return cofi_surface_provider_command(app, args);
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

static gboolean command_in_group(const CommandSpec *spec, const char *const *members) {
    if (!spec || !spec->primary || !members) return FALSE;
    for (int i = 0; members[i]; i++) {
        if (strcmp(spec->primary, members[i]) == 0) return TRUE;
    }
    return FALSE;
}

static void append_wrapped_line(GString *out, const char *label, const char *description, int width) {
    append_wrapped_command_line(out, label, description, width);
}

static void append_wrapped_paragraph(GString *out, const char *text, int width) {
    const char *indent = "  ";
    int indent_width = 2;
    if (width <= 0 || width <= indent_width) {
        g_string_append_printf(out, "%s%s\n", indent, text);
        return;
    }

    int content_width = width - indent_width;
    const char *cursor = text;
    while (*cursor) {
        int chunk_len = 0;
        int last_boundary = -1;
        while (cursor[chunk_len] != '\0' && chunk_len < content_width) {
            if (is_wrap_boundary_char(cursor[chunk_len])) {
                last_boundary = chunk_len;
            }
            chunk_len++;
        }

        g_string_append(out, indent);
        if (cursor[chunk_len] == '\0') {
            g_string_append_len(out, cursor, (gssize)chunk_len);
            g_string_append_c(out, '\n');
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
    }
}

static void append_help_static_sections(GString *out, int width) {
    g_string_append(out, "NAVIGATION\n\n");
    append_wrapped_line(out, "Up / Ctrl+K", "Move selection up", width);
    append_wrapped_line(out, "Down / Ctrl+J", "Move selection down", width);
    append_wrapped_line(out, "Enter", "Activate selected row", width);
    append_wrapped_line(out, "Escape", "Clear filter, close overlays, or hide cofi", width);
    append_wrapped_line(out, "Tab / Shift+Tab", "Cycle visible tabs forward/backward", width);

    g_string_append(out, "\nTABS\n\n");
    append_wrapped_paragraph(out, "Reach a tab via the prefix or command shown. Windows and Apps are cycled with Tab; the rest surface on demand.", width);
    g_string_append_c(out, '\n');
    append_wrapped_line(out, "windows      >", "Window list", width);
    append_wrapped_line(out, "apps         $ \\", "App launcher (default / all-apps)", width);
    append_wrapped_line(out, "emoji        :emoji", "Emoji picker", width);
    append_wrapped_line(out, "calc         = :calc", "Calculator", width);
    append_wrapped_line(out, "run          ! :run", "Shell command launcher", width);
    append_wrapped_line(out, "projects     :projects", "tmux/zellij sessions and folders", width);
    append_wrapped_line(out, "profiles     :profiles", "Browser profiles", width);
    append_wrapped_line(out, "sessions     :sessions", "Claude/Codex sessions", width);
    append_wrapped_line(out, "workspaces   :workspaces", "Workspace management", width);
    append_wrapped_line(out, "harpoon      :harpoon", "Window slots", width);
    append_wrapped_line(out, "matching     :matching", "Custom window matching", width);
    append_wrapped_line(out, "config       :config", "Configuration values", width);
    append_wrapped_line(out, "hotkeys      :hotkeys", "Global hotkey bindings", width);
    append_wrapped_line(out, "rules        :rules", "Window auto-action rules", width);
    append_wrapped_line(out, "sinks        :sinks", "Audio sink routing", width);
    append_wrapped_line(out, "proc         :proc", "Process list", width);

    g_string_append(out, "\nPREFIXES\n\n");
    append_wrapped_line(out, ":", "Enter command mode (works even with active filter text)", width);
    append_wrapped_line(out, "!", "Enter run mode", width);
    append_wrapped_line(out, "=", "Enter calculator mode", width);
    append_wrapped_line(out, "$", "Switch to Apps default mode", width);
    append_wrapped_line(out, "\\", "Switch to Apps all-apps mode", width);
    append_wrapped_line(out, ">", "Return to Windows tab", width);

    g_string_append(out, "\nHARPOON SLOTS\n\n");
    append_wrapped_line(out, "Ctrl+[key]", "Assign slot (J/K/U reserved for navigation unless Ctrl+Shift is used)", width);
    append_wrapped_line(out, "Ctrl+Shift+[key]", "Force slot assign for reserved keys", width);
    append_wrapped_line(out, "Alt+[letter]", "Recall letter slot target", width);
    append_wrapped_line(out, "Alt+[digit]", "Recall digit slot target (based on digit_slot_mode)", width);

    g_string_append(out, "\nWINDOWS-TAB KEYS\n\n");
    append_wrapped_line(out, "Alt+Tab", "Cycle selection forward", width);
    append_wrapped_line(out, "Shift+Alt+Tab", "Cycle selection backward", width);
    append_wrapped_line(out, ".", "Repeat last action", width);

    g_string_append(out, "\nPER-TAB KEYS\n\n");
    append_wrapped_line(out, "Harpoon", "Ctrl+A assign current window, Ctrl+E edit slot, Ctrl+D delete slot", width);
    append_wrapped_line(out, "Matching", "Ctrl+A assign name, Ctrl+E edit name, Ctrl+P edit pattern, Ctrl+D delete", width);
    append_wrapped_line(out, "Layouts", "Ctrl+D/Delete delete, Ctrl+L lock workspace restore, Ctrl+T toggle enable", width);
    append_wrapped_line(out, "Rules", "Ctrl+A add, Ctrl+E edit, Ctrl+D delete, Ctrl+X replay", width);
    append_wrapped_line(out, "Config", "Ctrl+E edit value, Ctrl+T toggle/cycle value", width);
    append_wrapped_line(out, "Hotkeys", "Ctrl+A add binding, Ctrl+E edit command, Ctrl+D remove binding", width);
    append_wrapped_line(out, "Projects", "Ctrl+S remote host, Ctrl+D/Delete delete, Ctrl+N/Insert new, Ctrl+R/F2 rename tmux, Ctrl+T terminal here", width);
    append_wrapped_line(out, "Sessions", "Ctrl+R/F2 rename session, Delete remove session entry", width);

    g_string_append(out, "\nCOMMAND MODE\n\n");
    append_wrapped_line(out, "Entry", "Press ':' to open, Escape to cancel, Enter to execute", width);
    append_wrapped_line(out, "History", "Up/Down cycle command history", width);
    append_wrapped_line(out, "Compact syntax", "Examples: cw2, sb+, ew-", width);
    append_wrapped_line(out, "Candidates", "Command completion strips compact suffix noise", width);
}

static void append_grouped_commands_section(GString *out, int width) {
    static const char *const window_cmds[] = {"ab","aot","cl","miw","mw","pw","sw","vmw","hmw",NULL};
    static const char *const tiling_cmds[] = {"tw",NULL};
    static const char *const workspace_cmds[] = {"cw","jw","maw","rw",NULL};
    static const char *const slot_cmds[] = {"as","jump-slot",NULL};
    static const char *const window_props_cmds[] = {"ew","sb","mouse",NULL};
    static const char *const monitor_cmds[] = {"tm",NULL};
    static const char *const naming_cmds[] = {"an","rename",NULL};
    static const char *const config_cmds[] = {"set",NULL};
    static const char *const tabs_cmds[] = {"show",NULL};

    struct {
        const char *title;
        const char *const *members;
    } groups[] = {
        {"Window", window_cmds},
        {"Tiling", tiling_cmds},
        {"Workspace", workspace_cmds},
        {"Slots", slot_cmds},
        {"Window-Props", window_props_cmds},
        {"Monitor", monitor_cmds},
        {"Naming", naming_cmds},
        {"Config", config_cmds},
        {"Tabs", tabs_cmds},
    };

    g_string_append(out, "\nCOMMANDS\n\n");
    for (unsigned int g = 0; g < sizeof(groups) / sizeof(groups[0]); g++) {
        g_string_append_printf(out, "%s\n\n", groups[g].title);
        for (int i = 0; i < cofi_command_count(); i++) {
            const CommandSpec *spec = cofi_command_at(i);
            if (!spec || !command_primary_is_available(spec->primary) ||
                !spec->help_format || !spec->description) {
                continue;
            }
            if (!command_in_group(spec, groups[g].members)) continue;
            append_wrapped_command_line(out, spec->help_format, spec->description, width);
        }
        g_string_append_c(out, '\n');
    }
}

char *generate_command_help_text(HelpFormat format, int width) {
    GString *help = g_string_new(NULL);
    if (format == HELP_FORMAT_CLI) {
        g_string_append(help, "COFI Command Mode Help\n");
        g_string_append(help, "======================\n\n");
    }

    append_help_static_sections(help, width);
    append_grouped_commands_section(help, width);
    g_string_append(help, "HELP\n\n");
    append_wrapped_line(help, "Navigation", "Up/Down/PgUp/PgDn/Home/End scroll help pages", width);

    return g_string_free(help, FALSE);
}
