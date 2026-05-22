#ifndef WINDOW_APPEARANCE_H
#define WINDOW_APPEARANCE_H

#include <X11/Xlib.h>

#include "window_info.h"

int collect_new_window_ids(const Window *old_ids, int old_count,
                           const WindowInfo *windows, int window_count,
                           Window *out_new_ids, int max_new_ids);

#endif
