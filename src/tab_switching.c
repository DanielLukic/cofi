#include "tab_switching.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "display.h"
#include "filter.h"
#include "filter_names.h"
#include "match.h"
#include "log.h"
#include "selection.h"

void switch_to_tab(AppData *app, TabMode target_tab) {
    if (app->current_tab == target_tab) {
        return;
    }

    app->current_tab = target_tab;
    gtk_entry_set_text(GTK_ENTRY(app->entry), "");

    if (target_tab == TAB_WINDOWS) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter windows...");
        filter_windows(app, "");
    } else if (target_tab == TAB_WORKSPACES) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter workspaces...");
        filter_workspaces(app, "");
    } else if (target_tab == TAB_HARPOON) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter harpoon slots...");
        filter_harpoon(app, "");
    } else if (target_tab == TAB_NAMES) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter named windows...");
        filter_names(app, "");
    } else if (target_tab == TAB_CONFIG) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter config options...");
        filter_config(app, "");
    } else if (target_tab == TAB_HOTKEYS) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter hotkey bindings...");
        filter_hotkeys(app, "");
    }

    reset_selection(app);
    update_display(app);

    const char *tab_names[] = {"Windows", "Workspaces", "Harpoon", "Names", "Config", "Hotkeys"};
    log_debug("Switched to %s tab", tab_names[target_tab]);
}

gboolean handle_tab_switching(GdkEventKey *event, AppData *app) {
    if (event->keyval == GDK_KEY_Tab && !(event->state & GDK_CONTROL_MASK)) {
        TabMode next_tab;

        if (event->state & GDK_SHIFT_MASK) {
            switch (app->current_tab) {
                case TAB_WINDOWS: next_tab = TAB_HOTKEYS; break;
                case TAB_WORKSPACES: next_tab = TAB_WINDOWS; break;
                case TAB_HARPOON: next_tab = TAB_WORKSPACES; break;
                case TAB_NAMES: next_tab = TAB_HARPOON; break;
                case TAB_CONFIG: next_tab = TAB_NAMES; break;
                case TAB_HOTKEYS: next_tab = TAB_CONFIG; break;
                default: next_tab = TAB_WINDOWS; break;
            }
            const char *tab_names[] = {"Windows", "Workspaces", "Harpoon", "Names", "Config", "Hotkeys"};
            log_debug("USER: SHIFT+TAB pressed -> Switching to %s tab", tab_names[next_tab]);
        } else {
            switch (app->current_tab) {
                case TAB_WINDOWS: next_tab = TAB_WORKSPACES; break;
                case TAB_WORKSPACES: next_tab = TAB_HARPOON; break;
                case TAB_HARPOON: next_tab = TAB_NAMES; break;
                case TAB_NAMES: next_tab = TAB_CONFIG; break;
                case TAB_CONFIG: next_tab = TAB_HOTKEYS; break;
                case TAB_HOTKEYS: next_tab = TAB_WINDOWS; break;
                default: next_tab = TAB_WINDOWS; break;
            }
            const char *tab_names[] = {"Windows", "Workspaces", "Harpoon", "Names", "Config", "Hotkeys"};
            log_debug("USER: TAB pressed -> Switching to %s tab", tab_names[next_tab]);
        }

        switch_to_tab(app, next_tab);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_ISO_Left_Tab) {
        TabMode next_tab;
        switch (app->current_tab) {
            case TAB_WINDOWS: next_tab = TAB_HOTKEYS; break;
            case TAB_WORKSPACES: next_tab = TAB_WINDOWS; break;
            case TAB_HARPOON: next_tab = TAB_WORKSPACES; break;
            case TAB_NAMES: next_tab = TAB_HARPOON; break;
            case TAB_CONFIG: next_tab = TAB_NAMES; break;
            case TAB_HOTKEYS: next_tab = TAB_CONFIG; break;
            default: next_tab = TAB_WINDOWS; break;
        }

        const char *tab_names[] = {"Windows", "Workspaces", "Harpoon", "Names", "Config", "Hotkeys"};
        log_debug("USER: SHIFT+TAB pressed -> Switching to %s tab", tab_names[next_tab]);
        switch_to_tab(app, next_tab);
        return TRUE;
    }

    return FALSE;
}

