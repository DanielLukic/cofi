#ifndef SESSIONS_COMMANDS_H
#define SESSIONS_COMMANDS_H

#include <glib.h>

gchar *sessions_build_tmux_attach_command(const char *session_name);
gchar *sessions_build_zellij_attach_command(const char *session_name);
gchar *sessions_build_tmux_kill_command(const char *session_name);
gchar *sessions_build_zellij_kill_command(const char *session_name);
gchar *sessions_build_tmux_rename_command(const char *old_name, const char *new_name);
gchar *sessions_build_tmux_new_command(const char *session_name, const char *start_dir);
gchar *sessions_build_zellij_new_command(const char *session_name, const char *start_dir);

#endif /* SESSIONS_COMMANDS_H */
