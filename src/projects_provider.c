#include "projects_provider.h"

#include "app_data.h"
#include "command_mode.h"
#include "command_registry.h"
#include "cofi_tab_provider.h"
#include "config.h"
#include "overlay_manager.h"
#include "projects.h"
#include "projects_parse.h"
#include "slot_store.h"
#include "tab_switching.h"
#include "window_lifecycle.h"

#include <string.h>

static CofiTabProvider s_projects_provider;
static int s_projects_provider_id = -1;
#define PROJECTS_PROVIDER_ID "projects"
#define LEGACY_PROJECTS_SLOT_PROVIDER_ID "sessions"

static int set_optional_executable_path(char *field, size_t field_size,
                                        const char *key,
                                        const char *value,
                                        char *err_buf,
                                        size_t err_size) {
    if (!field || field_size == 0 || !value) return 0;
    if (value[0] == '\0') {
        field[0] = '\0';
        return 1;
    }
    if (!g_path_is_absolute(value)) {
        g_snprintf(err_buf, err_size,
                   "%s must be absolute, or empty to use PATH", key);
        return 0;
    }
    if (!g_file_test(value, G_FILE_TEST_IS_REGULAR) ||
        !g_file_test(value, G_FILE_TEST_IS_EXECUTABLE)) {
        g_snprintf(err_buf, err_size, "%s is not executable: %s", key, value);
        return 0;
    }
    g_strlcpy(field, value, field_size);
    return 1;
}

static void copy_front_ellipsized(const char *text, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!text) return;

    size_t len = strlen(text);
    if (len < out_size) {
        g_strlcpy(out, text, out_size);
        return;
    }

    if (out_size <= 4) {
        g_strlcpy(out, "...", out_size);
        return;
    }

    size_t keep = out_size - 4;
    g_snprintf(out, out_size, "...%s", text + len - keep);
}

static int format_path_display(const char *configured, const char *tool_name,
                               char *out, size_t out_size) {
    if (!out || out_size == 0 || !tool_name) return 0;
    out[0] = '\0';

    if (configured && configured[0] != '\0') {
        copy_front_ellipsized(configured, out, out_size);
        return 1;
    }

    gchar *resolved = g_find_program_in_path(tool_name);
    if (!resolved) {
        g_strlcpy(out, "(NOT FOUND)", out_size);
        return 1;
    }

    char compact_path[CONFIG_VALUE_LEN];
    copy_front_ellipsized(resolved, compact_path, sizeof(compact_path) - 7);
    g_snprintf(out, out_size, "(PATH:%s)", compact_path);
    g_free(resolved);
    return 1;
}

static int format_file_explorer_display(const CofiConfig *config,
                                        char *out, size_t out_size) {
    if (!config || !out || out_size == 0) return 0;
    if (config->projects_file_explorer_path[0] != '\0') {
        copy_front_ellipsized(config->projects_file_explorer_path, out, out_size);
        return 1;
    }

    const char *tools[] = {"caja", "xdg-open", "gio"};
    for (size_t i = 0; i < sizeof(tools) / sizeof(tools[0]); i++) {
        gchar *resolved = g_find_program_in_path(tools[i]);
        if (!resolved) continue;

        char compact_path[CONFIG_VALUE_LEN];
        size_t suffix = strcmp(tools[i], "gio") == 0 ? strlen(" open)") : strlen(")");
        size_t budget = out_size > strlen("(PATH:") + suffix ?
            out_size - strlen("(PATH:") - suffix : out_size;
        copy_front_ellipsized(resolved, compact_path, budget);
        g_snprintf(out, out_size, strcmp(tools[i], "gio") == 0 ?
                   "(PATH:%s open)" : "(PATH:%s)", compact_path);
        g_free(resolved);
        return 1;
    }

    g_strlcpy(out, "(NOT FOUND)", out_size);
    return 1;
}

