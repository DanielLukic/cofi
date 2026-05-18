#include "projects_provider.h"

#include "app_data.h"
#include "command_mode.h"
#include "command_registry.h"
#include "cofi_tab_provider.h"
#include "overlay_manager.h"
#include "projects.h"
#include "projects_parse.h"
#include "slot_store.h"
#include "tab_switching.h"
#include "window_lifecycle.h"

static CofiTabProvider s_projects_provider;
static int s_projects_provider_id = -1;
#define PROJECTS_PROVIDER_ID "projects"
#define LEGACY_PROJECTS_SLOT_PROVIDER_ID "sessions"

static TabMode projects_tab_mode(void) {
    const CofiTabProvider *provider = cofi_get_provider(s_projects_provider_id);
    return provider ? (TabMode)provider->tab_mode : TAB_WINDOWS;
}

static void show_new_session_for_selection(AppData *app, gboolean prefer_zellij) {
    ProjectBackend backend = prefer_zellij ? PROJECT_BACKEND_ZELLIJ : PROJECT_BACKEND_TMUX;
    ProjectSessionEntry *session = projects_selected_session(app);
    if (!prefer_zellij && session && session->backend == PROJECT_BACKEND_ZELLIJ) {
        backend = PROJECT_BACKEND_ZELLIJ;
    }

    ProjectFolder *folder = projects_selected_folder(app);
    if (!folder) {
        show_project_new_overlay(app, backend, "", "");
        return;
    }

    gchar *session_name = projects_build_folder_session_name(folder->path);
    show_project_new_overlay(app, backend, folder->path, session_name);
    g_free(session_name);
}

gboolean handle_projects_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != projects_tab_mode()) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_Insert || event->keyval == GDK_KEY_KP_Insert) {
        show_new_session_for_selection(app, (event->state & GDK_SHIFT_MASK) != 0);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete) {
        ProjectSessionEntry *session = projects_selected_session(app);
        if (!session) {
            return FALSE;
        }
        show_project_kill_overlay(app, session->name, session->backend);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_F2) {
        ProjectSessionEntry *session = projects_selected_session(app);
        if (!session || session->backend != PROJECT_BACKEND_TMUX) {
            return FALSE;
        }
        show_project_rename_overlay(app, session->name);
        return TRUE;
    }

    return FALSE;
}

static CofiActionStatus projects_provider_on_enter_pressed(AppData *app, int filtered_idx,
                                                       int raw_idx,
                                                       const char *entry_text,
                                                       int modifier_state) {
    (void)filtered_idx;
    (void)entry_text;
    (void)modifier_state;
    /* raw_idx is visible here: Projects exposes its filtered list directly to the provider renderer. */
    ProjectFolder *folder = projects_folder_at_visible(app, raw_idx);
    if (folder) {
        return projects_open_folder(app, folder->path);
    }
    return projects_attach_visible(app, raw_idx);
}

static CofiActionStatus projects_provider_on_command_args(AppData *app, const char *args) {
    if (!app) return COFI_NO_OP;
    if (!args || args[0] == '\0') return COFI_NO_OP;
    projects_refresh(app);

    if (projects_has_named(app, args)) {
        return projects_attach_named(app, args);
    }

    char slot = '\0';
    if (slot_parse_at_key_arg(args, &slot)) {
        const char *payload = slot_lookup(&app->harpoon.store, PROJECTS_PROVIDER_ID, slot);
        if (!payload) {
            payload = slot_lookup(&app->harpoon.store, LEGACY_PROJECTS_SLOT_PROVIDER_ID, slot);
        }
        if (!payload) return COFI_ACTION_ERROR;
        return projects_slot_recall(app, payload);
    }
    return COFI_ACTION_ERROR;
}

static void projects_show_command_error(AppData *app, const char *message) {
    if (!app || !app->textbuffer) return;
    gtk_text_buffer_set_text(app->textbuffer, message, -1);
    app->command_mode.showing_help = TRUE;
}

static gboolean projects_command_handler(AppData *app,
                                         WindowInfo *window __attribute__((unused)),
                                         const char *args) {
    exit_command_mode(app);

    if (args && args[0] != '\0') {
        CofiActionStatus status = projects_provider_on_command_args(app, args);
        if (status == COFI_HANDLED_HIDE) {
            hide_window(app);
        } else if (status == COFI_ACTION_ERROR || status == COFI_NO_OP) {
            projects_show_command_error(app, "No matching tmux/zellij session.");
        }
        return FALSE;
    }

    if (app) {
        app->prefix_origin_tab = app->current_tab;
    }
    surface_tab(app, projects_tab_mode());
    return FALSE;
}

static const CommandSpec s_projects_command = {
    .primary = "projects",
    .aliases = {"project", "tmux", "tx", "zj", "zellij"},
    .owner_provider_id = PROJECTS_PROVIDER_ID,
    .handler = projects_command_handler,
    .description = "Switch to projects tab",
    .help_format = "projects, project, tmux, tx, zj, zellij [@SLOT|SESSION]",
    .keeps_open_on_hotkey_auto = 1
};

void projects_provider_register(void) {
    cofi_init_provider_defaults(&s_projects_provider);
    s_projects_provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    s_projects_provider.id = PROJECTS_PROVIDER_ID;
    s_projects_provider.display_name = "PROJECTS";
    s_projects_provider.get_shortcut_hint = projects_get_shortcut_hint;
    s_projects_provider.prefix_char = 0;
    s_projects_provider.required = 0;
    s_projects_provider.hidden_by_default = 1;
    s_projects_provider.modal_policy = COFI_MODAL_HIDE_ON_ESC;
    s_projects_provider.initial_selection_index = 0;
    s_projects_provider.row_count = projects_row_count;
    s_projects_provider.format_row = projects_format_row;
    s_projects_provider.match_string = projects_match_string;
    s_projects_provider.row_identity = projects_row_identity;
    s_projects_provider.on_enter = projects_on_enter;
    s_projects_provider.on_query_changed = projects_on_query_changed;
    s_projects_provider.on_tick = projects_on_tick;
    s_projects_provider.tick_interval_ms = 1500;
    s_projects_provider.on_enter_pressed = projects_provider_on_enter_pressed;
    s_projects_provider.on_command_args = projects_provider_on_command_args;
    s_projects_provider.handle_key = handle_projects_tab_keys;
    s_projects_provider.slot_store_enabled = 1;
    s_projects_provider.slot_payload_for = projects_slot_payload_for;
    s_projects_provider.slot_recall = projects_slot_recall;
    s_projects_provider_id = cofi_register_tab_provider(&s_projects_provider);
    if (s_projects_provider_id >= 0) {
        cofi_register_command(&s_projects_command);
    }
}
