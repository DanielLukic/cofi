#include "sessions_provider.h"

#include "sessions.h"
#include "app_data.h"
#include "command_mode.h"
#include "command_registry.h"
#include "cofi_tab_provider.h"
#include "display.h"
#include "overlay_manager.h"
#include "selection.h"
#include "tab_switching.h"

#include <gtk/gtk.h>

static SessionsMode s_sessions_mode;
static CofiTabProvider s_sessions_provider;
static int s_sessions_provider_id = -1;

TabMode sessions_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_sessions_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

static void sessions_changed(gpointer user_data) {
    AppData *app = (AppData *)user_data;
    if (!app || app->current_tab != sessions_tab_mode()) {
        return;
    }
    update_display(app);
}

static int sessions_row_count(AppData *app) {
    (void)app;
    return s_sessions_mode.filtered_count > 0 ? s_sessions_mode.filtered_count : 1;
}

static const SessionResult *session_at_row(int raw_idx) {
    return sessions_result_at(&s_sessions_mode, raw_idx);
}

static void sessions_select_path(AppData *app, const char *path) {
    if (!app || !path || path[0] == '\0') return;
    for (int i = 0; i < s_sessions_mode.filtered_count; i++) {
        const SessionResult *result =
            sessions_result_at(&s_sessions_mode, i);
        if (result && strcmp(result->path, path) == 0) {
            app->selection.provider_index = i;
            update_scroll_position(app);
            return;
        }
    }
    validate_selection(app);
    update_scroll_position(app);
}

static void sessions_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    (void)app;
    const SessionResult *result = session_at_row(raw_idx);
    if (!result) {
        out->cell_count = 1;
        out->cells[0].text = s_sessions_mode.status;
        out->row_flags = 0;
        return;
    }

    const char *title = result->display_name[0]
        ? result->display_name : result->session_id;
    const char *snippet = result->snippet;
    if (snippet && title && strcmp(snippet, title) == 0) {
        snippet = "";
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
    out->cells[4].text = title;
    out->cells[4].width_hint = 22;
    out->cells[5].text = snippet;
    out->cells[5].width_hint = 0;
    out->row_flags = COFI_ROW_ACTIONABLE;
}

static const char *sessions_match_string(AppData *app, int raw_idx) {
    (void)app;
    const SessionResult *result = session_at_row(raw_idx);
    return result ? result->search_text : "";
}

static const char *sessions_row_identity(AppData *app, int raw_idx) {
    (void)app;
    const SessionResult *result = session_at_row(raw_idx);
    return result ? result->path : "";
}

static void sessions_on_enter(AppData *app) {
    if (app && app->entry) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry),
                                       "Search sessions: terms | refine...");
        const char *query = gtk_entry_get_text(GTK_ENTRY(app->entry));
        sessions_cancel(&s_sessions_mode);
        s_sessions_mode.current_left[0] = '\0';
        sessions_search(&s_sessions_mode, query, sessions_changed, app);
    }
}

static void sessions_on_leave(AppData *app) {
    (void)app;
    sessions_cancel(&s_sessions_mode);
}

static void sessions_on_query_changed(AppData *app, const char *query) {
    sessions_search(&s_sessions_mode, query, sessions_changed, app);
    reset_selection(app);
}

