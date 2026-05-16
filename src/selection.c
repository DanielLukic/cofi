#include "selection.h"
#include "cofi_tab_provider.h"
#include "log.h"
#include "display.h"
#include "tab_metadata.h"

// Initialize selection state
void init_selection(AppData *app) {
    if (!app) return;

    app->selection.window_index = 0;
    app->selection.workspace_index = 0;
    app->selection.harpoon_index = 0;
    app->selection.names_index = 0;
    app->selection.config_index = 0;
    app->selection.rules_index = 0;
    app->selection.selected_window_id = 0;
    app->selection.selected_workspace_id = -1;
    app->selection.provider_index = 0;
    app->selection.sinks_index = 0;

    // Initialize scroll offsets
    app->selection.window_scroll_offset = 0;
    app->selection.workspace_scroll_offset = 0;
    app->selection.harpoon_scroll_offset = 0;
    app->selection.names_scroll_offset = 0;
    app->selection.config_scroll_offset = 0;
    app->selection.rules_scroll_offset = 0;
    app->selection.provider_scroll_offset = 0;
    app->selection.sinks_scroll_offset = 0;

    log_debug("Selection initialized");
}

// Reset selection to the tab's default row.
void reset_selection(AppData *app) {
    if (!app) return;

    if (app->current_tab == TAB_WINDOWS) {
        app->selection.window_index = 0;
        app->selection.selected_window_id = 0;
        app->selection.window_scroll_offset = 0;
        if (app->filtered_count > 0) {
            app->selection.selected_window_id = app->filtered[0].id;
        }
    } else if (app->current_tab == TAB_WORKSPACES) {
        app->selection.workspace_index = 0;
        app->selection.selected_workspace_id = -1;
        app->selection.workspace_scroll_offset = 0;
        if (app->filtered_workspace_count > 0) {
            app->selection.selected_workspace_id = app->filtered_workspaces[0].id;
        }
    } else if (app->current_tab == TAB_HARPOON) {
        app->selection.harpoon_index = 0;
        app->selection.harpoon_scroll_offset = 0;
    } else if (app->current_tab == TAB_NAMES) {
        app->selection.names_index = 0;
        app->selection.names_scroll_offset = 0;
    } else if (app->current_tab == TAB_CONFIG) {
        app->selection.config_index = 0;
        app->selection.config_scroll_offset = 0;
    } else if (app->current_tab == TAB_RULES) {
        app->selection.rules_index = 0;
        app->selection.rules_scroll_offset = 0;
    } else {
        const CofiTabProvider *provider = cofi_get_provider_for_tab(app->current_tab);
        if (provider) {
            int count = provider->row_count ? provider->row_count(app) : 0;
            app->selection.provider_index = provider->initial_selection_index;
            if (count > 0 && app->selection.provider_index >= count) {
                app->selection.provider_index = count - 1;
            } else if (app->selection.provider_index < 0 || count <= 0) {
                app->selection.provider_index = 0;
            }
            app->selection.provider_scroll_offset = 0;
        }
    }

    log_debug("Selection reset for %s tab", tab_log_name(app->current_tab));
}

// Get currently selected window
WindowInfo* get_selected_window(AppData *app) {
    if (!app || app->current_tab != TAB_WINDOWS) return NULL;
    if (app->filtered_count == 0) return NULL;
    if (app->selection.window_index < 0 || app->selection.window_index >= app->filtered_count) {
        return NULL;
    }
    
    return &app->filtered[app->selection.window_index];
}

// Get currently selected workspace
WorkspaceInfo* get_selected_workspace(AppData *app) {
    if (!app || app->current_tab != TAB_WORKSPACES) return NULL;
    if (app->filtered_workspace_count == 0) return NULL;
    if (app->selection.workspace_index < 0 || app->selection.workspace_index >= app->filtered_workspace_count) {
        return NULL;
    }
    
    return &app->filtered_workspaces[app->selection.workspace_index];
}

// Get the appropriate selected index for current tab
int get_selected_index(AppData *app) {
    if (!app) return 0;
    
    if (app->current_tab == TAB_WINDOWS) {
        return app->selection.window_index;
    } else if (app->current_tab == TAB_WORKSPACES) {
        return app->selection.workspace_index;
    } else if (app->current_tab == TAB_HARPOON) {
        return app->selection.harpoon_index;
    } else if (app->current_tab == TAB_NAMES) {
        return app->selection.names_index;
    } else if (app->current_tab == TAB_CONFIG) {
        return app->selection.config_index;
    } else if (app->current_tab == TAB_RULES) {
        return app->selection.rules_index;
    } else if (cofi_get_provider_for_tab(app->current_tab)) {
        return app->selection.provider_index;
    }

    return 0;
}

