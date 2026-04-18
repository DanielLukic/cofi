#include "key_handler_tabs.h"

#include <string.h>

#include "config.h"
#include "display.h"
#include "filter.h"
#include "filter_names.h"
#include "hotkey_config.h"
#include "hotkeys.h"
#include "log.h"
#include "named_window.h"
#include "named_window_config.h"
#include "overlay_manager.h"

gboolean handle_names_tab_keys(GdkEventKey *event, AppData *app) {
    if (app->current_tab != TAB_NAMES) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_e && (event->state & GDK_CONTROL_MASK)) {
        if (app->selection.names_index < app->filtered_names_count) {
            show_name_edit_overlay(app);
            return TRUE;
        }
    }

    if (event->keyval == GDK_KEY_d && (event->state & GDK_CONTROL_MASK)) {
        if (app->selection.names_index < app->filtered_names_count) {
            NamedWindow *named = &app->filtered_names[app->selection.names_index];
            int manager_index = find_named_window_by_name(&app->names, named->custom_name);
            if (manager_index >= 0) {
                delete_custom_name(&app->names, manager_index);
                save_named_windows(&app->names);

                const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
                filter_names(app, current_filter);
                update_display(app);

                log_info("USER: Deleted custom name '%s'", named->custom_name);
            }
        }
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
        if (app->selection.config_index < app->filtered_config_count) {
            ConfigEntry *entry = &app->filtered_config[app->selection.config_index];
            const char *new_value = NULL;
            if (entry->type == CONFIG_TYPE_BOOL) {
                new_value = (strcmp(entry->value, "true") == 0) ? "false" : "true";
            } else if (entry->type == CONFIG_TYPE_ENUM) {
                new_value = get_next_enum_value(entry->key, entry->value);
            }

            if (new_value) {
                char err_buf[128];
                if (apply_config_setting(&app->config, entry->key, new_value, err_buf, sizeof(err_buf))) {
                    save_config(&app->config);
                    const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
                    filter_config(app, current_filter);
                    update_display(app);
                    log_info("USER: Cycled config '%s' to %s", entry->key, new_value);
                } else {
                    log_error("Failed to cycle config '%s': %s", entry->key, err_buf);
                }
                return TRUE;
            }
        }
    }

    if (event->keyval == GDK_KEY_e && (event->state & GDK_CONTROL_MASK)) {
        if (app->selection.config_index < app->filtered_config_count) {
            show_overlay(app, OVERLAY_CONFIG_EDIT, NULL);
            return TRUE;
        }
    }

    return FALSE;
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
        if (app->selection.hotkeys_index < app->filtered_hotkeys_count) {
            HotkeyBinding *binding = &app->filtered_hotkeys[app->selection.hotkeys_index];
            remove_hotkey_binding(&app->hotkey_config, binding->key);
            save_hotkey_config(&app->hotkey_config);
            regrab_hotkeys(app);

            const char *current_filter = gtk_entry_get_text(GTK_ENTRY(app->entry));
            filter_hotkeys(app, current_filter);

            if (app->selection.hotkeys_index >= app->filtered_hotkeys_count && app->filtered_hotkeys_count > 0) {
                app->selection.hotkeys_index = app->filtered_hotkeys_count - 1;
            }

            update_display(app);
            log_info("USER: Deleted hotkey binding '%s'", binding->key);
        }
        return TRUE;
    }

    if (event->keyval == GDK_KEY_e && (event->state & GDK_CONTROL_MASK)) {
        if (app->selection.hotkeys_index < app->filtered_hotkeys_count) {
            show_overlay(app, OVERLAY_HOTKEY_EDIT, NULL);
            return TRUE;
        }
    }

    return FALSE;
}
