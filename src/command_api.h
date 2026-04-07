#ifndef COMMAND_API_H
#define COMMAND_API_H

#include <glib.h>

// Forward declarations
typedef struct AppData AppData;
typedef struct WindowInfo WindowInfo;

// Help text format options
typedef enum {
    HELP_FORMAT_CLI,
    HELP_FORMAT_GUI
} HelpFormat;

gboolean execute_command(const char *command, AppData *app);
gboolean execute_command_with_window(const char *command, AppData *app, WindowInfo *window);
gboolean execute_command_background(const char *command, AppData *app, WindowInfo *window);
gboolean should_keep_open_on_hotkey_auto(const char *command);

char *generate_command_help_text(HelpFormat format);

#endif
