#include "profiles_provider.h"

#include "app_data.h"
#include "browser_profiles.h"
#include "command_mode.h"
#include "command_registry.h"
#include "cofi_tab_provider.h"
#include "log.h"
#include "selection.h"
#include "slot_store.h"
#include "tab_switching.h"
#include "window_lifecycle.h"

#include <gtk/gtk.h>
#include <string.h>

static BrowserProfilesMode s_profiles_mode;
static const char *PROFILE_SLOT_PREFIX = "profile:chrome:";
static CofiTabProvider s_profiles_provider;

static BrowserProfileEntry *profile_at_row(AppData *app, int raw_idx) {
    (void)app;
    if (raw_idx < 0 || raw_idx >= s_profiles_mode.filtered_count) {
        return NULL;
    }
    int profile_idx = s_profiles_mode.filtered_indices[raw_idx];
    if (profile_idx < 0 || profile_idx >= s_profiles_mode.profile_count) {
        return NULL;
    }
    return &s_profiles_mode.profiles[profile_idx];
}

static int profiles_row_count(AppData *app) {
    (void)app;
    return s_profiles_mode.filtered_count > 0
        ? s_profiles_mode.filtered_count
        : 1;
}

static void profiles_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    BrowserProfileEntry *profile = profile_at_row(app, raw_idx);
    if (!profile) {
        out->cell_count = 1;
        out->cells[0].text = s_profiles_mode.last_error[0]
            ? s_profiles_mode.last_error
            : "No matching browser profiles found";
        out->row_flags = 0;
        return;
    }

    out->cell_count = 4;
    out->cells[0].text = "[gc]";
    out->cells[0].width_hint = 5;
    out->cells[1].text = profile->name;
    out->cells[1].width_hint = 24;
    out->cells[2].text = profile->email;
    out->cells[2].width_hint = 40;
    out->cells[3].text = profile->profile_dir;
    out->cells[3].width_hint = 14;
    out->row_flags = COFI_ROW_ACTIONABLE | COFI_ROW_SLOTTABLE;
}

static const char *profiles_match_string(AppData *app, int raw_idx) {
    static char match_text[512];
    BrowserProfileEntry *profile = profile_at_row(app, raw_idx);
    browser_profiles_format_match_text(profile, match_text, sizeof(match_text));
    return match_text;
}

static const char *profiles_row_identity(AppData *app, int raw_idx) {
    static char identity[256];
    BrowserProfileEntry *profile = profile_at_row(app, raw_idx);
    if (!profile) return "";
    g_snprintf(identity, sizeof(identity), "%s:%s",
               profile->browser_id, profile->profile_dir);
    return identity;
}

static void profiles_on_enter(AppData *app) {
    if (!app) return;
    if (app->entry) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter browser profiles...");
    }
    browser_profiles_load(&s_profiles_mode);
}

static void profiles_on_query_changed(AppData *app, const char *query) {
    browser_profiles_filter(&s_profiles_mode, query);
    reset_selection(app);
}

