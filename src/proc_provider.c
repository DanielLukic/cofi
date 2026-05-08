#include "proc_provider.h"

#include "app_data.h"
#include "cofi_tab_provider.h"
#include "log.h"
#include "proc.h"
#include "tab_switching.h"

#include <gtk/gtk.h>
#include <signal.h>
#include <stdint.h>

static int proc_provider_row_count(AppData *app) {
    return proc_row_count(app);
}

static void proc_provider_format_row(AppData *app, int raw_idx, CofiRowCells *out) {
    proc_format_row(app, raw_idx, out);
}

static const char *proc_provider_match_string(AppData *app, int raw_idx) {
    return proc_match_string(app, raw_idx);
}

static const char *proc_provider_row_identity(AppData *app, int raw_idx) {
    return proc_row_identity(app, raw_idx);
}

static CofiActionStatus proc_provider_on_enter_pressed(AppData *app, int filtered_idx,
                                                       int raw_idx, const char *entry_text,
                                                       int modifier_state) {
    (void)filtered_idx;
    (void)raw_idx;
    if (proc_execute_action_with_modifiers(app, entry_text, (guint)modifier_state)) {
        return COFI_HANDLED_HIDE;
    }
    return COFI_NO_OP;
}

static CofiActionStatus proc_provider_on_command_args(AppData *app, const char *args) {
    (void)app;
    if (args && args[0] != '\0') {
        log_warn("proc: ignoring command args '%s'", args);
        return COFI_HANDLED_KEEP;
    }
    return COFI_NO_OP;
}

static CofiActionStatus proc_signal_handler(AppData *app, void *const *payloads, int count,
                                            void *user_data) {
    (void)payloads;
    (void)count;
    (void)user_data;
    if (!app) return COFI_NO_OP;
    return proc_execute_action_with_modifiers(app, gtk_entry_get_text(GTK_ENTRY(app->entry)), 0)
               ? COFI_HANDLED_HIDE
               : COFI_NO_OP;
}

static CofiActionStatus proc_show_handler(AppData *app, void *const *payloads, int count,
                                          void *user_data) {
    (void)payloads;
    (void)count;
    (void)user_data;
    if (!app) return COFI_NO_OP;
    return proc_execute_action_with_modifiers(app, gtk_entry_get_text(GTK_ENTRY(app->entry)), 0)
               ? COFI_HANDLED_HIDE
               : COFI_NO_OP;
}

static const CofiPipeAction s_proc_actions[] = {
    {"k", {"kill", "term", "t", NULL}, (void *)(intptr_t)SIGTERM, proc_signal_handler},
    {"9", {"kill9", "force", NULL}, (void *)(intptr_t)SIGKILL, proc_signal_handler},
    {"h", {"hup", NULL}, (void *)(intptr_t)SIGHUP, proc_signal_handler},
    {"s", {"stop", NULL}, (void *)(intptr_t)SIGSTOP, proc_signal_handler},
    {"c", {"cont", NULL}, (void *)(intptr_t)SIGCONT, proc_signal_handler},
    {"w", {"show", "raise", NULL}, NULL, proc_show_handler},
    {NULL, {NULL}, NULL, NULL}
};

static const CofiPipeActionTable s_proc_pipe_table = {
    .actions = s_proc_actions,
};

static const char *const s_proc_aliases[] = {"ps", NULL};
static CofiTabProvider s_proc_provider;

void proc_provider_register(void) {
    cofi_init_provider_defaults(&s_proc_provider);
    s_proc_provider.tab_mode = TAB_PROC;
    s_proc_provider.id = "proc";
    s_proc_provider.display_name = "PROC";
    s_proc_provider.primary_cmd = "proc";
    s_proc_provider.aliases = s_proc_aliases;
    s_proc_provider.prefix_char = 0;
    s_proc_provider.modal_policy = COFI_MODAL_HIDE_ON_ESC;
    s_proc_provider.hidden_by_default = 1;
    s_proc_provider.initial_selection_index = 0;
    s_proc_provider.row_count = proc_provider_row_count;
    s_proc_provider.format_row = proc_provider_format_row;
    s_proc_provider.match_string = proc_provider_match_string;
    s_proc_provider.row_identity = proc_provider_row_identity;
    s_proc_provider.on_enter = proc_on_enter;
    s_proc_provider.on_leave = proc_on_leave;
    s_proc_provider.on_query_changed = proc_on_query_changed;
    s_proc_provider.on_tick = proc_on_tick;
    s_proc_provider.tick_interval_ms = 1500;
    s_proc_provider.on_enter_pressed = proc_provider_on_enter_pressed;
    s_proc_provider.on_command_args = proc_provider_on_command_args;
    s_proc_provider.pipe_actions = &s_proc_pipe_table;
    s_proc_provider.slot_store_enabled = 0;
    cofi_register_tab_provider(&s_proc_provider);
}
