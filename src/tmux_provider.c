#include "tmux_provider.h"

#include "app_data.h"
#include "cofi_tab_provider.h"
#include "tmux.h"

#include <gtk/gtk.h>

static CofiActionStatus tmux_provider_on_enter_pressed(AppData *app, int filtered_idx,
                                                       int raw_idx,
                                                       const char *entry_text,
                                                       int modifier_state) {
    (void)filtered_idx;
    (void)entry_text;
    (void)modifier_state;
    /* raw_idx is visible here: tmux exposes its filtered list directly to the provider renderer. */
    return tmux_attach_visible(app, raw_idx);
}

static CofiActionStatus tmux_provider_on_command_args(AppData *app, const char *args) {
    if (!app) return COFI_NO_OP;
    if (!args || args[0] == '\0') return COFI_NO_OP;
    tmux_refresh(app);
    return tmux_attach_named(app, args);
}

static const char *const s_tmux_aliases[] = {"tx", "zj", "zellij", "sessions", NULL};
static CofiTabProvider s_tmux_provider;

void tmux_provider_register(void) {
    cofi_init_provider_defaults(&s_tmux_provider);
    s_tmux_provider.tab_mode = TAB_TMUX;
    s_tmux_provider.id = "tmux";
    s_tmux_provider.display_name = "SESSIONS";
    s_tmux_provider.shortcut_hint = "Shortcuts: Delete=Kill session  F2=Rename session (tmux only)  Insert=New tmux session";
    s_tmux_provider.primary_cmd = "tmux";
    s_tmux_provider.aliases = s_tmux_aliases;
    s_tmux_provider.prefix_char = 0;
    s_tmux_provider.hidden_by_default = 1;
    s_tmux_provider.modal_policy = COFI_MODAL_HIDE_ON_ESC;
    s_tmux_provider.initial_selection_index = 0;
    s_tmux_provider.row_count = tmux_row_count;
    s_tmux_provider.format_row = tmux_format_row;
    s_tmux_provider.match_string = tmux_match_string;
    s_tmux_provider.row_identity = tmux_row_identity;
    s_tmux_provider.on_enter = tmux_on_enter;
    s_tmux_provider.on_query_changed = tmux_on_query_changed;
    s_tmux_provider.on_tick = tmux_on_tick;
    s_tmux_provider.tick_interval_ms = 1500;
    s_tmux_provider.on_enter_pressed = tmux_provider_on_enter_pressed;
    s_tmux_provider.on_command_args = tmux_provider_on_command_args;
    cofi_register_tab_provider(&s_tmux_provider);
}
