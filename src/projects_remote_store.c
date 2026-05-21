#include "projects_remote_store.h"

#include <errno.h>
#include <json-glib/json-glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "log.h"

static ProjectRemoteEntry *s_entries = NULL;
static int s_entry_count = 0;
static int s_entry_capacity = 0;
static char s_store_path[512];
static gboolean s_initialized = FALSE;

static const char *default_projects_path(void) {
    static char path[512];
    const char *home = g_get_home_dir();
    if (!home || home[0] == '\0') {
        home = ".";
    }
    g_snprintf(path, sizeof(path), "%s/.config", home);
    mkdir(path, 0755);
    g_snprintf(path, sizeof(path), "%s/.config/cofi", home);
    mkdir(path, 0755);
    g_snprintf(path, sizeof(path), "%s/.config/cofi/projects.json", home);
    return path;
}

static void clear_entries(void) {
    g_free(s_entries);
    s_entries = NULL;
    s_entry_count = 0;
    s_entry_capacity = 0;
}

static gboolean ensure_capacity(int target) {
    if (target <= s_entry_capacity) return TRUE;
    int next = s_entry_capacity > 0 ? s_entry_capacity * 2 : 16;
    while (next < target) next *= 2;
    ProjectRemoteEntry *grown = g_realloc(s_entries, (size_t)next * sizeof(ProjectRemoteEntry));
    if (!grown) return FALSE;
    s_entries = grown;
    s_entry_capacity = next;
    return TRUE;
}

static gboolean append_entry(const ProjectRemoteEntry *entry) {
    if (!entry || !entry->host[0] || !entry->name[0]) return FALSE;
    if (!ensure_capacity(s_entry_count + 1)) return FALSE;
    s_entries[s_entry_count++] = *entry;
    return TRUE;
}

void projects_remote_store_init(void) {
    if (s_initialized) return;
    g_strlcpy(s_store_path, default_projects_path(), sizeof(s_store_path));
    s_initialized = TRUE;
}

static gboolean save_store(void) {
    projects_remote_store_init();

    JsonBuilder *builder = json_builder_new();
    json_builder_begin_array(builder);
    for (int i = 0; i < s_entry_count; i++) {
        const ProjectRemoteEntry *entry = &s_entries[i];
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "host");
        json_builder_add_string_value(builder, entry->host);
        json_builder_set_member_name(builder, "tool");
        json_builder_add_string_value(builder,
                                      entry->backend == PROJECT_BACKEND_ZELLIJ ? "zellij" : "tmux");
        json_builder_set_member_name(builder, "name");
        json_builder_add_string_value(builder, entry->name);
        if (entry->cwd[0] != '\0') {
            json_builder_set_member_name(builder, "cwd");
            json_builder_add_string_value(builder, entry->cwd);
        }
        json_builder_end_object(builder);
    }
    json_builder_end_array(builder);

    JsonNode *root = json_builder_get_root(builder);
    JsonGenerator *generator = json_generator_new();
    json_generator_set_root(generator, root);
    json_generator_set_pretty(generator, TRUE);

    GError *error = NULL;
    gboolean ok = json_generator_to_file(generator, s_store_path, &error);
    if (!ok) {
        log_error("Failed to save projects remote store: %s",
                  error ? error->message : "unknown error");
    }
    g_clear_error(&error);
    g_object_unref(generator);
    json_node_free(root);
    g_object_unref(builder);
    return ok;
}

static gboolean parse_tool_backend(JsonObject *obj, ProjectBackend *backend_out) {
    if (!obj || !backend_out || !json_object_has_member(obj, "tool")) return FALSE;
    const char *tool = json_object_get_string_member(obj, "tool");
    if (!tool) return FALSE;
    if (strcmp(tool, "tmux") == 0) {
        *backend_out = PROJECT_BACKEND_TMUX;
        return TRUE;
    }
    if (strcmp(tool, "zellij") == 0) {
        *backend_out = PROJECT_BACKEND_ZELLIJ;
        return TRUE;
    }
    return FALSE;
}

