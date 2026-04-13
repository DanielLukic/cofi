#ifndef RUN_MODE_H
#define RUN_MODE_H

#include <gtk/gtk.h>

#include "app_data.h"

void init_run_mode(RunMode *run_mode);
void enter_run_mode(AppData *app, const char *prefill_command);
void exit_run_mode(AppData *app);
gboolean handle_run_key(GdkEventKey *event, AppData *app);
void handle_run_entry_changed(GtkEntry *entry, AppData *app);

gboolean extract_run_command(const char *entry_text, char *command_out, size_t command_size);
void add_run_history_entry(RunMode *run_mode, const char *command);
gboolean browse_run_history(RunMode *run_mode, int direction, char *entry_text_out, size_t entry_text_size);

#endif // RUN_MODE_H
