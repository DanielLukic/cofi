#include "repeat_action.h"
#include "app_data.h"
#include "display.h"
#include "filter.h"
#include "selection.h"
#include "window_highlight.h"
#include "x11_events.h"
#include "window_lifecycle.h"
#include "log.h"
#include <string.h>

void store_last_windows_query(AppData *app, const char *query) {
    if (!query || query[0] == '\0') {
        log_debug("Repeat: empty query not stored");
        return;
    }

    strncpy(app->last_windows_query, query, sizeof(app->last_windows_query) - 1);
    app->last_windows_query[sizeof(app->last_windows_query) - 1] = '\0';
    app->last_windows_query_valid = TRUE;
    log_debug("Repeat: stored query '%s'", app->last_windows_query);
}

void handle_repeat_key(AppData *app) {
    if (!app->last_windows_query_valid) {
        log_debug("Repeat: no stored query, no-op");
        return;
    }

    filter_windows(app, app->last_windows_query);
    reset_selection(app);

    WindowInfo *win = get_selected_window(app);
    if (!win) {
        log_debug("Repeat: query '%s' has no current matches, no-op",
                  app->last_windows_query);
        return;
    }

    log_debug("Repeat: activating '%s' (0x%lx) for query '%s'",
              win->title, win->id, app->last_windows_query);
    set_workspace_switch_state(1);
    activate_window(app->display, win->id);
    highlight_window(app, win->id);
    hide_window(app);
}