gboolean projects_remote_store_reload(void) {
    projects_remote_store_init();
    clear_entries();

    if (!g_file_test(s_store_path, G_FILE_TEST_EXISTS)) {
        return TRUE;
    }

    JsonParser *parser = json_parser_new();
    GError *error = NULL;
    gboolean ok = json_parser_load_from_file(parser, s_store_path, &error);
    if (!ok) {
        log_warn("Failed to parse projects remote store '%s': %s",
                 s_store_path, error ? error->message : "unknown error");
        g_clear_error(&error);
        g_object_unref(parser);
        return FALSE;
    }

    JsonNode *root = json_parser_get_root(parser);
    if (!root || json_node_get_node_type(root) != JSON_NODE_ARRAY) {
        g_object_unref(parser);
        return TRUE;
    }

    JsonArray *array = json_node_get_array(root);
    guint len = json_array_get_length(array);
    for (guint i = 0; i < len; i++) {
        JsonNode *item = json_array_get_element(array, i);
        if (!item || json_node_get_node_type(item) != JSON_NODE_OBJECT) continue;
        JsonObject *obj = json_node_get_object(item);
        if (!json_object_has_member(obj, "host") ||
            !json_object_has_member(obj, "name")) {
            continue;
        }

        const char *host = json_object_get_string_member(obj, "host");
        const char *name = json_object_get_string_member(obj, "name");
        if (!host || !host[0] || !name || !name[0]) continue;

        ProjectRemoteEntry entry;
        memset(&entry, 0, sizeof(entry));
        g_strlcpy(entry.host, host, sizeof(entry.host));
        g_strlcpy(entry.name, name, sizeof(entry.name));
        if (!parse_tool_backend(obj, &entry.backend)) continue;
        if (json_object_has_member(obj, "cwd")) {
            const char *cwd = json_object_get_string_member(obj, "cwd");
            if (cwd) g_strlcpy(entry.cwd, cwd, sizeof(entry.cwd));
        }
        append_entry(&entry);
    }

    g_object_unref(parser);
    return TRUE;
}

int projects_remote_store_count(void) {
    return s_entry_count;
}

const ProjectRemoteEntry *projects_remote_store_entry_at(int index) {
    if (index < 0 || index >= s_entry_count) return NULL;
    return &s_entries[index];
}

int projects_remote_store_append_sessions(ProjectSessionEntry *out, int start, int max_out) {
    if (!out || start < 0 || start >= max_out) return start;
    int cursor = start;
    for (int i = 0; i < s_entry_count && cursor < max_out; i++) {
        const ProjectRemoteEntry *entry = &s_entries[i];
        ProjectSessionEntry *session = &out[cursor];
        memset(session, 0, sizeof(*session));
        session->backend = entry->backend;
        session->windows = -1;
        session->attached = -1;
        session->is_saved_remote = TRUE;
        g_strlcpy(session->name, entry->name, sizeof(session->name));
        g_strlcpy(session->remote_host, entry->host, sizeof(session->remote_host));
        g_strlcpy(session->remote_cwd, entry->cwd, sizeof(session->remote_cwd));
        cursor++;
    }
    return cursor;
}

gboolean projects_remote_store_forget(const char *host,
                                      ProjectBackend backend,
                                      const char *name,
                                      const char *cwd) {
    if (!host || !host[0] || !name || !name[0]) return FALSE;

    for (int i = 0; i < s_entry_count; i++) {
        ProjectRemoteEntry *entry = &s_entries[i];
        if (entry->backend != backend) continue;
        if (strcmp(entry->host, host) != 0) continue;
        if (strcmp(entry->name, name) != 0) continue;
        if (g_strcmp0(entry->cwd, cwd ? cwd : "") != 0) continue;

        if (i + 1 < s_entry_count) {
            memmove(entry, entry + 1, (size_t)(s_entry_count - i - 1) * sizeof(ProjectRemoteEntry));
        }
        s_entry_count--;
        return save_store();
    }
    return FALSE;
}

gboolean projects_remote_store_save_intent(const char *host,
                                           ProjectBackend backend,
                                           const char *name,
                                           const char *cwd) {
    if (!host || !host[0] || !name || !name[0]) return FALSE;

    const char *target_cwd = cwd ? cwd : "";
    for (int i = 0; i < s_entry_count; i++) {
        ProjectRemoteEntry *entry = &s_entries[i];
        if (entry->backend == backend &&
            strcmp(entry->host, host) == 0 &&
            strcmp(entry->name, name) == 0) {
            g_strlcpy(entry->cwd, target_cwd, sizeof(entry->cwd));
            return save_store();
        }
    }

    ProjectRemoteEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.backend = backend;
    g_strlcpy(entry.host, host, sizeof(entry.host));
    g_strlcpy(entry.name, name, sizeof(entry.name));
    g_strlcpy(entry.cwd, target_cwd, sizeof(entry.cwd));
    if (!append_entry(&entry)) return FALSE;
    return save_store();
}

#ifdef COFI_TESTING
void projects_remote_store_set_path_for_test(const char *path) {
    projects_remote_store_init();
    if (path && path[0] != '\0') {
        g_strlcpy(s_store_path, path, sizeof(s_store_path));
    }
}

void projects_remote_store_reset_for_test(void) {
    clear_entries();
    s_store_path[0] = '\0';
    s_initialized = FALSE;
}

gboolean projects_remote_store_add_for_test(const char *host,
                                            ProjectBackend backend,
                                            const char *name,
                                            const char *cwd) {
    if (!host || !host[0] || !name || !name[0]) return FALSE;
    ProjectRemoteEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.backend = backend;
    g_strlcpy(entry.host, host, sizeof(entry.host));
    g_strlcpy(entry.name, name, sizeof(entry.name));
    if (cwd && cwd[0]) g_strlcpy(entry.cwd, cwd, sizeof(entry.cwd));
    return append_entry(&entry);
}

gboolean projects_remote_store_save_for_test(void) {
    return save_store();
}
#endif
