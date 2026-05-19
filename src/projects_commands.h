#ifndef PROJECTS_COMMANDS_H
#define PROJECTS_COMMANDS_H

#include <glib.h>

gchar *projects_build_tmux_attach_command(const char *tmux_path, const char *session_name);
gchar *projects_build_zellij_attach_command(const char *zellij_path, const char *session_name);
gchar *projects_build_tmux_kill_command(const char *tmux_path, const char *session_name);
gchar *projects_build_zellij_kill_command(const char *zellij_path, const char *session_name);
gchar *projects_build_tmux_rename_command(const char *tmux_path,
                                          const char *old_name,
                                          const char *new_name);
gchar *projects_build_tmux_new_command(const char *tmux_path,
                                       const char *session_name,
                                       const char *start_dir);
gchar *projects_build_zellij_new_command(const char *zellij_path,
                                         const char *session_name,
                                         const char *start_dir);

#endif /* PROJECTS_COMMANDS_H */
