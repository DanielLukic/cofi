#ifndef RUN_MODE_H
#define RUN_MODE_H

#include <glib.h>

#include "app_data.h"

void init_run_mode(RunMode *run_mode);

gboolean extract_run_command(const char *entry_text, char *command_out, size_t command_size);
void add_run_history_entry(RunMode *run_mode, const char *command);

#endif // RUN_MODE_H
