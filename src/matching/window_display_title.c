#include <stdio.h>
#include "matching/window_display_title.h"

void compose_window_display_title(const MatchEntryManager *manager,
                                  const WindowInfo *window,
                                  char *out, size_t out_size) {
    if (!window || !out || out_size == 0) return;
    const char *custom_name = manager ? match_entry_get_custom_name(manager, window->id) : NULL;
    if (custom_name) snprintf(out, out_size, "%s - %s", custom_name, window->title);
    else snprintf(out, out_size, "%s", window->title);
}
