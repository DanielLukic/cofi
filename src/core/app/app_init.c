#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include "core/app/app_init.h"
#include "x11/window_list.h"
#include "x11/workspace_info.h"
#include "harpoon/harpoon.h"
#include "matching/match_entry.h"
#include "matching/match_entry_config.h"
#include "names/names_store.h"
#include "geom/layout_store.h"
#include "ui/window_filter.h"
#include "core/log/log.h"
#include "core/utils/utils.h"
#include "x11/x11_utils.h"
#include "x11/atom_cache.h"
#include "commands/command_mode.h"
#include "providers/cofi_tab_provider.h"
#include "run/run_mode.h"
#include "calc/calc.h"
#include "bluetooth/bluetooth_model.h"
#include "core/selection/selection.h"
#include "rules/rules_config.h"
#include "rules/rules.h"
#include "sinks/sinks.h"
#include "proc/proc.h"
#include "projects/projects.h"

void init_tab_visibility(AppData *app) {
    if (!app) {
        return;
    }

    for (int i = 0; i < COFI_MAX_TAB_HANDLES; i++) {
        app->tab_visibility[i] = TAB_VIS_HIDDEN;
    }

    app->tab_visibility[TAB_WINDOWS] = TAB_VIS_PINNED;
}

void apply_provider_default_visibility(AppData *app) {
    if (!app) {
        return;
    }

    for (int i = 0; i < cofi_provider_count(); i++) {
        if (!cofi_provider_is_enabled(i)) continue;
        const CofiTabProvider *provider = cofi_get_provider(i);
        if (!provider || provider->hidden_by_default) continue;
        if (provider->tab_mode < TAB_WINDOWS || provider->tab_mode >= COFI_MAX_TAB_HANDLES) continue;
        app->tab_visibility[provider->tab_mode] = TAB_VIS_PINNED;
    }
}

void init_app_data(AppData *app) {
    // Initialize history and active window tracking
    app->history_count = 0;
    app->active_window_id = -1; // Use -1 to force initial active window to be moved to front
    app->command_target_id = 0;

    // Preserve any startup delegate tab; reset only invalid values to windows.
    if (app->current_tab < TAB_WINDOWS || app->current_tab == TAB_COUNT ||
        app->current_tab >= COFI_MAX_TAB_HANDLES) {
        app->current_tab = TAB_WINDOWS;
    }

    init_tab_visibility(app);

    // Initialize selection state (will be properly set by init_selection later)
    init_selection(app);
    
    // Initialize harpoon manager
    init_harpoon_manager(&app->harpoon);

    // Initialize workspace slots, overlay state, and highlight
    init_workspace_slots(&app->workspace_slots);
    init_slot_overlay_state(&app->slot_overlays);
    init_window_highlight(&app->highlight);
    init_hotkey_config(&app->hotkey_config);
    init_hotkey_grab_state(&app->hotkey_grab_state);
    if (!load_hotkey_config(&app->hotkey_config)) {
        // Load failed — distinguish "file missing" (write defaults) from
        // "file corrupt" (keep defaults in memory only, never overwrite the
        // user's existing file — corruption may hide recoverable data).
        init_default_hotkey_config(&app->hotkey_config);
        if (!hotkey_config_file_exists()) {
            save_hotkey_config(&app->hotkey_config);
        } else {
            log_warn("hotkey_config: existing hotkeys.json failed to parse; "
                     "using defaults in memory; NOT overwriting the file");
        }
    }
    
    // Initialize harpoon tab data
    app->filtered_harpoon_count = 0;
    app->confirm_overlay.active = FALSE;
    app->confirm_overlay.title = NULL;
    app->confirm_overlay.info = NULL;
    app->confirm_overlay.on_confirm = NULL;

    // Initialize matching identities and name records
    match_entry_manager_init(&app->matching);
    names_store_init(&app->names);
    layout_store_init(&app->layouts);
    app->filtered_matching_count = 0;
    app->filtered_names_count = 0;
    app->harpoon.matching = &app->matching;
    app->harpoon.windows = app->windows;
    app->harpoon.window_count = &app->window_count;
    
    // Load matching identities and name records from configuration
    load_match_entries(&app->matching);
    names_store_load(&app->names);

    // Initialize rules
    init_rules_config(&app->rules_config);
    load_rules_config(&app->rules_config, &app->matching);
    // Persist repaired ids so repeated loads don't create new fallback entries.
    save_match_entries(&app->matching);
    save_rules_config(&app->rules_config, &app->matching);
    app->filtered_rules_count = 0;
    init_rule_state(&app->rule_state);
    init_rule_breaker(&app->rule_breaker);
    app->in_rule_dispatch = FALSE;

    // Initialize command mode
    init_command_mode(&app->command_mode);
    init_run_mode(&app->run_mode);

    // Initialize calc mode
    memset(&app->calc_mode, 0, sizeof(app->calc_mode));
    calc_history_load(&app->calc_mode);
    init_bluetooth_mode(&app->bluetooth_mode);
    init_sinks_mode(&app->sinks_mode);
    init_proc_mode(&app->proc_mode);
    init_projects_mode(&app->projects_mode);
    
    // Initialize window visibility state
    app->window_visible = FALSE;
    app->hotkey_capture_active = FALSE;

    // Initialize fixed window sizing state
    app->fixed_cols = 0;
    app->fixed_rows = 0;
    app->fixed_window_size_initializing = FALSE;
    app->fixed_size_allocate_handler_id = 0;
    app->pending_initial_render = FALSE;
    
    // Initialize timers
    app->focus_loss_timer = 0;
    app->focus_grab_timer = 0;
    app->initial_overlay_idle_id = 0;
    app->prefix_origin_tab = TAB_WINDOWS;
    app->active_prefix_claim = '\0';

    if (app->daemon_socket_fd == 0) {
        app->daemon_socket_fd = -1;
    }
    app->daemon_socket_watch_id = 0;
    app->daemon_socket_channel = NULL;
    app->provider_tick_timer_id = 0;
    app->provider_tick_tab = TAB_WINDOWS;
}