// Move selection up (decrements index in display, moves up visually)
void move_selection_up(AppData *app) {
    if (!app) return;

    if (app->current_tab == TAB_WINDOWS) {
        if (app->filtered_count > 0) {
            if (app->selection.window_index < app->filtered_count - 1) {
                app->selection.window_index++;
            } else {
                // Wrap around to the bottom (index 0 is the best match)
                app->selection.window_index = 0;
            }
            app->selection.selected_window_id = app->filtered[app->selection.window_index].id;
            update_scroll_position(app);
            update_display(app);
            log_info("USER: Selection UP -> Window[%d] '%s' (ID: 0x%lx)",
                     app->selection.window_index,
                     app->filtered[app->selection.window_index].title,
                     app->filtered[app->selection.window_index].id);
        }
    } else if (app->current_tab == TAB_WORKSPACES) {
        if (app->filtered_workspace_count > 0) {
            if (app->selection.workspace_index < app->filtered_workspace_count - 1) {
                app->selection.workspace_index++;
            } else {
                // Wrap around to the bottom
                app->selection.workspace_index = 0;
            }
            app->selection.selected_workspace_id = app->filtered_workspaces[app->selection.workspace_index].id;
            update_scroll_position(app);
            update_display(app);
            log_info("USER: Selection UP -> Workspace[%d] '%s' (ID: %d)",
                     app->selection.workspace_index,
                     app->filtered_workspaces[app->selection.workspace_index].name,
                     app->filtered_workspaces[app->selection.workspace_index].id);
        }
    } else if (app->current_tab == TAB_HARPOON) {
        if (app->filtered_harpoon_count > 0) {
            if (app->selection.harpoon_index < app->filtered_harpoon_count - 1) {
                app->selection.harpoon_index++;
            } else {
                // Wrap around to the bottom
                app->selection.harpoon_index = 0;
            }
            update_scroll_position(app);
            update_display(app);
            log_info("USER: Selection UP -> Harpoon slot %d", app->selection.harpoon_index);
        }
    } else if (app->current_tab == TAB_NAMES) {
        if (app->filtered_names_count > 0) {
            if (app->selection.names_index < app->filtered_names_count - 1) {
                app->selection.names_index++;
            } else {
                app->selection.names_index = 0;
            }
            update_scroll_position(app);
            update_display(app);
            log_info("USER: Selection UP -> Named window[%d] '%s'",
                     app->selection.names_index,
                     app->filtered_names[app->selection.names_index].custom_name);
        }
    } else if (app->current_tab == TAB_CONFIG) {
        if (app->filtered_config_count > 0) {
            if (app->selection.config_index < app->filtered_config_count - 1) {
                app->selection.config_index++;
            } else {
                app->selection.config_index = 0;
            }
            update_scroll_position(app);
            update_display(app);
            log_info("USER: Selection UP -> Config[%d] '%s'",
                     app->selection.config_index,
                     app->filtered_config[app->selection.config_index].key);
        }
    } else if (app->current_tab == TAB_RULES) {
        if (app->filtered_rules_count > 0) {
            if (app->selection.rules_index < app->filtered_rules_count - 1) {
                app->selection.rules_index++;
            } else {
                app->selection.rules_index = 0;
            }
            update_scroll_position(app);
            update_display(app);
            log_info("USER: Selection UP -> Rule[%d] '%s'",
                     app->selection.rules_index,
                     app->filtered_rules[app->selection.rules_index].pattern);
        }
    } else {
        const CofiTabProvider *p = cofi_get_provider_for_tab(app->current_tab);
        if (p) {
            int count = p->row_count ? p->row_count(app) : 0;
            if (count > 0) {
                if (app->selection.provider_index < count - 1) {
                    app->selection.provider_index++;
                } else {
                    app->selection.provider_index = 0;
                }
                update_scroll_position(app);
                update_display(app);
                if (p->on_selection_changed)
                    p->on_selection_changed(app, app->selection.provider_index);
                log_info("USER: Selection UP -> provider[%d]", app->selection.provider_index);
            }
            return;
        }
    }
}

