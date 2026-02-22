#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <glib.h>
#include <stddef.h>

void trim_whitespace_in_place(char *text);
gboolean parse_command_and_arg(const char *input, char *cmd_out, char *arg_out,
                               size_t cmd_size, size_t arg_size);

#endif
