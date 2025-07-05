#ifndef COMMAND_MODE_H
#define COMMAND_MODE_H

#include <gtk/gtk.h>
#include "app_data.h"

// Help text format options
typedef enum {
    HELP_FORMAT_CLI,    // For command line output
    HELP_FORMAT_GUI     // For GUI text buffer
} HelpFormat;

void init_command_mode(CommandMode *cmd);
void enter_command_mode(AppData *app);
void exit_command_mode(AppData *app);
gboolean handle_command_key(GdkEventKey *event, AppData *app);
gboolean execute_command(const char *command, AppData *app);
void show_help_commands(AppData *app);

// Shared help text generation
char* generate_command_help_text(HelpFormat format);

#endif // COMMAND_MODE_H