// Move selection down (increments index in display, moves down visually)
void move_selection_down(AppData *app) {
    if (!app) return;

    if (app->current_tab == TAB_WINDOWS) {
        if (app->filtered_count > 0) {
            if (app->selection.window_index > 0) {
                app->selection.window_index--;
            } else {
                // Wrap around to the top (highest index)
                app->selection.window_index = app->filtered_count - 1;
            }
            app->selection.selected_window_id = app->filtered[app->selection.window_index].id;
            update_scroll_position(app);
            update_display(app);
            log_info("USER: Selection DOWN -> Window[%d] '%s' (ID: 0x%lx)",
                     app->selection.window_index,
                     app->filtered[app->selection.window_index].title,
                     app->filtered[app->selection.window_index].id);
        }
    } else if (app->current_tab == TAB_WORKSPACES) {
        if (app->filtered_workspace_count > 0) {
            if (app->selection.workspace_index > 0) {
                app->selection.workspace_index--;
            } else {
                // Wrap around to the top (highest index)
                app->selection.workspace_index = app->filtered_workspace_count - 1;
            }
            app->selection.selected_workspace_id = app->filtered_workspaces[app->selection.workspace_index].id;
            update_scroll_position(app);
            update_display(app);
            log_info("USER: Selection DOWN -> Workspace[%d] '%s' (ID: %d)",
                     app->selection.workspace_index,
                     app->filtered_workspaces[app->selection.workspace_index].name,
                     app->filtered_workspaces[app->selection.workspace_index].id);
        }
    } else if (app->current_tab == TAB_HARPOON) {
        if (app->filtered_harpoon_count > 0) {
            if (app->selection.harpoon_index > 0) {
                app->selection.harpoon_index--;
            } else {
                // Wrap around to the top (highest index)
                app->selection.harpoon_index = app->filtered_harpoon_count - 1;
            }
            update_scroll_position(app);
            update_display(app);
            log_info("USER: Selection DOWN -> Harpoon slot %d", app->selection.harpoon_index);
        }
    } else if (app->current_tab == TAB_NAMES) {
        if (app->filtered_names_count > 0) {
            if (app->selection.names_index > 0) {
                app->selection.names_index--;
            } else {
                app->selection.names_index = app->filtered_names_count - 1;
            }
            update_scroll_position(app);
            update_display(app);
            log_info("USER: Selection DOWN -> Named window[%d] '%s'",
                     app->selection.names_index,
                     app->filtered_names[app->selection.names_index].custom_name);
        }
    } else if (app->current_tab == TAB_CONFIG) {
        if (app->filtered_config_count > 0) {
            if (app->selection.config_index > 0) {
                app->selection.config_index--;
            } else {
                app->selection.config_index = app->filtered_config_count - 1;
            }
            update_scroll_position(app);
            update_display(app);
            log_info("USER: Selection DOWN -> Config[%d] '%s'",
                     app->selection.config_index,
                     app->filtered_config[app->selection.config_index].key);
        }
    } else if (app->current_tab == TAB_RULES) {
        if (app->filtered_rules_count > 0) {
            if (app->selection.rules_index > 0) {
                app->selection.rules_index--;
            } else {
                app->selection.rules_index = app->filtered_rules_count - 1;
            }
            update_scroll_position(app);
            update_display(app);
            log_info("USER: Selection DOWN -> Rule[%d] '%s'",
                     app->selection.rules_index,
                     app->filtered_rules[app->selection.rules_index].pattern);
        }
    } else {
        const CofiTabProvider *p = cofi_get_provider_for_tab(app->current_tab);
        if (p) {
            int count = p->row_count ? p->row_count(app) : 0;
            if (count > 0) {
                if (app->selection.provider_index > 0) {
                    app->selection.provider_index--;
                } else {
                    app->selection.provider_index = count - 1;
                }
                update_scroll_position(app);
                update_display(app);
                if (p->on_selection_changed)
                    p->on_selection_changed(app, app->selection.provider_index);
                log_info("USER: Selection DOWN -> provider[%d]", app->selection.provider_index);
            }
            return;
        }
    }
}

