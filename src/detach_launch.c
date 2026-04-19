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

typedef const char *(*ProgramResolver)(const char *program);

static const char *resolve_via_glib(const char *program) {
    gchar *path = g_find_program_in_path(program);
    if (path) {
        g_free(path);
        return program;
    }
    return NULL;
}

static const char *detect_terminal_with_resolver(ProgramResolver resolver) {
    const char *env_term = g_getenv("TERMINAL");
    if (env_term && env_term[0] != '\0' && resolver(env_term)) {
        return env_term;
    }

    static const char *const CANDIDATES[] = {
        "x-terminal-emulator",
        "mate-terminal",
        "gnome-terminal",
        "konsole",
        "alacritty",
        "kitty",
        "foot",
        "wezterm",
        "urxvt",
        "xterm",
        NULL
    };

    for (int i = 0; CANDIDATES[i]; i++) {
        if (resolver(CANDIDATES[i])) {
            return CANDIDATES[i];
        }
    }

    return "xterm";
}

gboolean detach_launch_in_terminal(const char *exec_path) {
    if (!exec_path || exec_path[0] == '\0') {
        return FALSE;
    }

    const char *term = detect_terminal_with_resolver(resolve_via_glib);
    const char *argv[] = {term, "-e", exec_path, NULL};
    gboolean ok = spawn_detachedv(argv, exec_path);

    if (ok) {
        log_info("Launched PATH binary in terminal '%s': %s", term, exec_path);
    }

    return ok;
}

#ifdef COFI_TESTING
const char *detect_terminal_for_test(ProgramResolver resolver) {
    return detect_terminal_with_resolver(resolver);
}
#endif
