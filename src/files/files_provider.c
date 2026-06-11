#include "files/files_provider.h"

#include "commands/command_mode.h"
#include "commands/command_registry.h"
#include "core/app/app_data.h"
#include "core/log/log.h"
#include "core/selection/selection.h"
#include "files/files_search.h"
#include "providers/cofi_tab_provider.h"
#include "ui/display.h"
#include "ui/tab_switching.h"

#include <gdk/gdkkeysyms.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <string.h>

static CofiTabProvider s_files_provider;
static int s_files_provider_id = -1;

#ifdef COFI_TESTING
typedef gboolean (*FilesOpenImpl)(const char *path);
static FilesOpenImpl s_open_impl = NULL;
#endif

static gboolean set_optional_executable_path(char *field, size_t field_size,
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
        g_snprintf(err_buf, err_size, "%s must be absolute, or empty to use PATH", key);
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

static int get_files_enabled(const CofiConfig *config, char *out, size_t out_size) {
    if (!config || !out || out_size == 0) return 0;
    g_strlcpy(out, config->files_enabled ? "true" : "false", out_size);
    return 1;
}

static int set_files_enabled(CofiConfig *config, const char *value,
                             char *err_buf, size_t err_size) {
    int parsed = 0;
    if (!parse_bool_setting(value, &parsed)) {
        g_snprintf(err_buf, err_size, "files.enabled must be true/false");
        return 0;
    }
    config->files_enabled = parsed;
    return 1;
}

static int get_files_fd_path(const CofiConfig *config, char *out, size_t out_size) {
    if (!config || !out || out_size == 0) return 0;
    g_strlcpy(out, config->files_fd_path, out_size);
    return 1;
}

static int set_files_fd_path(CofiConfig *config, const char *value,
                             char *err_buf, size_t err_size) {
    return set_optional_executable_path(config->files_fd_path,
                                        sizeof(config->files_fd_path),
                                        "files.fd_path",
                                        value,
                                        err_buf,
                                        err_size);
}

static int get_files_excludes(const CofiConfig *config, char *out, size_t out_size) {
    if (!config || !out || out_size == 0) return 0;
    g_strlcpy(out, config->files_excludes, out_size);
    return 1;
}

static int set_files_excludes(CofiConfig *config, const char *value,
                              char *err_buf __attribute__((unused)),
                              size_t err_size __attribute__((unused))) {
    if (!config || !value) return 0;
    g_strlcpy(config->files_excludes, value, sizeof(config->files_excludes));
    return 1;
}

static void register_files_config_entries(void) {
    static const CofiConfigSpec specs[] = {
        {
            .key = "files.enabled",
            .owner_provider_id = "files",
            .type = CONFIG_TYPE_BOOL,
            .get_value = get_files_enabled,
            .set_value = set_files_enabled,
            .get_display_value = get_files_enabled,
        },
        {
            .key = "files.fd_path",
            .owner_provider_id = "files",
            .type = CONFIG_TYPE_STRING,
            .get_value = get_files_fd_path,
            .set_value = set_files_fd_path,
            .get_display_value = get_files_fd_path,
        },
        {
            .key = "files.excludes",
            .owner_provider_id = "files",
            .type = CONFIG_TYPE_STRING,
            .get_value = get_files_excludes,
            .set_value = set_files_excludes,
            .get_display_value = get_files_excludes,
        },
    };
    for (size_t i = 0; i < sizeof(specs) / sizeof(specs[0]); i++) {
        cofi_register_config_entry(&specs[i]);
    }
}

static TabMode files_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_files_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

static int files_provider_row_count(AppData *app) {
    return files_row_count(app);
}

static void files_provider_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    const char *path = files_path_at_visible(app, raw_idx);
    if (!path) {
        out->cell_count = 1;
        out->cells[0].text = files_status_message(app);
        out->row_flags = files_status_is_error(app) ? COFI_ROW_ERROR : 0;
        return;
    }

    const char *basename = strrchr(path, '/');
    out->cell_count = 2;
    out->cells[0].text = basename ? basename + 1 : path;
    out->cells[0].width_hint = 28;
    out->cells[1].text = path;
    out->cells[1].width_hint = 72;
    out->row_flags = COFI_ROW_ACTIONABLE;
}

