#include "projects/projects_provider.h"

#include "core/app/app_data.h"
#include "commands/command_mode.h"
#include "commands/command_registry.h"
#include "providers/cofi_tab_provider.h"
#include "config/config.h"
#include "ui/display.h"
#include "ui/overlay_manager.h"
#include "projects/projects.h"
#include "projects/projects_parse.h"
#include "projects/projects_remote_store.h"
#include "projects/projects_remote_scope.h"
#include "core/selection/selection.h"
#include "core/slot_store/slot_store.h"
#include "ui/tab_switching.h"
#include "ui/window_lifecycle.h"
#include <stdlib.h>
#include <string.h>

static CofiTabProvider s_projects_provider;
static int s_projects_provider_id = -1;
#define PROJECTS_PROVIDER_ID "projects"

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

static int parse_bool_setting(const char *value, int *out) {
    if (!value || !out) return 0;
    if (strcmp(value, "1") == 0 || g_ascii_strcasecmp(value, "true") == 0 ||
        g_ascii_strcasecmp(value, "on") == 0 || g_ascii_strcasecmp(value, "yes") == 0) {
        *out = 1;
        return 1;
    }
    if (strcmp(value, "0") == 0 || g_ascii_strcasecmp(value, "false") == 0 ||
        g_ascii_strcasecmp(value, "off") == 0 || g_ascii_strcasecmp(value, "no") == 0) {
        *out = 0;
        return 1;
    }
    return 0;
}

