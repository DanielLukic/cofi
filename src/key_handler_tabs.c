#include "key_handler_tabs.h"

#include <string.h>

#include "config.h"
#include "config_provider.h"
#include "display.h"
#include "filter_names.h"
#include "hotkey_config.h"
#include "hotkeys_provider.h"
#include "hotkeys.h"
#include "log.h"
#include "named_window.h"
#include "named_window_config.h"
#include "overlay_manager.h"
#include "rules_replay.h"
#include "sessions.h"
#include "sessions_parse.h"

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

static gboolean clamp_names_selection(AppData *app) {
    if (app->filtered_names_count <= 0) {
        return FALSE;
    }

    if (app->selection.names_index < 0) {
        app->selection.names_index = 0;
    }
    if (app->selection.names_index >= app->filtered_names_count) {
        app->selection.names_index = app->filtered_names_count - 1;
    }
    return TRUE;
}

gboolean handle_names_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != TAB_NAMES) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_e && (event->state & GDK_CONTROL_MASK)) {
        if (!clamp_names_selection(app)) {
            return FALSE;
        }
        show_name_edit_overlay(app);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_d && (event->state & GDK_CONTROL_MASK)) {
        if (!clamp_names_selection(app)) {
            log_debug("Names Ctrl+D ignored: no rows to delete");
            return FALSE;
        }

        NamedWindow *named = &app->filtered_names[app->selection.names_index];
        int manager_index = -1;
        if (named->id != 0) {
            manager_index = find_named_window_index(&app->names, named->id);
        }
        if (manager_index < 0) {
            manager_index = find_named_window_by_name(&app->names, named->custom_name);
        }

        log_info("Names Ctrl+D: showing delete confirm for '%s' (mgr_idx=%d, sel=%d/%d)",
                 named->custom_name, manager_index,
                 app->selection.names_index, app->filtered_names_count);
        show_name_delete_overlay(app, named->custom_name, manager_index);
        return TRUE;
    }

    return FALSE;
}

gboolean handle_harpoon_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != TAB_HARPOON) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_d && (event->state & GDK_CONTROL_MASK)) {
        if (app->selection.harpoon_index < app->filtered_harpoon_count) {
            HarpoonSlot *slot = &app->filtered_harpoon[app->selection.harpoon_index];
            if (slot->assigned) {
                int actual_slot = app->filtered_harpoon_indices[app->selection.harpoon_index];
                show_harpoon_delete_overlay(app, actual_slot);
                return TRUE;
            }
        }
    }

    if (event->keyval == GDK_KEY_e && (event->state & GDK_CONTROL_MASK)) {
        if (app->selection.harpoon_index < app->filtered_harpoon_count) {
            HarpoonSlot *slot = &app->filtered_harpoon[app->selection.harpoon_index];
            if (slot->assigned) {
                int actual_slot = app->filtered_harpoon_indices[app->selection.harpoon_index];
                show_harpoon_edit_overlay(app, actual_slot);
                return TRUE;
            }
        }
    }

    return FALSE;
}

gboolean handle_config_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != TAB_CONFIG) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_t && (event->state & GDK_CONTROL_MASK)) {
        ConfigEntry *entry = config_selected_entry(app);
        if (entry) {
            const char *new_value = NULL;
            if (entry->type == CONFIG_TYPE_BOOL) {
                new_value = (strcmp(entry->value, "true") == 0) ? "false" : "true";
            } else if (entry->type == CONFIG_TYPE_ENUM) {
                new_value = get_next_enum_value(entry->key, entry->value);
            }

            if (new_value) {
                char selected_key[sizeof(entry->key)];
                g_strlcpy(selected_key, entry->key, sizeof(selected_key));
                char err_buf[128];
                if (apply_config_setting(&app->config, selected_key, new_value, err_buf, sizeof(err_buf))) {
                    save_config(&app->config);
                    const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
                    filter_config(app, current_filter);
                    config_select_key(app, selected_key);
                    update_display(app);
                    log_info("USER: Cycled config '%s' to %s", selected_key, new_value);
                } else {
                    log_error("Failed to cycle config '%s': %s", selected_key, err_buf);
                }
                return TRUE;
            }
        }
    }

    if (event->keyval == GDK_KEY_e && (event->state & GDK_CONTROL_MASK)) {
        if (config_selected_entry(app)) {
            show_overlay(app, OVERLAY_CONFIG_EDIT, NULL);
            return TRUE;
        }
    }

    return FALSE;
}

