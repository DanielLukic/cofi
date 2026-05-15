#include "sessions_commands.h"

gchar *sessions_build_tmux_attach_command(const char *session_name) {
    if (!session_name || session_name[0] == '\0') return NULL;
    gchar *target = g_strconcat("=", session_name, NULL);
    gchar *quoted_target = g_shell_quote(target);
    gchar *command = g_strdup_printf("tmux attach-session -t %s", quoted_target);
    g_free(quoted_target);
    g_free(target);
    return command;
}

gchar *sessions_build_zellij_attach_command(const char *session_name) {
    if (!session_name || session_name[0] == '\0') return NULL;
    gchar *quoted_session = g_shell_quote(session_name);
    gchar *command = g_strdup_printf("zellij attach --create %s", quoted_session);
    g_free(quoted_session);
    return command;
}

gchar *sessions_build_zellij_kill_command(const char *session_name) {
    if (!session_name || session_name[0] == '\0') return NULL;
    gchar *quoted_session = g_shell_quote(session_name);
    gchar *command = g_strdup_printf("zellij kill-session %s", quoted_session);
    g_free(quoted_session);
    return command;
}

gchar *sessions_build_tmux_kill_command(const char *session_name) {
    if (!session_name || session_name[0] == '\0') return NULL;
    gchar *target = g_strconcat("=", session_name, NULL);
    gchar *quoted_target = g_shell_quote(target);
    gchar *command = g_strdup_printf("tmux kill-session -t %s", quoted_target);
    g_free(quoted_target);
    g_free(target);
    return command;
}

gchar *sessions_build_tmux_rename_command(const char *old_name, const char *new_name) {
    if (!old_name || old_name[0] == '\0' || !new_name || new_name[0] == '\0') return NULL;
    gchar *target = g_strconcat("=", old_name, NULL);
    gchar *quoted_target = g_shell_quote(target);
    gchar *quoted_new_name = g_shell_quote(new_name);
    gchar *command = g_strdup_printf("tmux rename-session -t %s %s",
                                     quoted_target, quoted_new_name);
    g_free(quoted_new_name);
    g_free(quoted_target);
    g_free(target);
    return command;
}

gchar *sessions_build_tmux_new_command(const char *session_name, const char *start_dir) {
    if (!session_name || session_name[0] == '\0' || !start_dir || start_dir[0] == '\0') {
        return NULL;
    }
    gchar *quoted_name = g_shell_quote(session_name);
    gchar *quoted_dir = g_shell_quote(start_dir);
    gchar *command = g_strdup_printf("tmux new-session -A -s %s -c %s",
                                     quoted_name, quoted_dir);
    g_free(quoted_dir);
    g_free(quoted_name);
    return command;
}