// Preserve current selection before filtering
void preserve_selection(AppData *app) {
    if (!app) return;
    
    if (app->current_tab == TAB_WINDOWS) {
        if (app->filtered_count > 0 && app->selection.window_index >= 0 && 
            app->selection.window_index < app->filtered_count) {
            app->selection.selected_window_id = app->filtered[app->selection.window_index].id;
            log_trace("Preserved window selection: ID 0x%lx at index %d",
                      app->selection.selected_window_id, app->selection.window_index);
        }
    } else {
        if (app->filtered_workspace_count > 0 && app->selection.workspace_index >= 0 && 
            app->selection.workspace_index < app->filtered_workspace_count) {
            app->selection.selected_workspace_id = app->filtered_workspaces[app->selection.workspace_index].id;
            log_debug("Preserved workspace selection: ID %d at index %d", 
                      app->selection.selected_workspace_id, app->selection.workspace_index);
        }
    }
}

// Restore selection after filtering
void restore_selection(AppData *app) {
    if (!app) return;
    
    if (app->current_tab == TAB_WINDOWS) {
        if (app->selection.selected_window_id != 0) {
            // Try to find the previously selected window
            bool found = false;
            for (int i = 0; i < app->filtered_count; i++) {
                if (app->filtered[i].id == app->selection.selected_window_id) {
                    app->selection.window_index = i;
                    log_trace("Restored window selection to index %d for window ID 0x%lx",
                              i, app->selection.selected_window_id);
                    found = true;
                    break;
                }
            }
            if (!found) {
                // Window no longer exists, reset to first
                app->selection.window_index = 0;
                app->selection.selected_window_id = (app->filtered_count > 0) ? app->filtered[0].id : 0;
                log_debug("Previously selected window ID 0x%lx no longer exists, reset to 0", 
                          app->selection.selected_window_id);
            }
        } else {
            // No previous selection, select first window
            app->selection.window_index = 0;
            app->selection.selected_window_id = (app->filtered_count > 0) ? app->filtered[0].id : 0;
            log_debug("No previous window selection, defaulting to index 0");
        }
    } else {
        if (app->selection.selected_workspace_id != -1) {
            // Try to find the previously selected workspace
            bool found = false;
            for (int i = 0; i < app->filtered_workspace_count; i++) {
                if (app->filtered_workspaces[i].id == app->selection.selected_workspace_id) {
                    app->selection.workspace_index = i;
                    log_debug("Restored workspace selection to index %d for workspace ID %d", 
                              i, app->selection.selected_workspace_id);
                    found = true;
                    break;
                }
            }
            if (!found) {
                // Workspace no longer exists, reset to first
                app->selection.workspace_index = 0;
                app->selection.selected_workspace_id = (app->filtered_workspace_count > 0) ? 
                    app->filtered_workspaces[0].id : -1;
                log_debug("Previously selected workspace ID %d no longer exists, reset to 0", 
                          app->selection.selected_workspace_id);
            }
        } else {
            // No previous selection, select first workspace
            app->selection.workspace_index = 0;
            app->selection.selected_workspace_id = (app->filtered_workspace_count > 0) ? 
                app->filtered_workspaces[0].id : -1;
            log_debug("No previous workspace selection, defaulting to index 0");
        }
    }

    // Update scroll position to keep selected item visible
    update_scroll_position(app);
}

// Get current scroll offset for active tab
int get_scroll_offset(AppData *app) {
    if (!app) return 0;

    switch (app->current_tab) {
        case TAB_WINDOWS:
            return app->selection.window_scroll_offset;
        case TAB_WORKSPACES:
            return app->selection.workspace_scroll_offset;
        case TAB_HARPOON:
            return app->selection.harpoon_scroll_offset;
        case TAB_NAMES:
            return app->selection.names_scroll_offset;
        case TAB_CONFIG:
            return app->selection.config_scroll_offset;
        case TAB_RULES:
            return app->selection.rules_scroll_offset;
        default:
            if (cofi_get_provider_for_tab(app->current_tab))
                return app->selection.provider_scroll_offset;
            return 0;
    }
}

// Set scroll offset for active tab
void set_scroll_offset(AppData *app, int offset) {
    if (!app) return;

    switch (app->current_tab) {
        case TAB_WINDOWS:
            app->selection.window_scroll_offset = offset;
            break;
        case TAB_WORKSPACES:
            app->selection.workspace_scroll_offset = offset;
            break;
        case TAB_HARPOON:
            app->selection.harpoon_scroll_offset = offset;
            break;
        case TAB_NAMES:
            app->selection.names_scroll_offset = offset;
            break;
        case TAB_CONFIG:
            app->selection.config_scroll_offset = offset;
            break;
        case TAB_RULES:
            app->selection.rules_scroll_offset = offset;
            break;
        default:
            if (cofi_get_provider_for_tab(app->current_tab))
                app->selection.provider_scroll_offset = offset;
            break;
    }
}

