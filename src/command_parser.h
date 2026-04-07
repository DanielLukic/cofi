#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <glib.h>
#include <stddef.h>

void trim_whitespace_in_place(char *text);
gboolean parse_command_and_arg(const char *input, char *cmd_out, char *arg_out,
                               size_t cmd_size, size_t arg_size);
gboolean parse_command_for_execution(const char *input, char *cmd_out, char *arg_out,
                                     size_t cmd_size, size_t arg_size);
gboolean resolve_command_primary(const char *cmd_name, char *primary_out, size_t primary_size);
gboolean next_command_segment(char **cursor, char *segment_out, size_t segment_size);

#endif
