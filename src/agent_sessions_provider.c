#include "agent_sessions_provider.h"

#include "agent_sessions.h"
#include "app_data.h"
#include "command_mode.h"
#include "command_registry.h"
#include "cofi_tab_provider.h"
#include "display.h"
#include "overlay_manager.h"
#include "selection.h"
#include "tab_switching.h"

#include <gtk/gtk.h>

static AgentSessionsMode s_agent_sessions_mode;
static CofiTabProvider s_agent_sessions_provider;
static int s_agent_sessions_provider_id = -1;

TabMode agent_sessions_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_agent_sessions_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

static void agent_sessions_changed(gpointer user_data) {
    AppData *app = (AppData *)user_data;
    if (!app || app->current_tab != agent_sessions_tab_mode()) {
        return;
    }
    update_display(app);
}

static int agent_sessions_row_count(AppData *app) {
    (void)app;
    return s_agent_sessions_mode.filtered_count > 0 ? s_agent_sessions_mode.filtered_count : 1;
}

static const AgentSessionResult *agent_session_at_row(int raw_idx) {
    return agent_sessions_result_at(&s_agent_sessions_mode, raw_idx);
}

static void agent_sessions_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    (void)app;
    const AgentSessionResult *result = agent_session_at_row(raw_idx);
    if (!result) {
        out->cell_count = 1;
        out->cells[0].text = s_agent_sessions_mode.status;
        out->row_flags = 0;
        return;
    }

    out->cell_count = 6;
    out->cells[0].text = result->source;
    out->cells[0].width_hint = 6;
    out->cells[1].text = result->hit_text;
    out->cells[1].width_hint = 4;
    out->cells[1].align = 1;
    out->cells[2].text = result->modified_text;
    out->cells[2].width_hint = 11;
    out->cells[3].text = result->project_label;
    out->cells[3].width_hint = 16;
    out->cells[4].text = result->display_name[0]
        ? result->display_name : result->session_id;
    out->cells[4].width_hint = 22;
    out->cells[5].text = result->snippet;
    out->cells[5].width_hint = 0;
    out->row_flags = COFI_ROW_ACTIONABLE;
}

static const char *agent_sessions_match_string(AppData *app, int raw_idx) {
    (void)app;
    const AgentSessionResult *result = agent_session_at_row(raw_idx);
    return result ? result->search_text : "";
}

static const char *agent_sessions_row_identity(AppData *app, int raw_idx) {
    (void)app;
    const AgentSessionResult *result = agent_session_at_row(raw_idx);
    return result ? result->path : "";
}

static void agent_sessions_on_enter(AppData *app) {
    if (app && app->entry) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry),
                                       "Search agent sessions: terms | refine...");
        const char *query = gtk_entry_get_text(GTK_ENTRY(app->entry));
        agent_sessions_cancel(&s_agent_sessions_mode);
        s_agent_sessions_mode.current_left[0] = '\0';
        agent_sessions_search(&s_agent_sessions_mode, query, agent_sessions_changed, app);
    }
}

static void agent_sessions_on_leave(AppData *app) {
    (void)app;
    agent_sessions_cancel(&s_agent_sessions_mode);
}

static void agent_sessions_on_query_changed(AppData *app, const char *query) {
    agent_sessions_search(&s_agent_sessions_mode, query, agent_sessions_changed, app);
    reset_selection(app);
}

