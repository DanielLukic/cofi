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

gchar *projects_build_remote_attach_command(ProjectBackend backend,
                                            const char *host,
                                            const char *tool_path,
                                            const char *session_name) {
    if (!host || !host[0] || !tool_path || !tool_path[0] ||
        !session_name || !session_name[0]) return NULL;
    gchar *quoted_host = g_shell_quote(host);
    gchar *quoted_tool = g_shell_quote(tool_path);
    gchar *quoted_name = g_shell_quote(session_name);
    gchar *command = backend == PROJECT_BACKEND_ZELLIJ
        ? g_strdup_printf("ssh -t %s %s attach --create %s",
                          quoted_host, quoted_tool, quoted_name)
        : g_strdup_printf("ssh -t %s %s attach-session -t %s",
                          quoted_host, quoted_tool, quoted_name);
    g_free(quoted_name);
    g_free(quoted_tool);
    g_free(quoted_host);
    return command;
}

gchar *projects_build_remote_new_command(ProjectBackend backend,
                                         const char *host,
                                         const char *tool_path,
                                         const char *session_name,
                                         const char *start_dir) {
    if (!host || !host[0] || !tool_path || !tool_path[0] ||
        !session_name || !session_name[0] ||
        !start_dir || !start_dir[0]) return NULL;
    gchar *quoted_host = g_shell_quote(host);
    gchar *quoted_tool = g_shell_quote(tool_path);
    gchar *quoted_name = g_shell_quote(session_name);
    gchar *quoted_dir = g_shell_quote(start_dir);
    gchar *command = backend == PROJECT_BACKEND_ZELLIJ
        ? g_strdup_printf("ssh -t %s bash -lc \"cd %s && %s attach --create %s\"",
                          quoted_host, quoted_dir, quoted_tool, quoted_name)
        : g_strdup_printf("ssh -t %s %s new -A -s %s -c %s",
                          quoted_host, quoted_tool, quoted_name, quoted_dir);
    g_free(quoted_dir);
    g_free(quoted_name);
    g_free(quoted_tool);
    g_free(quoted_host);
    return command;
}

gchar *projects_build_folder_terminal_command(const char *path) {
    if (!path || path[0] == '\0') return NULL;
    gchar *quoted_path = g_shell_quote(path);
    gchar *command = g_strdup_printf("cd %s && exec \"${SHELL:-bash}\" -l", quoted_path);
    g_free(quoted_path);
    return command;
}

gchar *projects_build_remote_folder_terminal_command(const char *host, const char *path) {
    if (!host || host[0] == '\0' || !path || path[0] == '\0') return NULL;
    gchar *quoted_host = g_shell_quote(host);
    gchar *quoted_path = g_shell_quote(path);
    gchar *remote_cmd = g_strdup_printf("cd %s && exec \"${SHELL:-bash}\" -l", quoted_path);
    gchar *quoted_remote_cmd = g_shell_quote(remote_cmd);
    gchar *command = g_strdup_printf("ssh -t %s %s", quoted_host, quoted_remote_cmd);
    g_free(quoted_remote_cmd);
    g_free(remote_cmd);
    g_free(quoted_path);
    g_free(quoted_host);
    return command;
}

gchar *projects_with_terminal_title(const char *command, const char *title) {
    if (!command || command[0] == '\0' || !title || title[0] == '\0') {
        return NULL;
    }
    gchar *quoted_title = g_shell_quote(title);
    gchar *wrapped = g_strdup_printf("printf '\\033]2;%%s\\007' %s; %s",
                                     quoted_title, command);
    g_free(quoted_title);
    return wrapped;
}