static CofiActionStatus sessions_on_enter_pressed(AppData *app,
                                                        int filtered_idx,
                                                        int raw_idx,
                                                        const char *entry_text,
                                                        int modifier_state) {
    (void)app;
    (void)filtered_idx;
    (void)entry_text;
    (void)modifier_state;
    const SessionResult *result = session_at_row(raw_idx);
    if (!result) return COFI_NO_OP;
    return sessions_launch_result(result) ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

void sessions_provider_remove_path(AppData *app, const char *path) {
    if (!path || path[0] == '\0') return;
    sessions_cancel(&s_sessions_mode);
    sessions_remove_path(&s_sessions_mode, path);
    if (app) {
        validate_selection(app);
        update_scroll_position(app);
        update_display(app);
    }
}

void sessions_provider_rename_path(AppData *app,
                                         const char *path,
                                         const char *name) {
    if (!path || path[0] == '\0' || !name) return;
    sessions_rename_path(&s_sessions_mode, path, name);
    if (app) {
        sessions_select_path(app, path);
        update_display(app);
    }
}

static gboolean sessions_handle_key(GdkEventKey *event, AppData *app) {
    if (!app || app->current_tab != sessions_tab_mode()) {
        return FALSE;
    }

    gboolean rename_key =
        (event->state & GDK_CONTROL_MASK) &&
        (event->keyval == GDK_KEY_r || event->keyval == GDK_KEY_R);
    gboolean delete_key =
        event->keyval == GDK_KEY_Delete ||
        event->keyval == GDK_KEY_KP_Delete ||
        ((event->state & GDK_CONTROL_MASK) &&
         (event->keyval == GDK_KEY_d || event->keyval == GDK_KEY_D));
    if (!delete_key && !rename_key) {
        return FALSE;
    }

    const SessionResult *result =
        session_at_row(app->selection.provider_index);
    if (!result) {
        return FALSE;
    }
    if (rename_key) {
        if ((strcmp(result->source, "claude") != 0 &&
             strcmp(result->source, "codex") != 0) ||
            strcmp(result->session_id, "history") == 0) {
            return FALSE;
        }
        show_session_rename_overlay(app, result->source,
                                          result->session_id, result->path,
                                          result->display_name);
        return TRUE;
    }
    show_session_delete_overlay(app, result->source,
                                      result->session_id, result->path);
    return TRUE;
}

static gboolean sessions_command_handler(AppData *app,
                                               WindowInfo *window __attribute__((unused)),
                                               const char *args __attribute__((unused))) {
    exit_command_mode(app);
    if (app) {
        app->prefix_origin_tab = app->current_tab;
    }
    surface_tab(app, sessions_tab_mode());
    return FALSE;
}

static const CommandSpec s_sessions_command = {
    .primary = "sessions",
    .aliases = {"session", NULL},
    .owner_provider_id = "sessions",
    .handler = sessions_command_handler,
    .description = "Search Claude and Codex sessions",
    .help_format = "sessions, session [TERMS | REFINE]",
    .keeps_open_on_hotkey_auto = 1
};

void sessions_provider_register(void) {
    cofi_init_provider_defaults(&s_sessions_provider);
    sessions_init(&s_sessions_mode);
    s_sessions_provider_id = -1;
    s_sessions_provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    s_sessions_provider.id = "sessions";
    s_sessions_provider.display_name = "SESSIONS";
    s_sessions_provider.shortcut_hint =
        "Search: terms | refine   Enter=Resume  Ctrl+R=Rename  Ctrl+D/Delete=Delete";
    s_sessions_provider.required = 0;
    s_sessions_provider.hidden_by_default = 1;
    s_sessions_provider.modal_policy = COFI_MODAL_HIDE_ON_ESC;
    s_sessions_provider.initial_selection_index = 0;
    s_sessions_provider.row_count = sessions_row_count;
    s_sessions_provider.format_row = sessions_format_row;
    s_sessions_provider.match_string = sessions_match_string;
    s_sessions_provider.row_identity = sessions_row_identity;
    s_sessions_provider.on_enter = sessions_on_enter;
    s_sessions_provider.on_leave = sessions_on_leave;
    s_sessions_provider.on_query_changed = sessions_on_query_changed;
    s_sessions_provider.on_enter_pressed = sessions_on_enter_pressed;
    s_sessions_provider.handle_key = sessions_handle_key;

    s_sessions_provider_id = cofi_register_tab_provider(&s_sessions_provider);
    if (s_sessions_provider_id >= 0) {
        cofi_register_command(&s_sessions_command);
    }
}
