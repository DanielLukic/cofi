#ifndef HOTKEY_DISPATCH_H
#define HOTKEY_DISPATCH_H

#include <glib.h>

#include "core/app/app_data.h"
#include "core/utils/types.h"

void dispatch_hotkey_mode(AppData *app, ShowMode mode);

#endif
