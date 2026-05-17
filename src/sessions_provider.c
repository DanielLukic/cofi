#include "sessions_provider.h"

#include "app_data.h"
#include "command_mode.h"
#include "cofi_tab_provider.h"
#include "overlay_manager.h"
#include "sessions.h"
#include "sessions_parse.h"
#include "slot_store.h"
#include "tab_switching.h"
#include "window_lifecycle.h"

static const char *const s_sessions_aliases[] = {"tmux", "tx", "zj", "zellij", NULL};
static CofiTabProvider s_sessions_provider;

static void show_new_session_for_selection(AppData *app, gboolean prefer_zellij) {
    SessionBackend backend = prefer_zellij ? SESSION_BACKEND_ZELLIJ : SESSION_BACKEND_TMUX;
    SessionEntry *session = sessions_selected_session(app);
    if (!prefer_zellij && session && session->backend == SESSION_BACKEND_ZELLIJ) {
        backend = SESSION_BACKEND_ZELLIJ;
    }

    SessionFolder *folder = sessions_selected_folder(app);
    if (!folder) {
        show_session_new_overlay(app, backend, "", "");
        return;
    }

    gchar *session_name = sessions_build_folder_session_name(folder->path);
    show_session_new_overlay(app, backend, folder->path, session_name);
    g_free(session_name);
}

gboolean handle_sessions_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != TAB_SESSIONS) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_Insert || event->keyval == GDK_KEY_KP_Insert) {
        show_new_session_for_selection(app, (event->state & GDK_SHIFT_MASK) != 0);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete) {
        SessionEntry *session = sessions_selected_session(app);
        if (!session) {
            return FALSE;
        }
        show_session_kill_overlay(app, session->name, session->backend);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_F2) {
        SessionEntry *session = sessions_selected_session(app);
        if (!session || session->backend != SESSION_BACKEND_TMUX) {
            return FALSE;
        }
        show_session_rename_overlay(app, session->name);
        return TRUE;
    }

    return FALSE;
}

static CofiActionStatus sessions_provider_on_enter_pressed(AppData *app, int filtered_idx,
                                                       int raw_idx,
                                                       const char *entry_text,
                                                       int modifier_state) {
    (void)filtered_idx;
    (void)entry_text;
    (void)modifier_state;
    /* raw_idx is visible here: Sessions exposes its filtered list directly to the provider renderer. */
    SessionFolder *folder = sessions_folder_at_visible(app, raw_idx);
    if (folder) {
        return sessions_open_folder(app, folder->path);
    }
    return sessions_attach_visible(app, raw_idx);
}

static CofiActionStatus sessions_provider_on_command_args(AppData *app, const char *args) {
    if (!app) return COFI_NO_OP;
    if (!args || args[0] == '\0') return COFI_NO_OP;
    sessions_refresh(app);

    if (sessions_has_named(app, args)) {
        return sessions_attach_named(app, args);
    }

    char slot = '\0';
    if (slot_parse_at_key_arg(args, &slot)) {
        const char *payload = slot_lookup(&app->harpoon.store, "sessions", slot);
        if (!payload) return COFI_ACTION_ERROR;
        return sessions_slot_recall(app, payload);
    }
    return COFI_ACTION_ERROR;
}

static void sessions_show_command_error(AppData *app, const char *message) {
    if (!app || !app->textbuffer) return;
    gtk_text_buffer_set_text(app->textbuffer, message, -1);
    app->command_mode.showing_help = TRUE;
}

static gboolean sessions_command_handler(AppData *app,
                                         WindowInfo *window __attribute__((unused)),
                                         const char *args) {
    exit_command_mode(app);

    if (args && args[0] != '\0') {
        CofiActionStatus status = sessions_provider_on_command_args(app, args);
        if (status == COFI_HANDLED_HIDE) {
            hide_window(app);
        } else if (status == COFI_ACTION_ERROR || status == COFI_NO_OP) {
            sessions_show_command_error(app, "No matching tmux/zellij session.");
        }
        return FALSE;
    }

    if (app) {
        app->prefix_origin_tab = app->current_tab;
    }
    surface_tab(app, (TabMode)s_sessions_provider.tab_mode);
    return FALSE;
}

void sessions_provider_register(void) {
    cofi_init_provider_defaults(&s_sessions_provider);
    s_sessions_provider.tab_mode = TAB_SESSIONS;
    s_sessions_provider.id = "sessions";
    s_sessions_provider.display_name = "SESSIONS";
    s_sessions_provider.get_shortcut_hint = sessions_get_shortcut_hint;
    s_sessions_provider.primary_cmd = "sessions";
    s_sessions_provider.aliases = s_sessions_aliases;
    s_sessions_provider.command_description = "Switch to sessions tab";
    s_sessions_provider.command_help_format = "sessions, tmux, tx, zj, zellij [@SLOT|SESSION]";
    s_sessions_provider.command_handler = sessions_command_handler;
    s_sessions_provider.command_keeps_open_on_hotkey_auto = 1;
    s_sessions_provider.prefix_char = 0;
    s_sessions_provider.required = 0;
    s_sessions_provider.hidden_by_default = 1;
    s_sessions_provider.modal_policy = COFI_MODAL_HIDE_ON_ESC;
    s_sessions_provider.initial_selection_index = 0;
    s_sessions_provider.row_count = sessions_row_count;
    s_sessions_provider.format_row = sessions_format_row;
    s_sessions_provider.match_string = sessions_match_string;
    s_sessions_provider.row_identity = sessions_row_identity;
    s_sessions_provider.on_enter = sessions_on_enter;
    s_sessions_provider.on_query_changed = sessions_on_query_changed;
    s_sessions_provider.on_tick = sessions_on_tick;
    s_sessions_provider.tick_interval_ms = 1500;
    s_sessions_provider.on_enter_pressed = sessions_provider_on_enter_pressed;
    s_sessions_provider.on_command_args = sessions_provider_on_command_args;
    s_sessions_provider.handle_key = handle_sessions_tab_keys;
    s_sessions_provider.slot_store_enabled = 1;
    s_sessions_provider.slot_payload_for = sessions_slot_payload_for;
    s_sessions_provider.slot_recall = sessions_slot_recall;
    cofi_register_tab_provider(&s_sessions_provider);
}
