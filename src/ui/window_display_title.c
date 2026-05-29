#include <stdio.h>
#include "ui/window_display_title.h"

void compose_window_display_title(const MatchEntryManager *manager,
                                  const NamesStore *names,
                                  const WindowInfo *window,
                                  char *out, size_t out_size) {
    if (!window || !out || out_size == 0) return;
    const char *custom_name = (manager && names) ? names_get_for_window(names, manager, window) : NULL;
    if (custom_name) snprintf(out, out_size, "%s - %s", custom_name, window->title);
    else snprintf(out, out_size, "%s", window->title);
}