static gboolean clamp_rules_selection(AppData *app) {
    if (app->filtered_rules_count <= 0) {
        return FALSE;
    }

    if (app->selection.rules_index < 0) {
        app->selection.rules_index = 0;
    }
    if (app->selection.rules_index >= app->filtered_rules_count) {
        app->selection.rules_index = app->filtered_rules_count - 1;
    }
    return TRUE;
}

gboolean handle_hotkeys_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != TAB_HOTKEYS) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_a && (event->state & GDK_CONTROL_MASK)) {
        cleanup_hotkeys(app);
        app->hotkey_capture_active = TRUE;
        show_overlay(app, OVERLAY_HOTKEY_ADD, NULL);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_d && (event->state & GDK_CONTROL_MASK)) {
        HotkeyBinding *binding = hotkeys_selected_binding(app, NULL);
        if (binding) {
            char deleted_key[sizeof(binding->key)];
            g_strlcpy(deleted_key, binding->key, sizeof(deleted_key));
            remove_hotkey_binding(&app->hotkey_config, deleted_key);
            save_hotkey_config(&app->hotkey_config);
            regrab_hotkeys(app);

            const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
            filter_hotkeys(app, current_filter);
            if (app->selection.provider_index >= app->filtered_hotkeys_count &&
                app->filtered_hotkeys_count > 0) {
                app->selection.provider_index = app->filtered_hotkeys_count - 1;
            } else if (app->filtered_hotkeys_count <= 0) {
                app->selection.provider_index = 0;
            }

            update_display(app);
            log_info("USER: Deleted hotkey binding '%s'", deleted_key);
        }
        return TRUE;
    }

    if (event->keyval == GDK_KEY_e && (event->state & GDK_CONTROL_MASK)) {
        if (hotkeys_selected_binding(app, NULL)) {
            show_overlay(app, OVERLAY_HOTKEY_EDIT, NULL);
            return TRUE;
        }
    }

    if (event->keyval == GDK_KEY_b && (event->state & GDK_CONTROL_MASK)) {
        int master_idx = -1;
        HotkeyBinding *binding = hotkeys_selected_binding(app, &master_idx);
        if (!binding || master_idx < 0) {
            return FALSE;
        }
        app->hotkey_rebind.active = TRUE;
        app->hotkey_rebind.target_index = master_idx;
        g_strlcpy(app->hotkey_rebind.target_key, binding->key,
                  sizeof(app->hotkey_rebind.target_key));
        g_strlcpy(app->hotkey_rebind.target_command, binding->command,
                  sizeof(app->hotkey_rebind.target_command));
        app->hotkey_rebind.awaiting_confirm = FALSE;
        app->hotkey_rebind.conflict_index = -1;
        cleanup_hotkeys(app);
        app->hotkey_capture_active = TRUE;
        show_overlay(app, OVERLAY_HOTKEY_REBIND, NULL);
        return TRUE;
    }

    return FALSE;
}

gboolean handle_rules_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != TAB_RULES) {
        return FALSE;
    }

    if ((event->state & GDK_CONTROL_MASK) &&
        (event->keyval == GDK_KEY_a || event->keyval == GDK_KEY_A)) {
        show_overlay(app, OVERLAY_RULE_ADD, NULL);
        return TRUE;
    }

    if ((event->state & GDK_CONTROL_MASK) &&
        (event->keyval == GDK_KEY_e || event->keyval == GDK_KEY_E)) {
        if (!clamp_rules_selection(app)) {
            return FALSE;
        }
        show_overlay(app, OVERLAY_RULE_EDIT, NULL);
        return TRUE;
    }

    if ((event->state & GDK_CONTROL_MASK) &&
        (event->keyval == GDK_KEY_d || event->keyval == GDK_KEY_D)) {
        if (!clamp_rules_selection(app)) {
            return FALSE;
        }
        app->rules_delete.pending_delete = TRUE;
        app->rules_delete.rule_index = app->filtered_rule_indices[app->selection.rules_index];
        show_overlay(app, OVERLAY_RULE_DELETE, NULL);
        return TRUE;
    }

    if ((event->state & GDK_CONTROL_MASK) &&
        (event->state & GDK_SHIFT_MASK) &&
        (event->keyval == GDK_KEY_x || event->keyval == GDK_KEY_X)) {
        replay_all_rules_against_open_windows(app);
        return TRUE;
    }

    if ((event->state & GDK_CONTROL_MASK) &&
        (event->keyval == GDK_KEY_x || event->keyval == GDK_KEY_X)) {
        if (!clamp_rules_selection(app)) {
            return FALSE;
        }
        replay_selected_filtered_rule(app);
        return TRUE;
    }

    return FALSE;
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
