#include "path/path_provider.h"

#include <gtk/gtk.h>

#include "commands/command_mode.h"
#include "commands/command_registry.h"
#include "core/selection/selection.h"
#include "daemon/detach_launch.h"
#include "path/path_binaries.h"
#include "providers/cofi_tab_provider.h"
#include "ui/tab_switching.h"

static CofiTabProvider s_path_provider;
static int s_path_provider_id = -1;

TabMode path_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_path_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

static PathEntry *path_entry_at_row(AppData *app, int raw_idx) {
    if (!app || raw_idx < 0 || raw_idx >= app->filtered_path_count) {
        return NULL;
    }

    return &app->filtered_path[raw_idx];
}

static int path_row_count(AppData *app) {
    if (!app) {
        return 0;
    }

    if (path_binaries_is_scanning()) {
        return app->filtered_path_count + 1;
    }

    return app->filtered_path_count > 0 ? app->filtered_path_count : 1;
}

static void path_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    PathEntry *entry = path_entry_at_row(app, raw_idx);
    if (entry) {
        out->cell_count = 1;
        out->cells[0].text = entry->name;
        out->cells[0].width_hint = 0;
        out->row_flags = COFI_ROW_ACTIONABLE;
        return;
    }

    out->cell_count = 1;
    out->cells[0].text = path_binaries_is_scanning()
        ? "Scanning PATH..."
        : "No matching PATH executables found";
    out->row_flags = 0;
}

static const char *path_match_string(AppData *app, int raw_idx) {
    PathEntry *entry = path_entry_at_row(app, raw_idx);
    return entry ? entry->name : "";
}

static const char *path_row_identity(AppData *app, int raw_idx) {
    static char identity[640];
    PathEntry *entry = path_entry_at_row(app, raw_idx);
    if (!entry) {
        return "";
    }

    g_snprintf(identity, sizeof(identity), "path:%s", entry->exec_path);
    return identity;
}

static void launch_path_binary(const char *exec_path) {
    if (!exec_path || exec_path[0] == '\0') {
        return;
    }

    detach_launch_in_terminal_cmd(exec_path);
}

static void path_on_enter(AppData *app) {
    if (!app || !app->entry) {
        return;
    }

    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry),
                                   "Type to filter PATH executables...");
    path_binaries_ensure_loaded(app);
    path_binaries_filter("", app->filtered_path, &app->filtered_path_count);
}

static void path_on_query_changed(AppData *app, const char *query) {
    if (!app) {
        return;
    }

    path_binaries_filter(query, app->filtered_path, &app->filtered_path_count);
    reset_selection(app);
}

static CofiActionStatus path_on_enter_pressed(AppData *app,
                                              int filtered_idx,
                                              int raw_idx,
                                              const char *entry_text,
                                              int modifier_state) {
    (void)filtered_idx;
    (void)entry_text;
    (void)modifier_state;

    PathEntry *entry = path_entry_at_row(app, raw_idx);
    if (!entry) {
        return COFI_NO_OP;
    }

    launch_path_binary(entry->exec_path);
    return COFI_HANDLED_HIDE;
}

static gboolean path_command_handler(AppData *app,
                                     WindowInfo *window,
                                     const char *args) {
    (void)window;
    (void)args;

    if (!app) {
        return FALSE;
    }

    exit_command_mode(app);
    app->prefix_origin_tab = app->current_tab;
    surface_tab(app, path_tab_mode());
    return FALSE;
}

static const CommandSpec s_path_command = {
    .primary = "path",
    .aliases = {"binaries", "bin", "exe", NULL},
    .owner_provider_id = "path",
    .handler = path_command_handler,
    .description = "Switch to PATH executables tab",
    .help_format = "path, binaries, bin, exe",
    .keeps_open_on_hotkey_auto = 1
};

void path_provider_register(void) {
    cofi_init_provider_defaults(&s_path_provider);
    s_path_provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    s_path_provider.id = "path";
    s_path_provider.display_name = "PATH";
    s_path_provider.hidden_by_default = 1;
    s_path_provider.initial_selection_index = 0;
    s_path_provider.tab_prefix_chars = "$";
    s_path_provider.row_count = path_row_count;
    s_path_provider.format_row = path_format_row;
    s_path_provider.match_string = path_match_string;
    s_path_provider.row_identity = path_row_identity;
    s_path_provider.on_enter = path_on_enter;
    s_path_provider.on_query_changed = path_on_query_changed;
    s_path_provider.on_enter_pressed = path_on_enter_pressed;
    s_path_provider_id = cofi_register_tab_provider(&s_path_provider);
    if (s_path_provider_id >= 0) {
        cofi_register_command(&s_path_command);
    }
}
