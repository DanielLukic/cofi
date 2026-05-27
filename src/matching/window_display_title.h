#ifndef WINDOW_DISPLAY_TITLE_H
#define WINDOW_DISPLAY_TITLE_H

#include <stddef.h>
#include "x11/window_info.h"
#include "matching/match_entry.h"

void compose_window_display_title(const MatchEntryManager *manager,
                                  const WindowInfo *window,
                                  char *out, size_t out_size);

#endif
