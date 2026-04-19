#include "system_actions.h"
#include "log.h"

#include <gio/gio.h>
#include <string.h>
#include <unistd.h>

#define LOGIN1_BUS_NAME "org.freedesktop.login1"
#define LOGIN1_MANAGER_PATH "/org/freedesktop/login1"
#define LOGIN1_MANAGER_IFACE "org.freedesktop.login1.Manager"
#define LOGIN1_SESSION_IFACE "org.freedesktop.login1.Session"
#define LOGIN1_SESSION_AUTO_PATH "/org/freedesktop/login1/session/auto"

typedef struct {
    const char *name;
    const char *keywords;
    SystemActionId action_id;
} SystemActionDef;

static const SystemActionDef SYSTEM_ACTIONS[] = {
    {"Lock", "screen lock session", SYSTEM_ACTION_LOCK},
    {"Suspend", "sleep standby", SYSTEM_ACTION_SUSPEND},
    {"Hibernate", "deep sleep", SYSTEM_ACTION_HIBERNATE},
    {"Logout", "sign out session end", SYSTEM_ACTION_LOGOUT},
    {"Reboot", "restart", SYSTEM_ACTION_REBOOT},
    {"Shutdown", "power off poweroff halt turn off", SYSTEM_ACTION_SHUTDOWN},
};

static int total_system_actions(void) {
    return (int)(sizeof(SYSTEM_ACTIONS) / sizeof(SYSTEM_ACTIONS[0]));
}

static gboolean invoke_manager_bool_method(const char *method) {
    GError *error = NULL;
    GDBusProxy *proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SYSTEM,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        LOGIN1_BUS_NAME,
        LOGIN1_MANAGER_PATH,
        LOGIN1_MANAGER_IFACE,
        NULL,
        &error);

    if (!proxy) {
        log_error("System action: failed to create logind manager proxy for %s: %s",
                  method, error ? error->message : "unknown error");
        if (error) {
            g_error_free(error);
        }
        return FALSE;
    }

    GVariant *reply = g_dbus_proxy_call_sync(
        proxy,
        method,
        g_variant_new("(b)", TRUE),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error);

    g_object_unref(proxy);

    if (!reply) {
        log_error("System action: logind call %s failed: %s",
                  method, error ? error->message : "unknown error");
        if (error) {
            g_error_free(error);
        }
        return FALSE;
    }

    g_variant_unref(reply);
    return TRUE;
}

static gboolean spawn_lock_command(const char *const *argv, const char *label) {
    GError *error = NULL;
    GPid pid = 0;
    gint stdin_fd = -1;
    gint stdout_fd = -1;
    gint stderr_fd = -1;

    gchar *spawn_argv[8] = {0};
    int i = 0;
    while (argv[i] && i < 7) {
        spawn_argv[i] = (gchar *)argv[i];
        i++;
    }
    spawn_argv[i] = NULL;

    gboolean ok = g_spawn_async_with_pipes(
        NULL,
        spawn_argv,
        NULL,
        G_SPAWN_SEARCH_PATH,
        NULL,
        NULL,
        &pid,
        &stdin_fd,
        &stdout_fd,
        &stderr_fd,
        &error);

    if (!ok) {
        log_debug("System action lock: command '%s' unavailable/failed: %s",
                  label,
                  error ? error->message : "unknown error");
        if (error) {
            g_error_free(error);
        }
        return FALSE;
    }

    if (stdin_fd >= 0) {
        close(stdin_fd);
    }
    if (stdout_fd >= 0) {
        close(stdout_fd);
    }
    if (stderr_fd >= 0) {
        close(stderr_fd);
    }
    g_spawn_close_pid(pid);

    log_info("System action lock: using command '%s'", label);
    return TRUE;
}

