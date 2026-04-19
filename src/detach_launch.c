#include "detach_launch.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <gio/gio.h>

#include "log.h"

// ---------------------------------------------------------------------------
// Field code stripping
// ---------------------------------------------------------------------------

gchar *detach_strip_field_codes(const char *cmd) {
    if (!cmd) return g_strdup("");
    GString *result = g_string_new(NULL);
    for (const char *p = cmd; *p; p++) {
        if (*p == '%' && *(p + 1) != '\0') {
            p++;
            if (*p == '%') {
                g_string_append_c(result, '%');  // %% → literal %
            }
            // all other %X field codes are dropped
            continue;
        }
        g_string_append_c(result, *p);
    }
    // Trim trailing whitespace
    while (result->len > 0 && result->str[result->len - 1] == ' ') {
        g_string_truncate(result, result->len - 1);
    }
    return g_string_free(result, FALSE);
}

// ---------------------------------------------------------------------------
// Double-fork + setsid launch (no systemd dependency)
// ---------------------------------------------------------------------------

static gboolean fork_setsid_exec(const char *const *argv) {
    pid_t pid = fork();
    if (pid < 0) {
        log_error("fork() failed: %s", strerror(errno));
        return FALSE;
    }
    if (pid > 0) {
        // parent: reap intermediate child
        int status;
        waitpid(pid, &status, 0);
        return TRUE;
    }

    // intermediate child: create new session
    setsid();

    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > STDERR_FILENO) close(devnull);
    }

    // double-fork so grandchild is not a session leader
    pid_t gpid = fork();
    if (gpid < 0) _exit(1);
    if (gpid > 0) _exit(0);  // intermediate exits; grandchild continues

    // grandchild: exec target program
    execvp(argv[0], (char *const *)argv);
    _exit(127);
}

// ---------------------------------------------------------------------------
// systemd-run argv builder
// ---------------------------------------------------------------------------

static char **build_systemd_run_argv(const char *const *inner_argv) {
    int inner_len = 0;
    while (inner_argv[inner_len]) inner_len++;

    // ["systemd-run", "--user", "--scope", "--", inner_argv..., NULL]
    int total = 4 + inner_len + 1;
    char **result = g_new0(char *, total);
    result[0] = g_strdup("systemd-run");
    result[1] = g_strdup("--user");
    result[2] = g_strdup("--scope");
    result[3] = g_strdup("--");
    for (int i = 0; i < inner_len; i++) {
        result[4 + i] = g_strdup(inner_argv[i]);
    }
    result[4 + inner_len] = NULL;
    return result;
}

// ---------------------------------------------------------------------------
// Primary launch function: try systemd-run, fall back to fork+setsid
// ---------------------------------------------------------------------------