static CofiActionStatus profiles_on_enter_pressed(AppData *app, int filtered_idx,
                                                  int raw_idx,
                                                  const char *entry_text,
                                                  int modifier_state) {
    (void)filtered_idx;
    (void)entry_text;
    (void)modifier_state;
    BrowserProfileEntry *profile = profile_at_row(app, raw_idx);
    if (!profile) return COFI_NO_OP;
    log_info("USER: ENTER pressed -> Launching %s profile '%s'",
             profile->browser_name, profile->name);
    return browser_profiles_launch(profile) ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

static const char *profiles_slot_payload_for(AppData *app, int raw_idx) {
    static char payload[SLOT_STORE_PAYLOAD_LEN];
    BrowserProfileEntry *profile = profile_at_row(app, raw_idx);
    if (!profile || profile->backend != BROWSER_PROFILE_CHROME) return NULL;
    g_snprintf(payload, sizeof(payload), "%s%s",
               PROFILE_SLOT_PREFIX, profile->profile_dir);
    return payload;
}

static gboolean profile_from_slot_payload(const char *payload,
                                          BrowserProfileEntry *profile) {
    if (!payload || !profile || !g_str_has_prefix(payload, PROFILE_SLOT_PREFIX)) {
        return FALSE;
    }
    const char *profile_dir = payload + strlen(PROFILE_SLOT_PREFIX);
    if (profile_dir[0] == '\0') return FALSE;

    memset(profile, 0, sizeof(*profile));
    profile->backend = BROWSER_PROFILE_CHROME;
    g_strlcpy(profile->browser_id, "chrome", sizeof(profile->browser_id));
    g_strlcpy(profile->browser_name, "Chrome", sizeof(profile->browser_name));
    g_strlcpy(profile->executable, "google-chrome", sizeof(profile->executable));
    g_strlcpy(profile->profile_dir, profile_dir, sizeof(profile->profile_dir));
    g_strlcpy(profile->name, profile_dir, sizeof(profile->name));
    return TRUE;
}

static CofiActionStatus profiles_slot_recall(AppData *app, const char *payload) {
    (void)app;
    BrowserProfileEntry profile;
    if (!profile_from_slot_payload(payload, &profile)) {
        log_warn("Profiles slot has invalid payload: %s", payload ? payload : "(null)");
        return COFI_ACTION_ERROR;
    }
    return browser_profiles_launch(&profile) ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

static CofiActionStatus profiles_on_command_args(AppData *app, const char *args) {
    if (!app || !args || args[0] == '\0') return COFI_NO_OP;

    char slot = '\0';
    if (slot_parse_at_key_arg(args, &slot)) {
        const char *payload = slot_lookup(&app->harpoon.store, "profiles", slot);
        if (!payload) return COFI_ACTION_ERROR;
        return profiles_slot_recall(app, payload);
    }

    browser_profiles_load(&s_profiles_mode);
    browser_profiles_filter(&s_profiles_mode, args);
    if (s_profiles_mode.filtered_count <= 0) {
        return COFI_ACTION_ERROR;
    }
    BrowserProfileEntry *profile = profile_at_row(app, 0);
    return browser_profiles_launch(profile) ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

static gboolean profiles_command_handler(AppData *app,
                                         WindowInfo *window __attribute__((unused)),
                                         const char *args) {
    exit_command_mode(app);

    if (args && args[0] != '\0') {
        CofiActionStatus status = profiles_on_command_args(app, args);
        if (status == COFI_HANDLED_HIDE) {
            hide_window(app);
        } else if (status == COFI_ACTION_ERROR || status == COFI_NO_OP) {
            if (app && app->textbuffer) {
                gtk_text_buffer_set_text(app->textbuffer,
                                         "No matching browser profile.",
                                         -1);
                app->command_mode.showing_help = TRUE;
            }
        }
        return FALSE;
    }

    if (app) {
        app->prefix_origin_tab = app->current_tab;
    }
    surface_tab(app, (TabMode)s_profiles_provider.tab_mode);
    return FALSE;
}

static const CommandSpec s_profiles_command = {
    .primary = "profiles",
    .aliases = {"chrome", "browser", "browsers", NULL},
    .owner_provider_id = "profiles",
    .handler = profiles_command_handler,
    .description = "Switch to browser profiles tab",
    .help_format = "profiles, chrome [@SLOT|PROFILE]",
    .keeps_open_on_hotkey_auto = 1
};

void profiles_provider_register(void) {
    cofi_init_provider_defaults(&s_profiles_provider);
    init_browser_profiles_mode(&s_profiles_mode);
    s_profiles_provider.tab_mode = TAB_PROFILES;
    s_profiles_provider.id = "profiles";
    s_profiles_provider.display_name = "PROFILES";
    s_profiles_provider.shortcut_hint = "Actions: Enter=Open  Ctrl+key=Slot  Alt+key=Recall";
    s_profiles_provider.required = 0;
    s_profiles_provider.hidden_by_default = 1;
    s_profiles_provider.modal_policy = COFI_MODAL_HIDE_ON_ESC;
    s_profiles_provider.initial_selection_index = 0;
    s_profiles_provider.row_count = profiles_row_count;
    s_profiles_provider.format_row = profiles_format_row;
    s_profiles_provider.match_string = profiles_match_string;
    s_profiles_provider.row_identity = profiles_row_identity;
    s_profiles_provider.on_enter = profiles_on_enter;
    s_profiles_provider.on_query_changed = profiles_on_query_changed;
    s_profiles_provider.on_enter_pressed = profiles_on_enter_pressed;
    s_profiles_provider.on_command_args = profiles_on_command_args;
    s_profiles_provider.slot_store_enabled = 1;
    s_profiles_provider.slot_payload_for = profiles_slot_payload_for;
    s_profiles_provider.slot_recall = profiles_slot_recall;
    if (cofi_register_tab_provider(&s_profiles_provider) >= 0) {
        cofi_register_command(&s_profiles_command);
    }
}