static gboolean invoke_lock_session(void) {
    static const char *const cmd_xdg[] = {"xdg-screensaver", "lock", NULL};
    static const char *const cmd_mate[] = {"mate-screensaver-command", "--lock", NULL};
    static const char *const cmd_xscreen[] = {"xscreensaver-command", "-lock", NULL};
    static const char *const cmd_loginctl[] = {"loginctl", "lock-session", NULL};

    if (spawn_lock_command(cmd_xdg, "xdg-screensaver lock")) {
        return TRUE;
    }
    if (spawn_lock_command(cmd_mate, "mate-screensaver-command --lock")) {
        return TRUE;
    }
    if (spawn_lock_command(cmd_xscreen, "xscreensaver-command -lock")) {
        return TRUE;
    }
    if (spawn_lock_command(cmd_loginctl, "loginctl lock-session")) {
        return TRUE;
    }

    GError *error = NULL;
    GDBusProxy *proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SYSTEM,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        LOGIN1_BUS_NAME,
        LOGIN1_SESSION_AUTO_PATH,
        LOGIN1_SESSION_IFACE,
        NULL,
        &error);

    if (!proxy) {
        if (error) {
            g_error_free(error);
            error = NULL;
        }

        const char *session_id = g_getenv("XDG_SESSION_ID");
        if (!session_id || !*session_id) {
            log_error("System action: lock failed (session/auto unavailable and XDG_SESSION_ID missing)");
            return FALSE;
        }

        char session_path[128];
        g_snprintf(session_path, sizeof(session_path), "/org/freedesktop/login1/session/%s", session_id);

        proxy = g_dbus_proxy_new_for_bus_sync(
            G_BUS_TYPE_SYSTEM,
            G_DBUS_PROXY_FLAGS_NONE,
            NULL,
            LOGIN1_BUS_NAME,
            session_path,
            LOGIN1_SESSION_IFACE,
            NULL,
            &error);

        if (!proxy) {
            log_error("System action: failed to create lock session proxy at %s: %s",
                      session_path, error ? error->message : "unknown error");
            if (error) {
                g_error_free(error);
            }
            return FALSE;
        }
    }

    GVariant *reply = g_dbus_proxy_call_sync(
        proxy,
        "Lock",
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error);

    g_object_unref(proxy);

    if (!reply) {
        log_error("System action: logind call Lock failed: %s",
                  error ? error->message : "unknown error");
        if (error) {
            g_error_free(error);
        }
        return FALSE;
    }

    log_info("System action lock: using logind Session.Lock fallback");
    g_variant_unref(reply);
    return TRUE;
}

static gboolean invoke_logout_session(void) {
    const char *session_id = g_getenv("XDG_SESSION_ID");
    GError *error = NULL;

    if (!session_id || !*session_id) {
        log_error("System action: logout failed (XDG_SESSION_ID missing)");
        return FALSE;
    }

    GDBusProxy *proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SYSTEM,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        LOGIN1_BUS_NAME,
        LOGIN1_MANAGER_PATH,
        LOGIN1_MANAGER_IFACE,
        NULL,
        &error);

    if (!proxy) {
        log_error("System action: failed to create logind manager proxy for logout: %s",
                  error ? error->message : "unknown error");
        if (error) {
            g_error_free(error);
        }
        return FALSE;
    }

    GVariant *reply = g_dbus_proxy_call_sync(
        proxy,
        "TerminateSession",
        g_variant_new("(s)", session_id),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error);

    g_object_unref(proxy);

    if (!reply) {
        log_error("System action: logind call TerminateSession failed: %s",
                  error ? error->message : "unknown error");
        if (error) {
            g_error_free(error);
        }
        return FALSE;
    }

    g_variant_unref(reply);
    return TRUE;
}

void system_actions_load(AppEntry *out, int *count, int max) {
    int loaded = 0;

    if (count) {
        *count = 0;
    }

    if (!out || !count || max <= 0) {
        return;
    }

    const int available = total_system_actions();
    const int limit = (max < available) ? max : available;

    for (int i = 0; i < limit; i++) {
        const SystemActionDef *def = &SYSTEM_ACTIONS[i];
        AppEntry *entry = &out[loaded];

        memset(entry, 0, sizeof(*entry));
        g_strlcpy(entry->name, def->name, sizeof(entry->name));
        entry->generic_name[0] = '\0';
        g_strlcpy(entry->keywords, def->keywords, sizeof(entry->keywords));
        entry->source_kind = APP_SOURCE_SYSTEM;
        entry->action_id = def->action_id;
        entry->info = NULL;

        loaded++;
    }

    *count = loaded;
}

void system_actions_invoke(const AppEntry *entry) {
    const gint64 start_us = g_get_monotonic_time();
    const gint64 dbus_start_us = g_get_monotonic_time();
    gboolean ok = FALSE;

    if (!entry || entry->source_kind != APP_SOURCE_SYSTEM) {
        return;
    }

    switch (entry->action_id) {
        case SYSTEM_ACTION_LOCK:
            ok = invoke_lock_session();
            break;
        case SYSTEM_ACTION_SUSPEND:
            ok = invoke_manager_bool_method("Suspend");
            break;
        case SYSTEM_ACTION_HIBERNATE:
            ok = invoke_manager_bool_method("Hibernate");
            break;
        case SYSTEM_ACTION_LOGOUT:
            ok = invoke_logout_session();
            break;
        case SYSTEM_ACTION_REBOOT:
            ok = invoke_manager_bool_method("Reboot");
            break;
        case SYSTEM_ACTION_SHUTDOWN:
            ok = invoke_manager_bool_method("PowerOff");
            break;
        default:
            log_warn("System action: unknown action id %d for '%s'",
                     (int)entry->action_id,
                     entry->name);
            return;
    }

    const double dbus_ms = (double)(g_get_monotonic_time() - dbus_start_us) / 1000.0;
    const double total_ms = (double)(g_get_monotonic_time() - start_us) / 1000.0;
    log_debug("System action '%s' invoked in %.2fms (dbus=%.2fms, ok=%d)",
              entry->name, total_ms, dbus_ms, ok ? 1 : 0);
}
