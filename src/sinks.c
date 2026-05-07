#include "sinks.h"

#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

#ifndef COFI_SINKS_PARSER_TEST
#include "app_data.h"
#include "display.h"
#include "log.h"
#include "match.h"
#include "selection.h"
#include "window_lifecycle.h"
#endif

static void safe_copy(char *dest, size_t size, const char *src) {
    if (!dest || size == 0) {
        return;
    }

    if (!src) {
        dest[0] = '\0';
        return;
    }

    g_strlcpy(dest, src, size);
}

static int parse_inventory(const char *inventory,
                           const char *default_sink,
                           SinkEntry *out,
                           int max_out,
                           char *error_out,
                           size_t error_size) {
    char default_name[MAX_SINK_NAME_LEN];
    safe_copy(default_name, sizeof(default_name), default_sink);
    g_strstrip(default_name);

    if (error_out && error_size > 0) {
        error_out[0] = '\0';
    }
    if (!out || max_out <= 0) {
        return 0;
    }

    int count = 0;
    gboolean in_sink = FALSE;
    SinkEntry pending;
    memset(&pending, 0, sizeof(pending));

    gchar **lines = g_strsplit(inventory ? inventory : "", "\n", -1);

    for (int i = 0; lines[i] && count < max_out; i++) {
        char *line = lines[i];
        char *trimmed = g_strstrip(line);
        if (trimmed[0] == '\0') {
            continue;
        }

        if (g_str_has_prefix(trimmed, "Sink #")) {
            if (in_sink && pending.name[0] != '\0') {
                if (pending.description[0] == '\0') {
                    safe_copy(pending.description, sizeof(pending.description), pending.name);
                }
                out[count++] = pending;
            }
            memset(&pending, 0, sizeof(pending));
            in_sink = TRUE;
            continue;
        }

        if (!in_sink) {
            continue;
        }

        if (g_str_has_prefix(trimmed, "Name:")) {
            safe_copy(pending.name, sizeof(pending.name),
                      g_strstrip(trimmed + strlen("Name:")));
        } else if (g_str_has_prefix(trimmed, "Description:")) {
            safe_copy(pending.description, sizeof(pending.description),
                      g_strstrip(trimmed + strlen("Description:")));
        }
    }

    if (in_sink && count < max_out && pending.name[0] != '\0') {
        if (pending.description[0] == '\0') {
            safe_copy(pending.description, sizeof(pending.description), pending.name);
        }
        out[count++] = pending;
    }

    g_strfreev(lines);

    for (int i = 0; i < count; i++) {
        out[i].is_default = default_name[0] != '\0' &&
            strcmp(out[i].name, default_name) == 0;
        if (!out[i].is_default) {
            continue;
        }

        SinkEntry default_sink_entry = out[i];
        memmove(&out[i], &out[i + 1], sizeof(SinkEntry) * (count - i - 1));
        out[count - 1] = default_sink_entry;
        break;
    }

    if (count == 0 && error_out && error_size > 0) {
        g_snprintf(error_out, error_size, "No sinks found");
    }

    return count;
}

static int preferred_filtered_index(const SinkEntry *sinks,
                                    int sink_count,
                                    const int *filtered_indices,
                                    int filtered_count) {
    if (!sinks || !filtered_indices || filtered_count <= 0) {
        return 0;
    }

    for (int i = 0; i < filtered_count; i++) {
        int raw_index = filtered_indices[i];
        if (raw_index >= 0 && raw_index < sink_count &&
            sinks[raw_index].is_default) {
            return i;
        }
    }

    return filtered_count - 1;
}

static void build_snapshot(const SinkEntry *sinks,
                           int count,
                           char *out,
                           size_t out_size) {
    if (!out || out_size == 0) {
        return;
    }

    out[0] = '\0';
    for (int i = 0; sinks && i < count; i++) {
        g_strlcat(out, sinks[i].name, out_size);
        g_strlcat(out, sinks[i].is_default ? "|1\n" : "|0\n", out_size);
    }
}