#define DEFINE_PROJECT_PATH_CONFIG(name, field, tool) \
static int get_##name(const CofiConfig *config, char *out, size_t out_size) { \
    if (!config || !out || out_size == 0) return 0; \
    g_strlcpy(out, config->field, out_size); \
    return 1; \
} \
static int display_##name(const CofiConfig *config, char *out, size_t out_size) { \
    if (!config || !out || out_size == 0) return 0; \
    return format_path_display(config->field, tool, out, out_size); \
} \
static int set_##name(CofiConfig *config, const char *value, \
                      char *err_buf, size_t err_size) { \
    return set_optional_executable_path(config->field, sizeof(config->field), \
                                        "projects." #name, value, err_buf, err_size); \
}

DEFINE_PROJECT_PATH_CONFIG(tmux_path, projects_tmux_path, "tmux")
DEFINE_PROJECT_PATH_CONFIG(zellij_path, projects_zellij_path, "zellij")
DEFINE_PROJECT_PATH_CONFIG(zoxide_path, projects_zoxide_path, "zoxide")

static int get_file_explorer_path(const CofiConfig *config, char *out, size_t out_size) {
    if (!config || !out || out_size == 0) return 0;
    g_strlcpy(out, config->projects_file_explorer_path, out_size);
    return 1;
}

static int display_file_explorer_path(const CofiConfig *config,
                                      char *out, size_t out_size) {
    return format_file_explorer_display(config, out, out_size);
}

static int set_file_explorer_path(CofiConfig *config, const char *value,
                                  char *err_buf, size_t err_size) {
    return set_optional_executable_path(config->projects_file_explorer_path,
                                        sizeof(config->projects_file_explorer_path),
                                        "projects.file_explorer_path",
                                        value, err_buf, err_size);
}

static void register_projects_config_entries(void) {
    static const CofiConfigSpec specs[] = {
        {
            .key = "projects.tmux_path",
            .owner_provider_id = PROJECTS_PROVIDER_ID,
            .type = CONFIG_TYPE_STRING,
            .get_value = get_tmux_path,
            .set_value = set_tmux_path,
            .get_display_value = display_tmux_path,
        },
        {
            .key = "projects.zellij_path",
            .owner_provider_id = PROJECTS_PROVIDER_ID,
            .type = CONFIG_TYPE_STRING,
            .get_value = get_zellij_path,
            .set_value = set_zellij_path,
            .get_display_value = display_zellij_path,
        },
        {
            .key = "projects.zoxide_path",
            .owner_provider_id = PROJECTS_PROVIDER_ID,
            .type = CONFIG_TYPE_STRING,
            .get_value = get_zoxide_path,
            .set_value = set_zoxide_path,
            .get_display_value = display_zoxide_path,
        },
        {
            .key = "projects.file_explorer_path",
            .owner_provider_id = PROJECTS_PROVIDER_ID,
            .type = CONFIG_TYPE_STRING,
            .get_value = get_file_explorer_path,
            .set_value = set_file_explorer_path,
            .get_display_value = display_file_explorer_path,
        },
    };
    for (size_t i = 0; i < sizeof(specs) / sizeof(specs[0]); i++) {
        cofi_register_config_entry(&specs[i]);
    }
}

static TabMode projects_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_projects_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

static void show_new_session_for_selection(AppData *app, gboolean prefer_zellij) {
    ProjectBackend backend = prefer_zellij ? PROJECT_BACKEND_ZELLIJ : PROJECT_BACKEND_TMUX;
    ProjectSessionEntry *session = projects_selected_session(app);
    if (!prefer_zellij && session && session->backend == PROJECT_BACKEND_ZELLIJ) {
        backend = PROJECT_BACKEND_ZELLIJ;
    }

    ProjectFolder *folder = projects_selected_folder(app);
    if (!folder) {
        show_project_new_overlay(app, backend, "", "");
        return;
    }

    gchar *session_name = projects_build_folder_session_name(folder->path);
    show_project_new_overlay(app, backend, folder->path, session_name);
    g_free(session_name);
}

gboolean handle_projects_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != projects_tab_mode()) {
        return FALSE;
    }

    gboolean ctrl_n =
        (event->state & GDK_CONTROL_MASK) &&
        !(event->state & GDK_SHIFT_MASK) &&
        (event->keyval == GDK_KEY_n || event->keyval == GDK_KEY_N);
    if (event->keyval == GDK_KEY_Insert || event->keyval == GDK_KEY_KP_Insert || ctrl_n) {
        show_new_session_for_selection(app, (event->state & GDK_SHIFT_MASK) != 0);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete) {
        ProjectSessionEntry *session = projects_selected_session(app);
        if (!session) {
            return FALSE;
        }
        show_project_kill_overlay(app, session->name, session->backend);
        return TRUE;
    }

    gboolean ctrl_r =
        (event->state & GDK_CONTROL_MASK) &&
        !(event->state & GDK_SHIFT_MASK) &&
        (event->keyval == GDK_KEY_r || event->keyval == GDK_KEY_R);
    if (event->keyval == GDK_KEY_F2 || ctrl_r) {
        ProjectSessionEntry *session = projects_selected_session(app);
        if (!session || session->backend != PROJECT_BACKEND_TMUX) {
            return FALSE;
        }
        show_project_rename_overlay(app, session->name);
        return TRUE;
    }

    return FALSE;
}

