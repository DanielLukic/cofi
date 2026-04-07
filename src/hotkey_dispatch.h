#ifndef HOTKEY_DISPATCH_H
#define HOTKEY_DISPATCH_H

#include <glib.h>

#include "app_data.h"
#include "types.h"

extern guint command_mode_timer;

gboolean delayed_command_mode(gpointer data);
void dispatch_hotkey_mode(AppData *app, ShowMode mode);

#endif