static CofiActionStatus agent_sessions_on_enter_pressed(AppData *app,
                                                        int filtered_idx,
                                                        int raw_idx,
                                                        const char *entry_text,
                                                        int modifier_state) {
    (void)app;
    (void)filtered_idx;
    (void)entry_text;
    (void)modifier_state;
    const AgentSessionResult *result = agent_session_at_row(raw_idx);
    if (!result) return COFI_NO_OP;
    return agent_sessions_launch_result(result) ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

void agent_sessions_provider_remove_path(AppData *app, const char *path) {
    if (!path || path[0] == '\0') return;
    agent_sessions_cancel(&s_agent_sessions_mode);
    agent_sessions_remove_path(&s_agent_sessions_mode, path);
    if (app) {
        validate_selection(app);
        update_scroll_position(app);
        update_display(app);
    }
}

void agent_sessions_provider_rename_path(AppData *app,
                                         const char *path,
                                         const char *name) {
    if (!path || path[0] == '\0' || !name) return;
    agent_sessions_rename_path(&s_agent_sessions_mode, path, name);
    if (app) {
        validate_selection(app);
        update_scroll_position(app);
        update_display(app);
    }
}

static gboolean agent_sessions_handle_key(GdkEventKey *event, AppData *app) {
    if (!app || app->current_tab != agent_sessions_tab_mode()) {
        return FALSE;
    }

    gboolean rename_key =
        (event->state & GDK_CONTROL_MASK) &&
        (event->keyval == GDK_KEY_e || event->keyval == GDK_KEY_E);
    gboolean delete_key =
        event->keyval == GDK_KEY_Delete ||
        event->keyval == GDK_KEY_KP_Delete ||
        ((event->state & GDK_CONTROL_MASK) &&
         (event->keyval == GDK_KEY_d || event->keyval == GDK_KEY_D));
    if (!delete_key && !rename_key) {
        return FALSE;
    }

    const AgentSessionResult *result =
        agent_session_at_row(app->selection.provider_index);
    if (!result) {
        return FALSE;
    }
    if (rename_key) {
        if (strcmp(result->source, "claude") != 0 ||
            strcmp(result->session_id, "history") == 0) {
            return FALSE;
        }
        show_agent_session_rename_overlay(app, result->source,
                                          result->session_id, result->path,
                                          result->display_name);
        return TRUE;
    }
    show_agent_session_delete_overlay(app, result->source,
                                      result->session_id, result->path);
    return TRUE;
}

static gboolean agent_sessions_command_handler(AppData *app,
                                               WindowInfo *window __attribute__((unused)),
                                               const char *args __attribute__((unused))) {
    exit_command_mode(app);
    if (app) {
        app->prefix_origin_tab = app->current_tab;
    }
    surface_tab(app, agent_sessions_tab_mode());
    return FALSE;
}

static const CommandSpec s_agent_sessions_command = {
    .primary = "agent-sessions",
    .aliases = {"agents", "agent", NULL},
    .owner_provider_id = "agent-sessions",
    .handler = agent_sessions_command_handler,
    .description = "Search Claude and Codex agent sessions",
    .help_format = "agent-sessions, agents [TERMS | REFINE]",
    .keeps_open_on_hotkey_auto = 1
};

void agent_sessions_provider_register(void) {
    cofi_init_provider_defaults(&s_agent_sessions_provider);
    agent_sessions_init(&s_agent_sessions_mode);
    s_agent_sessions_provider_id = -1;
    s_agent_sessions_provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    s_agent_sessions_provider.id = "agent-sessions";
    s_agent_sessions_provider.display_name = "AGENTS";
    s_agent_sessions_provider.shortcut_hint =
        "Search: terms | refine   Enter=Resume  Ctrl+E=Rename Claude  Ctrl+D/Delete=Delete";
    s_agent_sessions_provider.required = 0;
    s_agent_sessions_provider.hidden_by_default = 1;
    s_agent_sessions_provider.modal_policy = COFI_MODAL_HIDE_ON_ESC;
    s_agent_sessions_provider.initial_selection_index = 0;
    s_agent_sessions_provider.row_count = agent_sessions_row_count;
    s_agent_sessions_provider.format_row = agent_sessions_format_row;
    s_agent_sessions_provider.match_string = agent_sessions_match_string;
    s_agent_sessions_provider.row_identity = agent_sessions_row_identity;
    s_agent_sessions_provider.on_enter = agent_sessions_on_enter;
    s_agent_sessions_provider.on_leave = agent_sessions_on_leave;
    s_agent_sessions_provider.on_query_changed = agent_sessions_on_query_changed;
    s_agent_sessions_provider.on_enter_pressed = agent_sessions_on_enter_pressed;
    s_agent_sessions_provider.handle_key = agent_sessions_handle_key;

    s_agent_sessions_provider_id = cofi_register_tab_provider(&s_agent_sessions_provider);
    if (s_agent_sessions_provider_id >= 0) {
        cofi_register_command(&s_agent_sessions_command);
    }
}
