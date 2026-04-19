#include "detach_launch.h"

#include <gio/gio.h>

#include "log.h"

static gboolean spawn_detachedv(const char *const *argv, const char *label) {
    GError *error = NULL;
    GSubprocessLauncher *launcher = g_subprocess_launcher_new(
        (GSubprocessFlags)(G_SUBPROCESS_FLAGS_STDIN_INHERIT |
                           G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                           G_SUBPROCESS_FLAGS_STDERR_SILENCE));

    GSubprocess *process = g_subprocess_launcher_spawnv(launcher, argv, &error);
    g_object_unref(launcher);

    if (!process) {
        log_error("Failed to launch '%s': %s",
                  label,
                  error ? error->message : "unknown error");
        g_clear_error(&error);
        return FALSE;
    }

    g_object_unref(process);
    return TRUE;
}

gboolean detach_launch_shell(const char *command) {
    if (!command || command[0] == '\0') {
        return FALSE;
    }

    const char *shell = g_getenv("SHELL");
    if (!shell || shell[0] == '\0') {
        shell = "/bin/sh";
    }

    const char *argv[] = {shell, "-c", command, NULL};
    gboolean ok = spawn_detachedv(argv, command);

    if (ok) {
        log_info("USER: Launched run command '%s'", command);
    }

    return ok;
}

gboolean detach_launch_argv(const char *exec_path) {
    if (!exec_path || exec_path[0] == '\0') {
        return FALSE;
    }

    const char *argv[] = {exec_path, NULL};
    gboolean ok = spawn_detachedv(argv, exec_path);

    if (ok) {
        log_info("Launched PATH binary: %s", exec_path);
    }

    return ok;
}
