#include "tab_switching.h"

#include <stdio.h>
#include <string.h>

#include "cofi_tab_provider.h"
#include "config.h"
#include "display.h"
#include "filter.h"
#include "log.h"
#include "selection.h"
#include "tab_metadata.h"
#include "workspaces_provider.h"

static gboolean provider_tick(gpointer data) {
    AppData *app = (AppData *)data;
    if (!app) return FALSE;

    const CofiTabProvider *p = cofi_get_provider_for_tab(app->current_tab);
    if (!p || !p->on_tick || app->current_tab != app->provider_tick_tab) {
        app->provider_tick_timer_id = 0;
        return FALSE;
    }

    int provider_id = cofi_get_provider_id_for_tab(app->current_tab);
    p->on_tick(app, cofi_next_generation(provider_id));
    return TRUE;
}

static void stop_provider_tick(AppData *app) {
    if (!app || app->provider_tick_timer_id == 0) return;
    g_source_remove(app->provider_tick_timer_id);
    app->provider_tick_timer_id = 0;
}

static void start_provider_tick(AppData *app, const CofiTabProvider *p) {
    stop_provider_tick(app);
    if (!app || !p || !p->on_tick || p->tick_interval_ms <= 0) return;
    app->provider_tick_tab = (TabMode)p->tab_mode;
    app->provider_tick_timer_id =
        g_timeout_add((guint)p->tick_interval_ms, provider_tick, app);
}

static int collect_tab_order(int *tabs, int max_tabs) {
    if (!tabs || max_tabs <= 0) return 0;
    int count = 0;
    tabs[count++] = TAB_WINDOWS;
    count += cofi_list_provider_tabs(tabs + count, max_tabs - count);
    return count;
}

static TabMode find_next_visible_tab(AppData *app, TabMode start_tab, int direction) {
    int tabs[COFI_MAX_TAB_HANDLES];
    int count = collect_tab_order(tabs, COFI_MAX_TAB_HANDLES);
    if (count <= 0) return start_tab;

    int current = 0;
    for (int i = 0; i < count; i++) {
        if (tabs[i] == (int)start_tab) {
            current = i;
            break;
        }
    }

    for (int i = 1; i <= count; i++) {
        int idx = (current + (direction * i) + count) % count;
        if (tab_is_visible(app, (TabMode)tabs[idx])) {
            return (TabMode)tabs[idx];
        }
    }

    return start_tab;
}

void switch_to_tab(AppData *app, TabMode target_tab) {
    if (target_tab != TAB_WINDOWS && !cofi_get_provider_for_tab(target_tab)) {
        target_tab = TAB_WINDOWS;
    }

    if (app->current_tab == target_tab) {
        return;
    }

    TabMode previous_tab = app->current_tab;
    const CofiTabProvider *previous_provider = cofi_get_provider_for_tab(previous_tab);
    if (previous_provider && previous_tab != target_tab) {
        if (previous_provider->on_leave) previous_provider->on_leave(app);
        stop_provider_tick(app);
    }

    app->current_tab = target_tab;
    gtk_entry_set_text(GTK_ENTRY(app->entry), "");

    if (target_tab == TAB_WINDOWS) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type to filter windows...");
        filter_windows(app, "");
    } else {
        const CofiTabProvider *p = cofi_get_provider_for_tab(target_tab);
        if (p && p->on_enter)
            p->on_enter(app);
        start_provider_tick(app, p);
    }

    reset_selection(app);
    update_display(app);

    log_debug("Switched to %s tab", tab_display_name(target_tab));
}

void surface_tab(AppData *app, TabMode tab) {
    if (!app) {
        return;
    }
    if (tab != TAB_WINDOWS && !cofi_get_provider_for_tab(tab)) {
        log_warn("Cannot surface unavailable tab: %s", tab_display_name(tab));
        return;
    }

    if (tab < TAB_WINDOWS || tab >= COFI_MAX_TAB_HANDLES) {
        log_warn("Cannot surface invalid tab: %d", tab);
        return;
    }

    if (app->tab_visibility[tab] == TAB_VIS_HIDDEN) {
        app->tab_visibility[tab] = TAB_VIS_SURFACED;
    }

    switch_to_tab(app, tab);
}

void clear_surfaced_tabs(AppData *app) {
    if (!app) {
        return;
    }

    for (int i = 0; i < COFI_MAX_TAB_HANDLES; i++) {
        if (app->tab_visibility[i] == TAB_VIS_SURFACED) {
            app->tab_visibility[i] = TAB_VIS_HIDDEN;
        }
    }
}

gboolean tab_is_visible(AppData *app, TabMode tab) {
    if (!app) {
        return FALSE;
    }
    if (tab < TAB_WINDOWS || tab >= COFI_MAX_TAB_HANDLES) {
        return FALSE;
    }
    if (tab != TAB_WINDOWS && !cofi_get_provider_for_tab(tab)) {
        return FALSE;
    }
    if (app->config.show_all_tabs) {
        return TRUE;
    }

    return app->tab_visibility[tab] == TAB_VIS_PINNED ||
           app->tab_visibility[tab] == TAB_VIS_SURFACED;
}

gboolean handle_tab_switching(GdkEventKey *event, AppData *app) {
    int direction = 0;

    if ((event->keyval == GDK_KEY_Tab && !(event->state & GDK_CONTROL_MASK)) ||
        event->keyval == GDK_KEY_ISO_Left_Tab) {
        direction = ((event->state & GDK_SHIFT_MASK) || event->keyval == GDK_KEY_ISO_Left_Tab) ? -1 : 1;
    }

    if (direction == 0) {
        return FALSE;
    }

    TabMode next_tab = find_next_visible_tab(app, app->current_tab, direction);

    if (direction < 0) {
        log_debug("USER: SHIFT+TAB pressed -> Switching to %s tab", tab_display_name(next_tab));
    } else {
        log_debug("USER: TAB pressed -> Switching to %s tab", tab_display_name(next_tab));
    }

    if (app->tab_visibility[app->current_tab] == TAB_VIS_SURFACED &&
        app->tab_visibility[next_tab] == TAB_VIS_PINNED) {
        clear_surfaced_tabs(app);
    }

    switch_to_tab(app, next_tab);
    return TRUE;
}