#ifdef COFI_TESTING
int sinks_parse_inventory_test_hook(const char *inventory,
                                    const char *default_sink,
                                    SinkEntry *out,
                                    int max_out,
                                    char *error_out,
                                    size_t error_size) {
    return parse_inventory(inventory, default_sink, out, max_out,
                           error_out, error_size);
}

void sinks_snapshot_test_hook(const SinkEntry *sinks,
                              int count,
                              char *out,
                              size_t out_size) {
    build_snapshot(sinks, count, out, out_size);
}

int sinks_preferred_filtered_index_test_hook(const SinkEntry *sinks,
                                             int sink_count,
                                             const int *filtered_indices,
                                             int filtered_count) {
    return preferred_filtered_index(sinks, sink_count, filtered_indices,
                                    filtered_count);
}
#endif

#ifndef COFI_SINKS_PARSER_TEST
static void set_error(SinksMode *mode, const char *message) {
    if (!mode) {
        return;
    }

    safe_copy(mode->last_error, sizeof(mode->last_error), message);
}

void sinks_filter(AppData *app, const char *filter) {
    if (!app) {
        return;
    }

    SinksMode *mode = &app->sinks_mode;
    mode->filtered_count = 0;

    for (int i = 0; i < mode->sink_count; i++) {
        if (mode->filtered_count >= MAX_SINKS) {
            break;
        }

        if (!filter || filter[0] == '\0') {
            mode->filtered_indices[mode->filtered_count++] = i;
            continue;
        }

        char searchable[MAX_SINK_DESC_LEN + MAX_SINK_NAME_LEN + 2];
        g_snprintf(searchable, sizeof(searchable), "%s %s",
                   mode->sinks[i].description, mode->sinks[i].name);
        if (has_match(filter, searchable)) {
            mode->filtered_indices[mode->filtered_count++] = i;
        }
    }
}

static void apply_refresh_result(AppData *app,
                                 const char *default_sink,
                                 const char *inventory) {
    SinksMode *mode = &app->sinks_mode;
    SinkEntry parsed[MAX_SINKS];
    char parse_error[256];
    char selected_name[MAX_SINK_NAME_LEN];
    char snapshot[sizeof(mode->snapshot)];

    selected_name[0] = '\0';
    if (app->selection.provider_index >= 0 &&
        app->selection.provider_index < mode->filtered_count) {
        int selected_raw = mode->filtered_indices[app->selection.provider_index];
        if (selected_raw >= 0 && selected_raw < mode->sink_count) {
            safe_copy(selected_name, sizeof(selected_name),
                      mode->sinks[selected_raw].name);
        }
    }

    int count = parse_inventory(inventory, default_sink, parsed, MAX_SINKS,
                                parse_error, sizeof(parse_error));
    build_snapshot(parsed, count, snapshot, sizeof(snapshot));

    if (count > 0 && mode->last_error[0] == '\0' &&
        strcmp(mode->snapshot, snapshot) == 0) {
        return;
    }

    mode->sink_count = count;
    if (count > 0) {
        memcpy(mode->sinks, parsed, sizeof(SinkEntry) * count);
        safe_copy(mode->snapshot, sizeof(mode->snapshot), snapshot);
        mode->last_error[0] = '\0';
    } else {
        mode->snapshot[0] = '\0';
        set_error(mode, parse_error[0] ? parse_error : "No sinks found");
    }

    sinks_filter(app, gtk_entry_get_text(GTK_ENTRY(app->entry)));
    app->selection.provider_index =
        preferred_filtered_index(mode->sinks, mode->sink_count,
                                 mode->filtered_indices, mode->filtered_count);
    app->selection.provider_scroll_offset = 0;
    if (selected_name[0] != '\0') {
        for (int i = 0; i < mode->filtered_count; i++) {
            int raw_index = mode->filtered_indices[i];
            if (strcmp(mode->sinks[raw_index].name, selected_name) == 0) {
                app->selection.provider_index = i;
                break;
            }
        }
    }
    update_scroll_position(app);
    update_display(app);
}

