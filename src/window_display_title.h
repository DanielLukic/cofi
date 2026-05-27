#ifndef WINDOW_DISPLAY_TITLE_H
#define WINDOW_DISPLAY_TITLE_H

#include <stddef.h>
#include "window_info.h"
#include "match_entry.h"

void compose_window_display_title(const MatchEntryManager *manager,
                                  const WindowInfo *window,
                                  char *out, size_t out_size);

#endif
