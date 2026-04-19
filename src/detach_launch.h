#ifndef DETACH_LAUNCH_H
#define DETACH_LAUNCH_H

#include <glib.h>

gboolean detach_launch_shell(const char *command);
gboolean detach_launch_argv(const char *exec_path);

#endif // DETACH_LAUNCH_H
