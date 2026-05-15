#include "sessions_parse.h"

#include <stdlib.h>
#include <string.h>

static gboolean parse_int_field(const char *text, int *out) {
    if (!text || !out || text[0] == '\0') return FALSE;
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (*end != '\0' || value < 0 || value > 9999) return FALSE;
    *out = (int)value;
    return TRUE;
}

static void copy_field(char *dest, size_t dest_size, const char *start, size_t len) {
    if (!dest || dest_size == 0) return;
    if (len >= dest_size) len = dest_size - 1;
    memcpy(dest, start, len);
    dest[len] = '\0';
}

int sessions_parse_tmux_list(const char *output,
                             SessionEntry *out,
                             int max_out,
                             char *error_out,
                             size_t error_size) {
    if (error_out && error_size > 0) error_out[0] = '\0';
    if (!out || max_out <= 0) return 0;
    if (!output || output[0] == '\0') {
        if (error_out && error_size > 0)
            g_strlcpy(error_out, "No tmux sessions", error_size);
        return 0;
    }

    int count = 0;
    const char *line = output;
    while (*line && count < max_out) {
        const char *line_end = strchr(line, '\n');
        if (!line_end) line_end = line + strlen(line);
        if (line_end > line) {
            const char *tab1 = memchr(line, '\t', (size_t)(line_end - line));
            const char *tab2 = tab1 ? memchr(tab1 + 1, '\t',
                                             (size_t)(line_end - tab1 - 1)) : NULL;
            if (tab1 && tab2) {
                int windows = 0;
                int attached = 0;
                char windows_buf[32];
                char attached_buf[32];
                copy_field(windows_buf, sizeof(windows_buf), tab1 + 1,
                           (size_t)(tab2 - tab1 - 1));
                copy_field(attached_buf, sizeof(attached_buf), tab2 + 1,
                           (size_t)(line_end - tab2 - 1));
                if (tab1 > line &&
                    parse_int_field(windows_buf, &windows) &&
                    parse_int_field(attached_buf, &attached)) {
                    copy_field(out[count].name, sizeof(out[count].name), line,
                               (size_t)(tab1 - line));
                    out[count].backend = SESSION_BACKEND_TMUX;
                    out[count].windows = windows;
                    out[count].attached = attached;
                    count++;
                }
            }
        }
        line = (*line_end == '\n') ? line_end + 1 : line_end;
    }

    if (count == 0 && error_out && error_size > 0) {
        g_strlcpy(error_out, "No tmux sessions", error_size);
    }
    return count;
}

int sessions_parse_zellij_list(const char *output,
                               SessionEntry *out,
                               int max_out,
                               char *error_out,
                               size_t error_size) {
    if (error_out && error_size > 0) error_out[0] = '\0';
    if (!out || max_out <= 0) return 0;
    if (!output || output[0] == '\0') {
        if (error_out && error_size > 0)
            g_strlcpy(error_out, "No zellij sessions", error_size);
        return 0;
    }

    int count = 0;
    const char *line = output;
    while (*line && count < max_out) {
        const char *line_end = strchr(line, '\n');
        if (!line_end) line_end = line + strlen(line);
        if (line_end > line) {
            copy_field(out[count].name, sizeof(out[count].name), line,
                       (size_t)(line_end - line));
            if (out[count].name[0] != '\0') {
                out[count].backend = SESSION_BACKEND_ZELLIJ;
                out[count].windows = -1;
                out[count].attached = -1;
                count++;
            }
        }
        line = (*line_end == '\n') ? line_end + 1 : line_end;
    }

    if (count == 0 && error_out && error_size > 0) {
        g_strlcpy(error_out, "No zellij sessions", error_size);
    }
    return count;
}

static const char *path_basename(const char *path) {
    if (!path || path[0] == '\0') return "";
    const char *end = path + strlen(path);
    while (end > path && *(end - 1) == '/') end--;
    if (end == path) return path;

    const char *slash = end;
    while (slash > path && *(slash - 1) != '/') slash--;
    return slash;
}

static gchar *folder_label_from_path(const char *path) {
    const char *base = path_basename(path);
    size_t len = strlen(base);
    while (len > 0 && base[len - 1] == '/') len--;
    return len > 0 ? g_strndup(base, len) : g_strdup("zoxide");
}

void sessions_clear_folders(SessionFolder *folders, int count) {
    if (!folders || count <= 0) return;
    for (int i = 0; i < count; i++) {
        g_clear_pointer(&folders[i].path, g_free);
        g_clear_pointer(&folders[i].label, g_free);
    }
}

