#include "layout_store.h"

#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cofi_json_io.h"
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
                      bool maximized_vert, bool maximized_horz, bool fullscreen,
                      bool restore_desktop, bool disabled) {
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
    store->records[idx].restore_desktop = restore_desktop;
    store->records[idx].disabled = disabled;
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

    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "layouts");
    json_builder_begin_array(builder);
    for (int i = 0; i < store->count; i++) {
        const LayoutRecord *record = &store->records[i];
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "match_id");
        json_builder_add_int_value(builder, record->match_id);
        json_builder_set_member_name(builder, "x");
        json_builder_add_int_value(builder, record->x);
        json_builder_set_member_name(builder, "y");
        json_builder_add_int_value(builder, record->y);
        json_builder_set_member_name(builder, "w");
        json_builder_add_int_value(builder, record->width);
        json_builder_set_member_name(builder, "h");
        json_builder_add_int_value(builder, record->height);
        json_builder_set_member_name(builder, "desktop");
        json_builder_add_int_value(builder, record->desktop);
        json_builder_set_member_name(builder, "maximized_vert");
        json_builder_add_boolean_value(builder, record->maximized_vert);
        json_builder_set_member_name(builder, "maximized_horz");
        json_builder_add_boolean_value(builder, record->maximized_horz);
        json_builder_set_member_name(builder, "fullscreen");
        json_builder_add_boolean_value(builder, record->fullscreen);
        json_builder_set_member_name(builder, "restore_desktop");
        json_builder_add_boolean_value(builder, record->restore_desktop);
        json_builder_set_member_name(builder, "disabled");
        json_builder_add_boolean_value(builder, record->disabled);
        json_builder_end_object(builder);
    }
    json_builder_end_array(builder);
    json_builder_end_object(builder);

    JsonNode *root = json_builder_get_root(builder);
    bool ok = cofi_json_save_root(store->path, root);
    json_node_unref(root);
    g_object_unref(builder);
    return ok;
}

bool layout_store_load(LayoutStore *store) {
    if (!store) {
        return false;
    }

    char path[512];
    g_strlcpy(path, store->path, sizeof(path));
    layout_store_init(store);
    g_strlcpy(store->path, path, sizeof(store->path));

    JsonParser *parser = cofi_json_load_object_file(store->path);
    if (!parser) {
        return false;
    }

    JsonObject *root = json_node_get_object(json_parser_get_root(parser));
    JsonArray *layouts = cofi_json_obj_array(root, "layouts");
    if (!layouts) {
        g_object_unref(parser);
        return false;
    }

    guint n = json_array_get_length(layouts);
    for (guint i = 0; i < n; i++) {
        JsonNode *element = json_array_get_element(layouts, i);
        if (!element || !JSON_NODE_HOLDS_OBJECT(element)) {
            continue;
        }
        JsonObject *layout = json_node_get_object(element);
        gboolean has_match_id = FALSE;
        int match_id = cofi_json_obj_int_or(layout, "match_id", 0, &has_match_id);
        if (!has_match_id || match_id <= 0) {
            log_warn("layout_store: skipping record with missing/invalid match_id");
            continue;
        }
        int width = cofi_json_obj_int_or(layout, "w", 0, NULL);
        int height = cofi_json_obj_int_or(layout, "h", 0, NULL);
        if (width <= 0 || height <= 0) {
            log_warn("layout_store: skipping match_id=%d with invalid size", match_id);
            continue;
        }

        layout_store_set(store, match_id,
                         cofi_json_obj_int_or(layout, "x", 0, NULL),
                         cofi_json_obj_int_or(layout, "y", 0, NULL),
                         width,
                         height,
                         cofi_json_obj_int_or(layout, "desktop", 0, NULL),
                         cofi_json_obj_bool_or(layout, "maximized_vert", FALSE, NULL),
                         cofi_json_obj_bool_or(layout, "maximized_horz", FALSE, NULL),
                         cofi_json_obj_bool_or(layout, "fullscreen", FALSE, NULL),
                         cofi_json_obj_bool_or(layout, "restore_desktop", TRUE, NULL),
                         cofi_json_obj_bool_or(layout, "disabled", FALSE, NULL));
    }

    g_object_unref(parser);
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
