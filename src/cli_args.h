#ifndef CLI_ARGS_H
#define CLI_ARGS_H

#include "app_data.h"

// Command line argument parsing functions
void print_usage(const char *prog_name);
int parse_log_level(const char *level_str);
WindowAlignment parse_alignment(const char *align_str);
int parse_command_line(int argc, char *argv[], AppData *app, char **log_file, int *log_enabled, int *alignment_specified, int *close_on_focus_loss_specified);

#endif // CLI_ARGS_H