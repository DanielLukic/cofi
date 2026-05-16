#include "profiles_provider.h"

#include "app_data.h"
#include "browser_profiles.h"
#include "cofi_tab_provider.h"
#include "log.h"
#include "selection.h"

#include <gtk/gtk.h>

static BrowserProfilesMode s_profiles_mode;

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
    out->cells[0].text = "[c]";
    out->cells[0].width_hint = 4;
    out->cells[1].text = profile->name;
    out->cells[1].width_hint = 24;
    out->cells[2].text = profile->email;
    out->cells[2].width_hint = 40;
    out->cells[3].text = profile->profile_dir;
    out->cells[3].width_hint = 14;
    out->row_flags = COFI_ROW_ACTIONABLE;
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

static CofiActionStatus profiles_on_command_args(AppData *app, const char *args) {
    if (!app || !args || args[0] == '\0') return COFI_NO_OP;
    browser_profiles_load(&s_profiles_mode);
    browser_profiles_filter(&s_profiles_mode, args);
    if (s_profiles_mode.filtered_count <= 0) {
        return COFI_ACTION_ERROR;
    }
    BrowserProfileEntry *profile = profile_at_row(app, 0);
    return browser_profiles_launch(profile) ? COFI_HANDLED_HIDE : COFI_ACTION_ERROR;
}

static const char *const s_profiles_aliases[] = {"chrome", "browser", "browsers", NULL};
static CofiTabProvider s_profiles_provider;

void profiles_provider_register(void) {
    cofi_init_provider_defaults(&s_profiles_provider);
    init_browser_profiles_mode(&s_profiles_mode);
    s_profiles_provider.tab_mode = TAB_PROFILES;
    s_profiles_provider.id = "profiles";
    s_profiles_provider.display_name = "PROFILES";
    s_profiles_provider.primary_cmd = "profiles";
    s_profiles_provider.aliases = s_profiles_aliases;
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
    cofi_register_tab_provider(&s_profiles_provider);
}