static int parse_int_setting(const char *value, int min_value, int max_value, int *out) {
    if (!value || !out) return 0;
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (!end || *end != '\0' || parsed < min_value || parsed > max_value) return 0;
    *out = (int)parsed;
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

static int get_locate_enabled(const CofiConfig *config, char *out, size_t out_size) {
    if (!config || !out || out_size == 0) return 0;
    g_strlcpy(out, config->projects_locate_enabled ? "true" : "false", out_size);
    return 1;
}

static int set_locate_enabled(CofiConfig *config, const char *value,
                              char *err_buf, size_t err_size) {
    int parsed = 0;
    if (!parse_bool_setting(value, &parsed)) {
        g_snprintf(err_buf, err_size, "projects.locate_enabled must be true/false");
        return 0;
    }
    config->projects_locate_enabled = parsed;
    return 1;
}

static int get_locate_excludes(const CofiConfig *config, char *out, size_t out_size) {
    if (!config || !out || out_size == 0) return 0;
    g_strlcpy(out, config->projects_locate_excludes, out_size);
    return 1;
}

static int set_locate_excludes(CofiConfig *config, const char *value,
                               char *err_buf __attribute__((unused)),
                               size_t err_size __attribute__((unused))) {
    if (!config || !value) return 0;
    g_strlcpy(config->projects_locate_excludes, value, sizeof(config->projects_locate_excludes));
    return 1;
}

static int get_locate_search_roots(const CofiConfig *config, char *out, size_t out_size) {
    if (!config || !out || out_size == 0) return 0;
    g_strlcpy(out, config->projects_locate_search_roots, out_size);
    return 1;
}

static int set_locate_search_roots(CofiConfig *config, const char *value,
                                   char *err_buf __attribute__((unused)),
                                   size_t err_size __attribute__((unused))) {
    if (!config || !value) return 0;
    g_strlcpy(config->projects_locate_search_roots, value,
              sizeof(config->projects_locate_search_roots));
    return 1;
}

static int get_locate_timeout_ms(const CofiConfig *config, char *out, size_t out_size) {
    if (!config || !out || out_size == 0) return 0;
    g_snprintf(out, out_size, "%d", config->projects_locate_timeout_ms);
    return 1;
}

static int set_locate_timeout_ms(CofiConfig *config, const char *value,
                                 char *err_buf, size_t err_size) {
    int parsed = 0;
    if (!parse_int_setting(value, 100, 60000, &parsed)) {
        g_snprintf(err_buf, err_size, "projects.locate_timeout_ms must be 100-60000");
        return 0;
    }
    config->projects_locate_timeout_ms = parsed;
    return 1;
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
        {
            .key = "projects.locate_enabled",
            .owner_provider_id = PROJECTS_PROVIDER_ID,
            .type = CONFIG_TYPE_BOOL,
            .get_value = get_locate_enabled,
            .set_value = set_locate_enabled,
            .get_display_value = get_locate_enabled,
        },
        {
            .key = "projects.locate_excludes",
            .owner_provider_id = PROJECTS_PROVIDER_ID,
            .type = CONFIG_TYPE_STRING,
            .get_value = get_locate_excludes,
            .set_value = set_locate_excludes,
            .get_display_value = get_locate_excludes,
        },
        {
            .key = "projects.locate_search_roots",
            .owner_provider_id = PROJECTS_PROVIDER_ID,
            .type = CONFIG_TYPE_STRING,
            .get_value = get_locate_search_roots,
            .set_value = set_locate_search_roots,
            .get_display_value = get_locate_search_roots,
        },
        {
            .key = "projects.locate_timeout_ms",
            .owner_provider_id = PROJECTS_PROVIDER_ID,
            .type = CONFIG_TYPE_INT,
            .get_value = get_locate_timeout_ms,
            .set_value = set_locate_timeout_ms,
            .get_display_value = get_locate_timeout_ms,
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

static gboolean handle_delete_selected_entry(AppData *app) {
    ProjectSessionEntry *session = projects_selected_session(app);
    ProjectFolder *folder = projects_selected_folder(app);

    if (!session && !folder) {
        return TRUE;
    }

    if (session) {
        if (session->is_saved_remote) {
            app->project_kill.pending_kill = TRUE;
            app->project_kill.action = PROJECT_DELETE_FORGET_REMOTE;
            app->project_kill.backend = session->backend;
            g_strlcpy(app->project_kill.session_name, session->name,
                      sizeof(app->project_kill.session_name));
            g_strlcpy(app->project_kill.remote_host, session->remote_host,
                      sizeof(app->project_kill.remote_host));
            g_strlcpy(app->project_kill.remote_cwd, session->remote_cwd,
                      sizeof(app->project_kill.remote_cwd));
            show_overlay(app, OVERLAY_PROJECT_KILL, NULL);
            return TRUE;
        }

        show_project_kill_overlay(app, session->name, session->backend);
        return TRUE;
    }

    app->project_kill.pending_kill = TRUE;
    if (folder->source == FOLDER_SOURCE_LOCATE) {
        return TRUE;
    }
    app->project_kill.action = PROJECT_DELETE_REMOVE_FOLDER;
    app->project_kill.backend = PROJECT_BACKEND_TMUX;
    g_strlcpy(app->project_kill.folder_path, folder->path ? folder->path : "",
              sizeof(app->project_kill.folder_path));
    app->project_kill.folder_is_remote = folder->is_remote;
    g_strlcpy(app->project_kill.remote_host,
              folder->is_remote ? folder->remote_host : "",
              sizeof(app->project_kill.remote_host));
    app->project_kill.remote_cwd[0] = '\0';
    app->project_kill.session_name[0] = '\0';
    show_overlay(app, OVERLAY_PROJECT_KILL, NULL);
    return TRUE;
}

static void show_new_session_for_selection(AppData *app, gboolean prefer_zellij) {
    projects_remote_scope_clear_status_message();
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

    gboolean ctrl_s =
        (event->state & GDK_CONTROL_MASK) &&
        !(event->state & GDK_SHIFT_MASK) &&
        (event->keyval == GDK_KEY_s || event->keyval == GDK_KEY_S);
    if (ctrl_s) {
        projects_remote_scope_clear_status_message();
        show_project_remote_host_overlay(app);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Escape && projects_remote_scope_is_active()) {
        projects_remote_scope_clear_status_message();
        projects_remote_scope_clear();
        projects_refresh(app);
        reset_selection(app);
        update_scroll_position(app);
        update_display(app);
        return TRUE;
    }

    gboolean ctrl_t =
        (event->state & GDK_CONTROL_MASK) &&
        !(event->state & GDK_SHIFT_MASK) &&
        (event->keyval == GDK_KEY_t || event->keyval == GDK_KEY_T);
    if (ctrl_t) {
        projects_remote_scope_clear_status_message();
        ProjectFolder *folder = projects_selected_folder(app);
        if (!folder) return FALSE; /* not a folder: let Ctrl+T fall through to harpoon slot 't' */

        CofiActionStatus status = projects_open_folder_terminal(app, folder);
        if (status == COFI_HANDLED_HIDE) {
            hide_window(app);
            return TRUE;
        }
        if (status == COFI_HANDLED_REFRESH) {
            projects_refresh(app);
            reset_selection(app);
            update_scroll_position(app);
            update_display(app);
            return TRUE;
        }
        return status == COFI_NO_OP ? TRUE : (status == COFI_ACTION_ERROR);
    }

    if (event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete) {
        projects_remote_scope_clear_status_message();
        return handle_delete_selected_entry(app);
    }

    gboolean ctrl_d =
        (event->state & GDK_CONTROL_MASK) &&
        !(event->state & GDK_SHIFT_MASK) &&
        (event->keyval == GDK_KEY_d || event->keyval == GDK_KEY_D);
    if (ctrl_d) {
        projects_remote_scope_clear_status_message();
        return handle_delete_selected_entry(app);
    }

    gboolean ctrl_r =
        (event->state & GDK_CONTROL_MASK) &&
        !(event->state & GDK_SHIFT_MASK) &&
        (event->keyval == GDK_KEY_r || event->keyval == GDK_KEY_R);
    if (event->keyval == GDK_KEY_F2 || ctrl_r) {
        projects_remote_scope_clear_status_message();
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
    projects_remote_scope_clear_status_message();
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
    projects_remote_scope_clear_status_message();
    projects_refresh(app);

    if (projects_has_named(app, args)) {
        return projects_attach_named(app, args);
    }

    char slot = '\0';
    if (slot_parse_at_key_arg(args, &slot)) {
        const char *payload = slot_lookup(&app->harpoon.store, PROJECTS_PROVIDER_ID, slot);
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
            return TRUE;
        }
        if (status == COFI_ACTION_ERROR || status == COFI_NO_OP) {
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
    .closes_cofi_after_execute = 1,
    .keeps_open_on_hotkey_auto = 1
};

void projects_provider_register(void) {
    projects_remote_store_init();
    projects_remote_store_reload();
    projects_remote_scope_init();
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
    s_projects_provider.on_leave = projects_on_leave;
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
