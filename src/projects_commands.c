#include "projects_commands.h"

gchar *projects_build_tmux_attach_command(const char *tmux_path, const char *session_name) {
    if (!tmux_path || tmux_path[0] == '\0' ||
        !session_name || session_name[0] == '\0') return NULL;
    gchar *quoted_tmux = g_shell_quote(tmux_path);
    gchar *target = g_strconcat("=", session_name, NULL);
    gchar *quoted_target = g_shell_quote(target);
    gchar *command = g_strdup_printf("%s attach-session -t %s", quoted_tmux, quoted_target);
    g_free(quoted_target);
    g_free(target);
    g_free(quoted_tmux);
    return command;
}

gchar *projects_build_zellij_attach_command(const char *zellij_path, const char *session_name) {
    if (!zellij_path || zellij_path[0] == '\0' ||
        !session_name || session_name[0] == '\0') return NULL;
    gchar *quoted_zellij = g_shell_quote(zellij_path);
    gchar *quoted_session = g_shell_quote(session_name);
    gchar *command = g_strdup_printf("%s attach --create %s", quoted_zellij, quoted_session);
    g_free(quoted_session);
    g_free(quoted_zellij);
    return command;
}

gchar *projects_build_zellij_kill_command(const char *zellij_path, const char *session_name) {
    if (!zellij_path || zellij_path[0] == '\0' ||
        !session_name || session_name[0] == '\0') return NULL;
    gchar *quoted_zellij = g_shell_quote(zellij_path);
    gchar *quoted_session = g_shell_quote(session_name);
    gchar *command = g_strdup_printf("%s kill-session %s", quoted_zellij, quoted_session);
    g_free(quoted_session);
    g_free(quoted_zellij);
    return command;
}

gchar *projects_build_tmux_kill_command(const char *tmux_path, const char *session_name) {
    if (!tmux_path || tmux_path[0] == '\0' ||
        !session_name || session_name[0] == '\0') return NULL;
    gchar *quoted_tmux = g_shell_quote(tmux_path);
    gchar *target = g_strconcat("=", session_name, NULL);
    gchar *quoted_target = g_shell_quote(target);
    gchar *command = g_strdup_printf("%s kill-session -t %s", quoted_tmux, quoted_target);
    g_free(quoted_target);
    g_free(target);
    g_free(quoted_tmux);
    return command;
}

gchar *projects_build_tmux_rename_command(const char *tmux_path,
                                          const char *old_name,
                                          const char *new_name) {
    if (!tmux_path || tmux_path[0] == '\0' ||
        !old_name || old_name[0] == '\0' ||
        !new_name || new_name[0] == '\0') return NULL;
    gchar *quoted_tmux = g_shell_quote(tmux_path);
    gchar *target = g_strconcat("=", old_name, NULL);
    gchar *quoted_target = g_shell_quote(target);
    gchar *quoted_new_name = g_shell_quote(new_name);
    gchar *command = g_strdup_printf("%s rename-session -t %s %s",
                                     quoted_tmux, quoted_target, quoted_new_name);
    g_free(quoted_new_name);
    g_free(quoted_target);
    g_free(target);
    g_free(quoted_tmux);
    return command;
}

gchar *projects_build_tmux_new_command(const char *tmux_path,
                                       const char *session_name,
                                       const char *start_dir) {
    if (!tmux_path || tmux_path[0] == '\0' ||
        !session_name || session_name[0] == '\0' ||
        !start_dir || start_dir[0] == '\0') {
        return NULL;
    }
    gchar *quoted_tmux = g_shell_quote(tmux_path);
    gchar *quoted_name = g_shell_quote(session_name);
    gchar *quoted_dir = g_shell_quote(start_dir);
    gchar *command = g_strdup_printf("%s new-session -A -s %s -c %s",
                                     quoted_tmux, quoted_name, quoted_dir);
    g_free(quoted_dir);
    g_free(quoted_name);
    g_free(quoted_tmux);
    return command;
}

gchar *projects_build_zellij_new_command(const char *zellij_path,
                                         const char *session_name,
                                         const char *start_dir) {
    if (!zellij_path || zellij_path[0] == '\0' ||
        !session_name || session_name[0] == '\0' ||
        !start_dir || start_dir[0] == '\0') {
        return NULL;
    }
    gchar *quoted_zellij = g_shell_quote(zellij_path);
    gchar *quoted_name = g_shell_quote(session_name);
    gchar *quoted_dir = g_shell_quote(start_dir);
    gchar *command = g_strdup_printf("cd %s && %s attach --create %s",
                                     quoted_dir, quoted_zellij, quoted_name);
    g_free(quoted_dir);
    g_free(quoted_name);
    g_free(quoted_zellij);
    return command;
}