void filter_workspaces(AppData *app, const char *filter) {
    app->filtered_workspace_count = 0;

    if (!filter || !*filter) {
        for (int i = 0; i < app->workspace_count; i++) {
            app->filtered_workspaces[app->filtered_workspace_count++] = app->workspaces[i];
        }
        return;
    }

    char searchable[512];
    for (int i = 0; i < app->workspace_count; i++) {
        snprintf(searchable, sizeof(searchable), "%d %s",
                 app->workspaces[i].id + 1, app->workspaces[i].name);

        if (has_match(filter, searchable)) {
            app->filtered_workspaces[app->filtered_workspace_count++] = app->workspaces[i];
        }
    }
}

void filter_harpoon(AppData *app, const char *filter) {
    app->filtered_harpoon_count = 0;

    if (!filter || !*filter) {
        for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
            HarpoonSlot *slot = &app->harpoon.slots[i];
            if (slot->assigned) {
                app->filtered_harpoon[app->filtered_harpoon_count] = *slot;
                app->filtered_harpoon_indices[app->filtered_harpoon_count] = i;
                app->filtered_harpoon_count++;
            }
        }
        return;
    }

    char searchable[1024];
    for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
        HarpoonSlot *slot = &app->harpoon.slots[i];

        char slot_name[4];
        if (i < 10) {
            snprintf(slot_name, sizeof(slot_name), "%d", i);
        } else {
            snprintf(slot_name, sizeof(slot_name), "%c", 'a' + (i - 10));
        }

        if (slot->assigned) {
            snprintf(searchable, sizeof(searchable), "%s %s %s %s",
                     slot_name, slot->title, slot->class_name, slot->instance);

            if (has_match(filter, searchable)) {
                app->filtered_harpoon[app->filtered_harpoon_count] = *slot;
                app->filtered_harpoon_indices[app->filtered_harpoon_count] = i;
                app->filtered_harpoon_count++;
            }
        }
    }
}

void filter_config(AppData *app, const char *filter) {
    ConfigEntry all_entries[MAX_CONFIG_ENTRIES];
    int all_count = 0;
    build_config_entries(&app->config, all_entries, &all_count);

    app->filtered_config_count = 0;

    if (!filter || !*filter) {
        for (int i = 0; i < all_count; i++) {
            app->filtered_config[app->filtered_config_count++] = all_entries[i];
        }
        return;
    }

    for (int i = 0; i < all_count; i++) {
        char searchable[256];
        snprintf(searchable, sizeof(searchable), "%s %s", all_entries[i].key, all_entries[i].value);
        if (has_match(filter, searchable)) {
            app->filtered_config[app->filtered_config_count++] = all_entries[i];
        }
    }
}

void filter_hotkeys(AppData *app, const char *filter) {
    app->filtered_hotkeys_count = 0;

    if (!filter || !*filter) {
        for (int i = 0; i < app->hotkey_config.count; i++) {
            app->filtered_hotkeys[app->filtered_hotkeys_count] = app->hotkey_config.bindings[i];
            app->filtered_hotkeys_indices[app->filtered_hotkeys_count] = i;
            app->filtered_hotkeys_count++;
        }
        return;
    }

    for (int i = 0; i < app->hotkey_config.count; i++) {
        char searchable[512];
        snprintf(searchable, sizeof(searchable), "%s %s",
                 app->hotkey_config.bindings[i].key,
                 app->hotkey_config.bindings[i].command);
        if (has_match(filter, searchable)) {
            app->filtered_hotkeys[app->filtered_hotkeys_count] = app->hotkey_config.bindings[i];
            app->filtered_hotkeys_indices[app->filtered_hotkeys_count] = i;
            app->filtered_hotkeys_count++;
        }
    }
}