static void on_refresh_done(GObject *source, GAsyncResult *result,
                            gpointer user_data) {
    AppData *app = (AppData *)user_data;
    GSubprocess *process = G_SUBPROCESS(source);
    g_autofree char *stdout_text = NULL;
    g_autofree char *stderr_text = NULL;
    g_autoptr(GError) error = NULL;

    app->sinks_mode.refresh_in_flight = FALSE;
    if (!g_subprocess_communicate_utf8_finish(process, result,
                                              &stdout_text, &stderr_text,
                                              &error)) {
        set_error(&app->sinks_mode, error ? error->message : "pactl refresh failed");
        update_display(app);
        return;
    }

    if (!g_subprocess_get_successful(process)) {
        set_error(&app->sinks_mode,
                  stderr_text && stderr_text[0] ? stderr_text : "pactl refresh failed");
        update_display(app);
        return;
    }

    char *separator = strstr(stdout_text ? stdout_text : "", "\n__COFI_SINKS__\n");
    if (!separator) {
        set_error(&app->sinks_mode, "Malformed pactl refresh output");
        update_display(app);
        return;
    }

    *separator = '\0';
    char *default_sink = g_strstrip(stdout_text);
    char *inventory = separator + strlen("\n__COFI_SINKS__\n");
    apply_refresh_result(app, default_sink, inventory);
}

void sinks_refresh_async(AppData *app) {
    if (!app || app->sinks_mode.refresh_in_flight) {
        return;
    }

    static const char *script =
        "pactl get-default-sink && printf '\\n__COFI_SINKS__\\n' && pactl list sinks";

    g_autoptr(GError) error = NULL;
    GSubprocess *process = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                            G_SUBPROCESS_FLAGS_STDERR_PIPE,
                                            &error, "sh", "-c", script, NULL);
    if (!process) {
        set_error(&app->sinks_mode, error ? error->message : "Unable to spawn pactl");
        update_display(app);
        return;
    }

    app->sinks_mode.refresh_in_flight = TRUE;
    g_subprocess_communicate_utf8_async(process, NULL, NULL,
                                        on_refresh_done, app);
    g_object_unref(process);
}

static void on_set_default_done(GObject *source, GAsyncResult *result,
                                gpointer user_data) {
    AppData *app = (AppData *)user_data;
    GSubprocess *process = G_SUBPROCESS(source);
    g_autofree char *stderr_text = NULL;
    g_autoptr(GError) error = NULL;

    if (!g_subprocess_communicate_utf8_finish(process, result, NULL,
                                              &stderr_text, &error)) {
        set_error(&app->sinks_mode, error ? error->message : "pactl set-default-sink failed");
        update_display(app);
        return;
    }

    if (!g_subprocess_get_successful(process)) {
        set_error(&app->sinks_mode,
                  stderr_text && stderr_text[0] ? stderr_text : "pactl set-default-sink failed");
        update_display(app);
        return;
    }

    sinks_refresh_async(app);
    hide_window(app);
}

gboolean sinks_switch_name(AppData *app, const char *sink_name) {
    if (!app || !sink_name || sink_name[0] == '\0') {
        return FALSE;
    }

    g_autoptr(GError) error = NULL;
    GSubprocess *process = g_subprocess_new(G_SUBPROCESS_FLAGS_STDERR_PIPE,
                                            &error, "pactl",
                                            "set-default-sink", sink_name,
                                            NULL);
    if (!process) {
        set_error(&app->sinks_mode, error ? error->message : "Unable to spawn pactl");
        update_display(app);
        return FALSE;
    }

    g_subprocess_communicate_utf8_async(process, NULL, NULL,
                                        on_set_default_done, app);
    g_object_unref(process);
    return TRUE;
}

#endif
