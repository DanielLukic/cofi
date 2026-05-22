#include "layout_store.h"

#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "log.h"

static const char *default_layout_store_path(void) {
    static char path[512];
    const char *home = getenv("HOME");
    if (!home) {
        home = ".";
    }

    g_snprintf(path, sizeof(path), "%s/.config", home);
    mkdir(path, 0755);
    g_snprintf(path, sizeof(path), "%s/.config/cofi", home);
    mkdir(path, 0755);
    g_snprintf(path, sizeof(path), "%s/.config/cofi/layouts.json", home);
    return path;
}

void layout_store_init(LayoutStore *store) {
    if (!store) {
        return;
    }

    memset(store, 0, sizeof(*store));
    g_strlcpy(store->path, default_layout_store_path(), sizeof(store->path));
}

void layout_store_init_with_path(LayoutStore *store, const char *path) {
    layout_store_init(store);
    if (store && path && path[0] != '\0') {
        g_strlcpy(store->path, path, sizeof(store->path));
    }
}

static int layout_store_find_index(const LayoutStore *store, int match_id) {
    if (!store || match_id <= 0) {
        return -1;
    }

    for (int i = 0; i < store->count; i++) {
        if (store->records[i].match_id == match_id) {
            return i;
        }
    }

    return -1;
}

bool layout_store_set(LayoutStore *store, int match_id,
                      int x, int y, int width, int height, int desktop,
                      bool maximized_vert, bool maximized_horz, bool fullscreen) {
    if (!store || match_id <= 0 || width <= 0 || height <= 0) {
        return false;
    }

    int idx = layout_store_find_index(store, match_id);
    if (idx < 0) {
        if (store->count >= MAX_WINDOWS) {
            log_error("Cannot add more saved layouts, limit reached");
            return false;
        }
        idx = store->count++;
        memset(&store->records[idx], 0, sizeof(store->records[idx]));
        store->records[idx].match_id = match_id;
    }

    store->records[idx].x = x;
    store->records[idx].y = y;
    store->records[idx].width = width;
    store->records[idx].height = height;
    store->records[idx].desktop = desktop;
    store->records[idx].maximized_vert = maximized_vert;
    store->records[idx].maximized_horz = maximized_horz;
    store->records[idx].fullscreen = fullscreen;
    return true;
}

const LayoutRecord *layout_store_get(const LayoutStore *store, int match_id) {
    int idx = layout_store_find_index(store, match_id);
    if (idx < 0) {
        return NULL;
    }

    return &store->records[idx];
}

bool layout_store_clear(LayoutStore *store, int match_id) {
    int idx = layout_store_find_index(store, match_id);
    if (idx < 0) {
        return false;
    }

    if (idx + 1 < store->count) {
        memmove(&store->records[idx], &store->records[idx + 1],
                (size_t)(store->count - idx - 1) * sizeof(store->records[0]));
    }
    store->count--;
    memset(&store->records[store->count], 0, sizeof(store->records[store->count]));
    return true;
}

bool layout_store_save(const LayoutStore *store) {
    if (!store || store->path[0] == '\0') {
        return false;
    }

    FILE *file = fopen(store->path, "w");
    if (!file) {
        log_error("Failed to open layout store for writing: %s", store->path);
        return false;
    }

    fprintf(file, "{\n  \"layouts\": [\n");
    for (int i = 0; i < store->count; i++) {
        const LayoutRecord *record = &store->records[i];
        fprintf(file,
                "    { \"match_id\": %d, \"x\": %d, \"y\": %d, \"w\": %d, \"h\": %d, \"desktop\": %d, \"maximized_vert\": %s, \"maximized_horz\": %s, \"fullscreen\": %s }%s\n",
                record->match_id, record->x, record->y, record->width,
                record->height, record->desktop,
                record->maximized_vert ? "true" : "false",
                record->maximized_horz ? "true" : "false",
                record->fullscreen ? "true" : "false",
                (i + 1 < store->count) ? "," : "");
    }
    fprintf(file, "  ]\n}\n");
    fclose(file);
    return true;
}

static bool parse_layout_line(const char *line, LayoutRecord *record) {
    if (!line || !record) {
        return false;
    }

    int match_id = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int desktop = 0;
    int parsed = sscanf(line,
                        " { \"match_id\": %d, \"x\": %d, \"y\": %d, \"w\": %d, \"h\": %d, \"desktop\": %d }",
                        &match_id, &x, &y, &width, &height, &desktop);
    if (parsed != 6 || match_id <= 0 || width <= 0 || height <= 0) {
        return false;
    }

    record->match_id = match_id;
    record->x = x;
    record->y = y;
    record->width = width;
    record->height = height;
    record->desktop = desktop;
    record->maximized_vert = strstr(line, "\"maximized_vert\": true") != NULL;
    record->maximized_horz = strstr(line, "\"maximized_horz\": true") != NULL;
    record->fullscreen = strstr(line, "\"fullscreen\": true") != NULL;
    return true;
}

bool layout_store_load(LayoutStore *store) {
    if (!store) {
        return false;
    }

    char path[512];
    g_strlcpy(path, store->path, sizeof(path));
    layout_store_init(store);
    g_strlcpy(store->path, path, sizeof(store->path));

    FILE *file = fopen(store->path, "r");
    if (!file) {
        if (errno != ENOENT) {
            log_error("Failed to open layout store for reading: %s", store->path);
        }
        return false;
    }

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        LayoutRecord record = {0};
        if (!parse_layout_line(line, &record)) {
            continue;
        }
        layout_store_set(store, record.match_id, record.x, record.y,
                         record.width, record.height, record.desktop,
                         record.maximized_vert, record.maximized_horz,
                         record.fullscreen);
    }

    fclose(file);
    return store->count > 0;
}

int layout_store_collect_ids(const LayoutStore *store, int *out, int max) {
    if (!store || !out || max <= 0) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < store->count && count < max; i++) {
        if (store->records[i].match_id <= 0) {
            continue;
        }
        out[count++] = store->records[i].match_id;
    }

    return count;
}