static const char *files_provider_match_string(AppData *app, int raw_idx) {
    return files_match_string(app, raw_idx);
}

static const char *files_provider_row_identity(AppData *app, int raw_idx) {
    return files_row_identity(app, raw_idx);
}

static gboolean open_with_xdg(const char *path) {
#ifdef COFI_TESTING
    if (s_open_impl) {
        return s_open_impl(path);
    }
#endif
    const gchar *argv[] = {"xdg-open", path, NULL};
    GError *error = NULL;
    GSubprocess *process = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_NONE, &error);
    if (!process) {
        log_warn("files: failed to launch xdg-open for %s: %s",
                 path ? path : "",
                 error ? error->message : "unknown error");
        g_clear_error(&error);
        return FALSE;
    }
    g_object_unref(process);
    return TRUE;
}

static void files_provider_on_enter(AppData *app) {
    if (!app) return;
    app->files_mode.tab_mode = files_tab_mode();
    if (app->entry) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "files...");
    }
    files_on_enter(app);
}

static void files_provider_on_leave(AppData *app) {
    files_on_leave(app);
}

static void files_provider_on_query_changed(AppData *app, const char *query) {
    files_on_query_changed(app, query);
    reset_selection(app);
}

static gboolean files_provider_handle_key(GdkEventKey *event, AppData *app) {
    if (!event || !app) return FALSE;
    switch (event->keyval) {
        case GDK_KEY_r:
        case GDK_KEY_R:
            files_search_refresh(app);
            update_display(app);
            return TRUE;
        default:
            return FALSE;
    }
}

static CofiActionStatus files_provider_on_enter_pressed(AppData *app,
                                                        int filtered_idx,
                                                        int raw_idx,
                                                        const char *entry_text,
                                                        int modifier_state) {
    (void)filtered_idx;
    (void)entry_text;
    (void)modifier_state;
    const char *path = files_path_at_visible(app, raw_idx);
    if (!path) return COFI_NO_OP;
    return open_with_xdg(path) ? COFI_HANDLED_HIDE : COFI_NO_OP;
}

static gboolean files_command_handler(AppData *app,
                                      WindowInfo *window __attribute__((unused)),
                                      const char *args __attribute__((unused))) {
    exit_command_mode(app);
    if (app) {
        app->prefix_origin_tab = app->current_tab;
    }
    surface_tab(app, files_tab_mode());
    return FALSE;
}

static const CommandSpec s_files_command = {
    .primary = "files",
    .aliases = {NULL},
    .owner_provider_id = "files",
    .handler = files_command_handler,
    .description = "Switch to files tab",
    .help_format = "files",
    .keeps_open_on_hotkey_auto = 1
};

void files_provider_register(void) {
    register_files_config_entries();
    cofi_init_provider_defaults(&s_files_provider);
    s_files_provider_id = -1;
    s_files_provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    s_files_provider.id = "files";
    s_files_provider.display_name = "FILES";
    s_files_provider.shortcut_hint = "Shortcuts: Enter=Open  r=Refresh";
    s_files_provider.hidden_by_default = 1;
    s_files_provider.row_count = files_provider_row_count;
    s_files_provider.format_row = files_provider_format_row;
    s_files_provider.match_string = files_provider_match_string;
    s_files_provider.row_identity = files_provider_row_identity;
    s_files_provider.on_enter = files_provider_on_enter;
    s_files_provider.on_leave = files_provider_on_leave;
    s_files_provider.on_query_changed = files_provider_on_query_changed;
    s_files_provider.handle_key = files_provider_handle_key;
    s_files_provider.on_enter_pressed = files_provider_on_enter_pressed;
    s_files_provider_id = cofi_register_tab_provider(&s_files_provider);
    if (s_files_provider_id >= 0) {
        cofi_register_command(&s_files_command);
    }
}