int sessions_parse_zoxide_list(const char *output,
                               SessionFolder *out,
                               int max_out,
                               char *error_out,
                               size_t error_size) {
    if (error_out && error_size > 0) error_out[0] = '\0';
    if (!out || max_out <= 0) return 0;
    if (!output || output[0] == '\0') {
        if (error_out && error_size > 0)
            g_strlcpy(error_out, "No zoxide folders", error_size);
        return 0;
    }

    int count = 0;
    const char *line = output;
    while (*line && count < max_out) {
        const char *line_end = strchr(line, '\n');
        if (!line_end) line_end = line + strlen(line);
        if (line_end > line) {
            out[count].path = g_strndup(line, (size_t)(line_end - line));
            out[count].label = folder_label_from_path(out[count].path);
            if (out[count].path[0] != '\0' && out[count].label[0] != '\0') {
                count++;
            } else {
                g_clear_pointer(&out[count].path, g_free);
                g_clear_pointer(&out[count].label, g_free);
            }
        }
        line = (*line_end == '\n') ? line_end + 1 : line_end;
    }

    if (count == 0 && error_out && error_size > 0) {
        g_strlcpy(error_out, "No zoxide folders", error_size);
    }
    return count;
}

gchar *sessions_build_folder_session_name(const char *path) {
    gchar *label = folder_label_from_path(path);

    GString *name = g_string_new(NULL);
    for (const char *p = label; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-') {
            g_string_append_c(name, (char)c);
        } else {
            g_string_append_c(name, '_');
        }
    }
    if (name->len == 0) {
        g_string_append(name, "zoxide");
    }
    g_free(label);
    return g_string_free(name, FALSE);
}

gchar *sessions_build_session_slot_payload(SessionBackend backend, const char *name) {
    if (!name || name[0] == '\0') return NULL;
    return g_strdup_printf("session:%s:%s",
                           backend == SESSION_BACKEND_ZELLIJ ? "zellij" : "tmux",
                           name);
}

gchar *sessions_build_folder_slot_payload(const char *path) {
    if (!path || path[0] == '\0') return NULL;
    return g_strdup_printf("folder:%s", path);
}

gboolean sessions_parse_slot_payload(const char *payload, SessionSlotTarget *out) {
    if (out) {
        memset(out, 0, sizeof(*out));
        out->kind = SESSION_SLOT_INVALID;
    }
    if (!payload || payload[0] == '\0' || !out) return FALSE;

    const char *tmux_prefix = "session:tmux:";
    const char *zellij_prefix = "session:zellij:";
    const char *folder_prefix = "folder:";

    if (g_str_has_prefix(payload, tmux_prefix)) {
        out->kind = SESSION_SLOT_SESSION;
        out->backend = SESSION_BACKEND_TMUX;
        out->value = payload + strlen(tmux_prefix);
    } else if (g_str_has_prefix(payload, zellij_prefix)) {
        out->kind = SESSION_SLOT_SESSION;
        out->backend = SESSION_BACKEND_ZELLIJ;
        out->value = payload + strlen(zellij_prefix);
    } else if (g_str_has_prefix(payload, folder_prefix)) {
        out->kind = SESSION_SLOT_FOLDER;
        out->backend = SESSION_BACKEND_TMUX;
        out->value = payload + strlen(folder_prefix);
    } else {
        return FALSE;
    }

    if (!out->value || out->value[0] == '\0') {
        out->kind = SESSION_SLOT_INVALID;
        return FALSE;
    }
    return TRUE;
}

const char *sessions_session_marker(SessionBackend backend) {
    return backend == SESSION_BACKEND_ZELLIJ ? "[z]" : "[t]";
}

const char *sessions_folder_marker(void) {
    return "[d]";
}

void sessions_format_session_match_text(const SessionEntry *session,
                                        char *out,
                                        size_t out_size) {
    if (!out || out_size == 0) return;
    if (!session) {
        out[0] = '\0';
        return;
    }
    if (session->backend == SESSION_BACKEND_ZELLIJ) {
        g_snprintf(out, out_size, "%s %s", sessions_session_marker(session->backend),
                   session->name);
        return;
    }
    g_snprintf(out, out_size, "%s %s %d %s %d %s",
               sessions_session_marker(session->backend),
               session->name,
               session->windows,
               session->windows == 1 ? "win" : "wins",
               session->attached,
               session->attached == 1 ? "client" : "clients");
}

void sessions_format_folder_match_text(const SessionFolder *folder,
                                       char *out,
                                       size_t out_size) {
    if (!out || out_size == 0) return;
    if (!folder) {
        out[0] = '\0';
        return;
    }
    g_snprintf(out, out_size, "%s %s %s", sessions_folder_marker(),
               folder->label ? folder->label : "",
               folder->path ? folder->path : "");
}