static CofiActionStatus projects_provider_on_enter_pressed(AppData *app, int filtered_idx,
                                                       int raw_idx,
                                                       const char *entry_text,
                                                       int modifier_state) {
    (void)filtered_idx;
    (void)entry_text;
    (void)modifier_state;
    /* raw_idx is visible here: Projects exposes its filtered list directly to the provider renderer. */
    ProjectFolder *folder = projects_folder_at_visible(app, raw_idx);
    if (folder) {
        return projects_open_folder(app, folder->path);
    }
    return projects_attach_visible(app, raw_idx);
}

static CofiActionStatus projects_provider_on_command_args(AppData *app, const char *args) {
    if (!app) return COFI_NO_OP;
    if (!args || args[0] == '\0') return COFI_NO_OP;
    projects_refresh(app);

    if (projects_has_named(app, args)) {
        return projects_attach_named(app, args);
    }

    char slot = '\0';
    if (slot_parse_at_key_arg(args, &slot)) {
        const char *payload = slot_lookup(&app->harpoon.store, PROJECTS_PROVIDER_ID, slot);
        if (!payload) {
            payload = slot_lookup(&app->harpoon.store, LEGACY_PROJECTS_SLOT_PROVIDER_ID, slot);
        }
        if (!payload) return COFI_ACTION_ERROR;
        return projects_slot_recall(app, payload);
    }
    return COFI_ACTION_ERROR;
}

static void projects_show_command_error(AppData *app, const char *message) {
    if (!app || !app->textbuffer) return;
    gtk_text_buffer_set_text(app->textbuffer, message, -1);
    app->command_mode.showing_help = TRUE;
}

static gboolean projects_command_handler(AppData *app,
                                         WindowInfo *window __attribute__((unused)),
                                         const char *args) {
    exit_command_mode(app);

    if (args && args[0] != '\0') {
        CofiActionStatus status = projects_provider_on_command_args(app, args);
        if (status == COFI_HANDLED_HIDE) {
            hide_window(app);
        } else if (status == COFI_ACTION_ERROR || status == COFI_NO_OP) {
            projects_show_command_error(app, "No matching tmux/zellij session.");
        }
        return FALSE;
    }

    if (app) {
        app->prefix_origin_tab = app->current_tab;
    }
    surface_tab(app, projects_tab_mode());
    return FALSE;
}

static const CommandSpec s_projects_command = {
    .primary = "projects",
    .aliases = {"project", "tmux", "tx", "zj", "zellij"},
    .owner_provider_id = PROJECTS_PROVIDER_ID,
    .handler = projects_command_handler,
    .description = "Switch to projects tab",
    .help_format = "projects, project, tmux, tx, zj, zellij [@SLOT|SESSION]",
    .keeps_open_on_hotkey_auto = 1
};

void projects_provider_register(void) {
    register_projects_config_entries();
    cofi_init_provider_defaults(&s_projects_provider);
    s_projects_provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    s_projects_provider.id = PROJECTS_PROVIDER_ID;
    s_projects_provider.display_name = "PROJECTS";
    s_projects_provider.get_shortcut_hint = projects_get_shortcut_hint;
    s_projects_provider.prefix_char = 0;
    s_projects_provider.required = 0;
    s_projects_provider.hidden_by_default = 1;
    s_projects_provider.modal_policy = COFI_MODAL_HIDE_ON_ESC;
    s_projects_provider.initial_selection_index = 0;
    s_projects_provider.row_count = projects_row_count;
    s_projects_provider.format_row = projects_format_row;
    s_projects_provider.match_string = projects_match_string;
    s_projects_provider.row_identity = projects_row_identity;
    s_projects_provider.on_enter = projects_on_enter;
    s_projects_provider.on_query_changed = projects_on_query_changed;
    s_projects_provider.on_tick = projects_on_tick;
    s_projects_provider.tick_interval_ms = 1500;
    s_projects_provider.on_enter_pressed = projects_provider_on_enter_pressed;
    s_projects_provider.on_command_args = projects_provider_on_command_args;
    s_projects_provider.handle_key = handle_projects_tab_keys;
    s_projects_provider.slot_store_enabled = 1;
    s_projects_provider.slot_payload_for = projects_slot_payload_for;
    s_projects_provider.slot_recall = projects_slot_recall;
    s_projects_provider_id = cofi_register_tab_provider(&s_projects_provider);
    if (s_projects_provider_id >= 0) {
        cofi_register_command(&s_projects_command);
    }
}