static gboolean detach_launch_properly(const char *const *argv, const char *label) {
    gchar *srun = g_find_program_in_path("systemd-run");
    if (srun) {
        g_free(srun);
        char **srun_argv = build_systemd_run_argv(argv);
        GError *error = NULL;
        GSubprocessLauncher *launcher = g_subprocess_launcher_new(
            (GSubprocessFlags)(G_SUBPROCESS_FLAGS_STDIN_INHERIT |
                               G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                               G_SUBPROCESS_FLAGS_STDERR_SILENCE));
        GSubprocess *process = g_subprocess_launcher_spawnv(
            launcher, (const char *const *)srun_argv, &error);
        g_object_unref(launcher);
        g_strfreev(srun_argv);
        if (!process) {
            log_warn("systemd-run failed for '%s': %s — falling back to fork+setsid",
                     label, error ? error->message : "unknown");
            g_clear_error(&error);
        } else {
            g_object_unref(process);
            log_info("Launched via systemd-run: %s", label);
            return TRUE;
        }
    }
    return fork_setsid_exec(argv);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

gboolean detach_launch_shell(const char *command) {
    if (!command || command[0] == '\0') {
        return FALSE;
    }

    const char *shell = g_getenv("SHELL");
    if (!shell || shell[0] == '\0') {
        shell = "/bin/sh";
    }

    const char *argv[] = {shell, "-c", command, NULL};
    gboolean ok = detach_launch_properly(argv, command);

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
    gboolean ok = detach_launch_properly(argv, exec_path);

    if (ok) {
        log_info("Launched PATH binary: %s", exec_path);
    }

    return ok;
}

typedef const char *(*ProgramResolver)(const char *program);
typedef const char *(*DesktopTerminalGetter)(const char *desktop, ProgramResolver resolver);

static const char *resolve_via_glib(const char *program) {
    gchar *path = g_find_program_in_path(program);
    if (path) {
        g_free(path);
        return program;
    }
    return NULL;
}

// ---------------------------------------------------------------------------
// Desktop environment terminal detection helpers
// ---------------------------------------------------------------------------

// Run a shell command and return the first word of stdout, or NULL.
// Strips surrounding quotes and whitespace. Caller g_free()s.
static gchar *run_command_first_word(const char *cmd) {
    gchar *stdout_str = NULL;
    gint exit_status = 0;
    GError *err = NULL;

    if (!g_spawn_command_line_sync(cmd, &stdout_str, NULL, &exit_status, &err)) {
        g_clear_error(&err);
        return NULL;
    }
    if (exit_status != 0 || !stdout_str || stdout_str[0] == '\0') {
        g_free(stdout_str);
        return NULL;
    }

    // Trim whitespace and newlines
    g_strstrip(stdout_str);

    // Strip surrounding single or double quotes
    size_t len = strlen(stdout_str);
    if (len >= 2 &&
        ((stdout_str[0] == '\'' && stdout_str[len-1] == '\'') ||
         (stdout_str[0] == '"'  && stdout_str[len-1] == '"'))) {
        stdout_str[len-1] = '\0';
        memmove(stdout_str, stdout_str + 1, len - 1);
        len -= 2;
    }

    if (stdout_str[0] == '\0') {
        g_free(stdout_str);
        return NULL;
    }

    // If it contains spaces, take only the first word (binary name only; args ignored)
    // Rationale: we append our own -e arg; extra args from gsettings would conflict.
    char *space = strchr(stdout_str, ' ');
    if (space) {
        *space = '\0';
    }

    return stdout_str;
}

static const char *get_desktop_terminal(const char *desktop, ProgramResolver resolver) {
    if (!desktop || desktop[0] == '\0') {
        return NULL;
    }

    static gchar s_cached[256];
    const char *gsettings_cmd = NULL;

    // XDG_CURRENT_DESKTOP can be colon-separated (e.g. "GNOME:Unity")
    // Check for each known desktop prefix.
    if (strstr(desktop, "MATE")) {
        gsettings_cmd = "gsettings get org.mate.applications-terminal exec";
    } else if (strstr(desktop, "GNOME") || strstr(desktop, "Unity")) {
        gsettings_cmd = "gsettings get org.gnome.desktop.default-applications.terminal exec";
    } else if (strstr(desktop, "X-Cinnamon") || strstr(desktop, "Cinnamon")) {
        gsettings_cmd = "gsettings get org.cinnamon.desktop.default-applications.terminal exec";
    } else if (strstr(desktop, "KDE") || strstr(desktop, "plasma")) {
        // KDE doesn't use gsettings; konsole is the standard terminal
        if (resolver("konsole")) {
            return "konsole";
        }
        return NULL;
    } else if (strstr(desktop, "XFCE") || strstr(desktop, "xfce")) {
        // Try xfconf-query first, fall back to xfce4-terminal binary
        gchar *xfce_term = run_command_first_word(
            "xfconf-query -c xfce4-terminal -p /default-terminal");
        if (xfce_term && xfce_term[0] != '\0' && resolver(xfce_term)) {
            g_strlcpy(s_cached, xfce_term, sizeof(s_cached));
            g_free(xfce_term);
            return s_cached;
        }
        g_free(xfce_term);
        if (resolver("xfce4-terminal")) {
            return "xfce4-terminal";
        }
        return NULL;
    } else {
        return NULL;  // unknown desktop — fall through
    }

    gchar *term = run_command_first_word(gsettings_cmd);
    if (!term) {
        return NULL;
    }

    if (resolver(term)) {
        g_strlcpy(s_cached, term, sizeof(s_cached));
        g_free(term);
        return s_cached;
    }

    g_free(term);
    return NULL;
}

// ---------------------------------------------------------------------------
// Terminal detection chain
// ---------------------------------------------------------------------------

static const char *detect_terminal_with_resolver(ProgramResolver resolver,
                                                  DesktopTerminalGetter dt_getter) {
    // Step 1: $TERMINAL env var
    const char *env_term = g_getenv("TERMINAL");
    if (env_term && env_term[0] != '\0' && resolver(env_term)) {
        return env_term;
    }

    // Step 2: Desktop-environment configured terminal
    if (dt_getter) {
        const char *desktop = g_getenv("XDG_CURRENT_DESKTOP");
        if (desktop && desktop[0] != '\0') {
            const char *dt = dt_getter(desktop, resolver);
            if (dt && dt[0] != '\0') {
                return dt;
            }
        }
    }

    // Step 3: x-terminal-emulator + hardcoded candidate list
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

    const char *term = detect_terminal_with_resolver(resolve_via_glib, get_desktop_terminal);
    const char *argv[] = {term, "-e", exec_path, NULL};
    gboolean ok = detach_launch_properly(argv, exec_path);

    if (ok) {
        log_info("Launched PATH binary in terminal '%s': %s", term, exec_path);
    }

    return ok;
}

// ---------------------------------------------------------------------------
// Test hooks (COFI_TESTING only)
// ---------------------------------------------------------------------------

#ifdef COFI_TESTING
// Existing hook: passes NULL for dt_getter (skips desktop step) — existing tests unchanged
const char *detect_terminal_for_test(ProgramResolver resolver) {
    return detect_terminal_with_resolver(resolver, NULL);
}

// New hook: both resolver and getter injectable
const char *detect_terminal_with_desktop_for_test(ProgramResolver resolver,
                                                   DesktopTerminalGetter dt_getter) {
    return detect_terminal_with_resolver(resolver, dt_getter);
}

char **build_systemd_run_argv_for_test(const char *const *inner_argv) {
    return build_systemd_run_argv(inner_argv);
}
#endif
