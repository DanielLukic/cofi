#ifndef DETACH_LAUNCH_H
#define DETACH_LAUNCH_H

#include <glib.h>

gboolean detach_launch_shell(const char *command);
gboolean detach_launch_argv(const char *exec_path);
gboolean detach_launch_in_terminal(const char *exec_path);

#ifdef COFI_TESTING
typedef const char *(*ProgramResolver)(const char *program);
const char *detect_terminal_for_test(ProgramResolver resolver);
#endif

#endif // DETACH_LAUNCH_H