void init_x11_connection(AppData *app) {
    // Open X11 display
    app->display = XOpenDisplay(NULL);
    if (!app->display) {
        log_error("Cannot open X11 display");
        exit(1);
    }
    
    log_debug("X11 display opened successfully");
    
    // Initialize atom cache
    atom_cache_init(app->display, &app->atoms);
}

void init_workspaces(AppData *app) {
    // Get workspace list
    int num_desktops = get_number_of_desktops(app->display);
    int current_desktop = get_current_desktop(app->display);
    int desktop_count = 0;
    char** desktop_names = get_desktop_names(app->display, &desktop_count);
    
    app->workspace_count = (num_desktops < MAX_WORKSPACES) ? num_desktops : MAX_WORKSPACES;
    for (int i = 0; i < app->workspace_count; i++) {
        app->workspaces[i].id = i;
        safe_string_copy(app->workspaces[i].name, desktop_names[i], MAX_WORKSPACE_NAME_LEN);
        app->workspaces[i].is_current = (i == current_desktop);
        app->filtered_workspaces[i] = app->workspaces[i];
    }
    app->filtered_workspace_count = app->workspace_count;
    
    // Free desktop names
    for (int i = 0; i < desktop_count; i++) {
        free(desktop_names[i]);
    }
    free(desktop_names);
    
    log_debug("Found %d workspaces, current workspace: %d", app->workspace_count, current_desktop);
}

void init_window_list(AppData *app) {
    // Get window list
    get_window_list(app);
    
    // Check for matching reassignments after loading config and getting window list.
    // Persist immediately so matching.json reflects the bound state at startup
    // (otherwise the file lags until the next window event triggers a save).
    if (match_entry_reassign_live_windows(&app->matching, app->windows, app->window_count)) {
        save_match_entries(&app->matching);
    }
}

void init_history_from_windows(AppData *app) {
    // Initialize history with current windows
    for (int i = 0; i < app->window_count && i < MAX_WINDOWS; i++) {
        app->history[i] = app->windows[i];
    }
    app->history_count = app->window_count;
    
    // Initialize filtered list with all windows (this will process history)
    filter_windows(app, "");
    
    log_trace("First 3 windows in history after filter:");
    for (int i = 0; i < 3 && i < app->history_count; i++) {
        log_trace("  [%d] %s (0x%lx)", i, app->history[i].title, app->history[i].id);
    }
}
