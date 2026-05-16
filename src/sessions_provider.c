#include "sessions_provider.h"

#include "app_data.h"
#include "cofi_tab_provider.h"
#include "key_handler_tabs.h"
#include "sessions.h"
#include "slot_store.h"

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
    if (slot_parse_single_key_arg(args, &slot)) {
        const char *payload = slot_lookup(&app->harpoon.store, "sessions", slot);
        if (!payload) return COFI_ACTION_ERROR;
        return sessions_slot_recall(app, payload);
    }
    return COFI_ACTION_ERROR;
}

static const char *const s_sessions_aliases[] = {"tmux", "tx", "zj", "zellij", NULL};
static CofiTabProvider s_sessions_provider;

void sessions_provider_register(void) {
    cofi_init_provider_defaults(&s_sessions_provider);
    s_sessions_provider.tab_mode = TAB_SESSIONS;
    s_sessions_provider.id = "sessions";
    s_sessions_provider.display_name = "SESSIONS";
    s_sessions_provider.get_shortcut_hint = sessions_get_shortcut_hint;
    s_sessions_provider.primary_cmd = "sessions";
    s_sessions_provider.aliases = s_sessions_aliases;
    s_sessions_provider.prefix_char = 0;
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
