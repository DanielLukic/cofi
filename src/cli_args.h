#ifndef CLI_ARGS_H
#define CLI_ARGS_H

#include "app_data.h"

#ifdef __cplusplus
extern "C" {
#endif

// Command line argument parsing functions
void print_usage(const char *prog_name);
void print_command_mode_help(void);
int parse_log_level(const char *level_str);
WindowAlignment parse_alignment(const char *align_str);
int parse_command_line(int argc, char *argv[], AppData *app, char **log_file, int *log_enabled, int *alignment_specified, int *close_on_focus_loss_specified, int *workspace_shortcut_specified);

#ifdef __cplusplus
}
#endif

#endif // CLI_ARGS_H