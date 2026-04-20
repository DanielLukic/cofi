#ifndef DETACH_LAUNCH_H
#define DETACH_LAUNCH_H

#include <glib.h>

gboolean detach_launch_shell(const char *command);
gboolean detach_launch_argv(const char *exec_path);
gboolean detach_launch_argv_array(const char *const *argv);
gboolean detach_launch_in_terminal_cmd(const char *cmd);

// Strip %X field codes from a .desktop Exec= command line.
// Returns a newly allocated string. Caller frees with g_free.
gchar *detach_strip_field_codes(const char *cmd);

#ifdef COFI_TESTING
typedef const char *(*ProgramResolver)(const char *program);
typedef const char *(*DesktopTerminalGetter)(const char *desktop, ProgramResolver resolver);
const char *detect_terminal_for_test(ProgramResolver resolver);
const char *detect_terminal_with_desktop_for_test(ProgramResolver resolver, DesktopTerminalGetter dt_getter);
char **build_systemd_run_argv_for_test(const char *const *inner_argv);
gboolean fork_setsid_exec_for_test(const char *const *argv);
char **build_terminal_cmd_argv_for_test(const char *cmd, ProgramResolver resolver);
#endif

#endif // DETACH_LAUNCH_H
