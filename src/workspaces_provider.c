#include "workspaces_provider.h"

#include <stdio.h>
#include <string.h>

#include "command_mode.h"
#include "command_registry.h"
#include "cofi_tab_provider.h"
#include "log.h"
#include "match.h"
#include "selection.h"
#include "tab_switching.h"
#include "x11_utils.h"

static gboolean workspaces_command_handler(AppData *app, WindowInfo *window, const char *args);

static int s_workspaces_provider_id = -1;

static CofiTabProvider s_workspaces_provider = {
    .tab_mode = COFI_PROVIDER_DYNAMIC_TAB,
    .id = "workspaces",
    .display_name = "WORKSPACES",
    .required = 0,
    .hidden_by_default = 1,
};

static const CommandSpec s_workspaces_command = {
    .primary = "workspaces",
    .aliases = {"ws", NULL},
    .owner_provider_id = "workspaces",
    .handler = workspaces_command_handler,
    .description = "Switch to Workspaces tab",
    .help_format = "workspaces, ws",
    .keeps_open_on_hotkey_auto = 1
};

TabMode workspaces_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_workspaces_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

void filter_workspaces(AppData *app, const char *filter) {
    if (!app) return;

    app->filtered_workspace_count = 0;

    char searchable[512];
    for (int i = 0; i < app->workspace_count; i++) {
        WorkspaceInfo *workspace = &app->workspaces[i];
        snprintf(searchable, sizeof(searchable), "%d %s",
                 workspace->id + 1, workspace->name);

        if (!filter || !*filter || has_match(filter, searchable)) {
            app->filtered_workspaces[app->filtered_workspace_count++] = *workspace;
        }
    }
}

static int workspaces_row_count(AppData *app) {
    if (!app) return 0;
    return app->filtered_workspace_count > 0 ? app->filtered_workspace_count : 1;
}

static WorkspaceInfo *workspace_at_row(AppData *app, int raw_idx) {
    if (!app || raw_idx < 0 || raw_idx >= app->filtered_workspace_count) {
        return NULL;
    }
    return &app->filtered_workspaces[raw_idx];
}

WorkspaceInfo *workspaces_selected_workspace(AppData *app) {
    if (!app || app->filtered_workspace_count <= 0) return NULL;

    int idx = app->selection.provider_index;
    if (idx < 0) idx = 0;
    if (idx >= app->filtered_workspace_count) {
        idx = app->filtered_workspace_count - 1;
    }
    app->selection.provider_index = idx;

    return workspace_at_row(app, idx);
}

static void workspaces_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    WorkspaceInfo *workspace = workspace_at_row(app, raw_idx);
    if (!workspace) {
        out->cell_count = 1;
        out->cells[0].text = "No matching workspaces found";
        return;
    }

    static char marker[2];
    static char number[16];
    marker[0] = workspace->is_current ? '*' : ' ';
    marker[1] = '\0';
    snprintf(number, sizeof(number), "[%d]", workspace->id + 1);

    out->cell_count = 3;
    out->cells[0].text = marker;
    out->cells[0].width_hint = 1;
    out->cells[1].text = number;
    out->cells[1].width_hint = 5;
    out->cells[2].text = workspace->name;
    out->row_flags = COFI_ROW_ACTIONABLE;
}

static const char *workspaces_match_string(AppData *app, int raw_idx) {
    WorkspaceInfo *workspace = workspace_at_row(app, raw_idx);
    static char searchable[512];
    if (!workspace) return "";

    snprintf(searchable, sizeof(searchable), "%d %s",
             workspace->id + 1, workspace->name);
    return searchable;
}

static const char *workspaces_row_identity(AppData *app, int raw_idx) {
    WorkspaceInfo *workspace = workspace_at_row(app, raw_idx);
    static char identity[32];
    if (!workspace) return "";

    snprintf(identity, sizeof(identity), "workspace:%d", workspace->id);
    return identity;
}

static void workspaces_on_enter(AppData *app) {
    if (!app || !app->entry) return;
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry),
                                   "Type to filter workspaces...");
    filter_workspaces(app, "");
}

static void workspaces_on_query_changed(AppData *app, const char *query) {
    filter_workspaces(app, query);
    reset_selection(app);
}

static CofiActionStatus workspaces_on_enter_pressed(AppData *app,
                                                    int filtered_idx,
                                                    int raw_idx,
                                                    const char *entry_text,
                                                    int modifier_state) {
    (void)filtered_idx;
    (void)entry_text;
    (void)modifier_state;

    WorkspaceInfo *workspace = workspace_at_row(app, raw_idx);
    if (!workspace) return COFI_NO_OP;

    log_info("USER: ENTER pressed -> Switching to workspace %d: %s",
             workspace->id, workspace->name);
    switch_to_desktop(app->display, workspace->id);
    return COFI_HANDLED_HIDE;
}

static gboolean workspaces_command_handler(AppData *app,
                                           WindowInfo *window __attribute__((unused)),
                                           const char *args __attribute__((unused))) {
    if (!app) return FALSE;
    exit_command_mode(app);
    app->prefix_origin_tab = app->current_tab;
    surface_tab(app, workspaces_tab_mode());
    return FALSE;
}

void workspaces_provider_register(void) {
    CofiTabProvider provider;
    provider = s_workspaces_provider;
    provider.row_count = workspaces_row_count;
    provider.format_row = workspaces_format_row;
    provider.match_string = workspaces_match_string;
    provider.row_identity = workspaces_row_identity;
    provider.on_enter = workspaces_on_enter;
    provider.on_query_changed = workspaces_on_query_changed;
    provider.on_enter_pressed = workspaces_on_enter_pressed;
    s_workspaces_provider_id = cofi_register_tab_provider(&provider);
    if (s_workspaces_provider_id >= 0) {
        cofi_register_command(&s_workspaces_command);
    }
}
