#ifndef COMMAND_MODE_H
#define COMMAND_MODE_H

#include <gtk/gtk.h>
#include "app_data.h"

void init_command_mode(CommandMode *cmd);
void enter_command_mode(AppData *app);
void exit_command_mode(AppData *app);
gboolean handle_command_key(GdkEventKey *event, AppData *app);
void command_update_candidates(CommandMode *cmd, const char *text);
void show_help_commands(AppData *app);

#endif // COMMAND_MODE_H