// Update scroll position to keep selected item visible
void update_scroll_position(AppData *app) {
    if (!app) return;

    int selected_idx = get_selected_index(app);
    int max_lines = get_max_display_lines_dynamic(app);
    int total_count = 0;

    // Get total count for current tab
    switch (app->current_tab) {
        case TAB_WINDOWS:
            total_count = app->filtered_count;
            break;
        case TAB_WORKSPACES:
            total_count = app->filtered_workspace_count;
            break;
        case TAB_HARPOON:
            total_count = app->filtered_harpoon_count;
            break;
        case TAB_NAMES:
            total_count = app->filtered_names_count;
            break;
        case TAB_CONFIG:
            total_count = app->filtered_config_count;
            break;
        case TAB_RULES:
            total_count = app->filtered_rules_count;
            break;
        default: {
            const CofiTabProvider *p = cofi_get_provider_for_tab(app->current_tab);
            if (p && p->row_count)
                total_count = p->row_count(app);
            break;
        }
    }

    if (total_count <= max_lines) {
        // All items fit on screen, no scrolling needed
        set_scroll_offset(app, 0);
        return;
    }

    int current_offset = get_scroll_offset(app);
    int new_offset = current_offset;

    // Check if selected item is above visible area
    if (selected_idx < current_offset) {
        new_offset = selected_idx;
    }
    // Check if selected item is below visible area
    else if (selected_idx >= current_offset + max_lines) {
        new_offset = selected_idx - max_lines + 1;
    }

    // Ensure offset is within bounds
    if (new_offset < 0) {
        new_offset = 0;
    }
    if (new_offset > total_count - max_lines) {
        new_offset = total_count - max_lines;
    }

    set_scroll_offset(app, new_offset);
}

// Validate and fix selection bounds
void validate_selection(AppData *app) {
    if (!app) return;

    if (app->current_tab == TAB_WINDOWS) {
        if (app->filtered_count <= 0) {
            app->selection.window_index = 0;
            app->selection.selected_window_id = 0;
        } else if (app->selection.window_index >= app->filtered_count) {
            app->selection.window_index = app->filtered_count - 1;
            app->selection.selected_window_id = app->filtered[app->selection.window_index].id;
        } else if (app->selection.window_index < 0) {
            app->selection.window_index = 0;
            app->selection.selected_window_id = app->filtered[0].id;
        }
        return;
    }

    if (app->current_tab == TAB_WORKSPACES) {
        if (app->filtered_workspace_count <= 0) {
            app->selection.workspace_index = 0;
            app->selection.selected_workspace_id = -1;
        } else if (app->selection.workspace_index >= app->filtered_workspace_count) {
            app->selection.workspace_index = app->filtered_workspace_count - 1;
            app->selection.selected_workspace_id = app->filtered_workspaces[app->selection.workspace_index].id;
        } else if (app->selection.workspace_index < 0) {
            app->selection.workspace_index = 0;
            app->selection.selected_workspace_id = app->filtered_workspaces[0].id;
        }
        return;
    }

    if (app->current_tab == TAB_HARPOON && app->filtered_harpoon_count > 0 &&
        app->selection.harpoon_index >= app->filtered_harpoon_count) {
        app->selection.harpoon_index = app->filtered_harpoon_count - 1;
    }

    if (app->current_tab == TAB_NAMES && app->filtered_names_count > 0 &&
        app->selection.names_index >= app->filtered_names_count) {
        app->selection.names_index = app->filtered_names_count - 1;
    }

    if (app->current_tab == TAB_CONFIG && app->filtered_config_count > 0 &&
        app->selection.config_index >= app->filtered_config_count) {
        app->selection.config_index = app->filtered_config_count - 1;
    }

    if (app->current_tab == TAB_RULES && app->filtered_rules_count > 0 &&
        app->selection.rules_index >= app->filtered_rules_count) {
        app->selection.rules_index = app->filtered_rules_count - 1;
    }

    const CofiTabProvider *provider = cofi_get_provider_for_tab(app->current_tab);
    if (provider && provider->row_count) {
        int count = provider->row_count(app);
        if (count > 0 && app->selection.provider_index >= count) {
            app->selection.provider_index = count - 1;
        } else if (count <= 0) {
            app->selection.provider_index = 0;
        }
    }

}
