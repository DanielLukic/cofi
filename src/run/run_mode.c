#include "run/run_mode.h"

#include <string.h>

#include "core/log/log.h"

void init_run_mode(RunMode *run_mode) {
    if (!run_mode) return;
    memset(run_mode, 0, sizeof(*run_mode));
    run_mode->history_index = -1;
}

gboolean extract_run_command(const char *entry_text, char *command_out, size_t command_size) {
    if (!command_out || command_size == 0) return FALSE;

    command_out[0] = '\0';
    if (!entry_text) return FALSE;

    const char *command = entry_text;
    if (command[0] == '!') command++;

    while (*command && g_ascii_isspace(*command)) command++;

    g_strlcpy(command_out, command, command_size);
    g_strstrip(command_out);
    return command_out[0] != '\0';
}

void add_run_history_entry(RunMode *run_mode, const char *command) {
    if (!run_mode || !command || command[0] == '\0') return;

    if (run_mode->history_count > 0 &&
        strcmp(run_mode->history[0], command) == 0) {
        return;
    }

    int cap = RUN_HISTORY_CAP;
    for (int i = (cap - 1 < run_mode->history_count ? cap - 1 : run_mode->history_count); i > 0; i--) {
        strcpy(run_mode->history[i], run_mode->history[i - 1]);
    }

    g_strlcpy(run_mode->history[0], command, sizeof(run_mode->history[0]));
    if (run_mode->history_count < RUN_HISTORY_CAP) run_mode->history_count++;
    run_mode->history_index = -1;
}
