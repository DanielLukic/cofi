#ifndef LAYOUT_STORE_H
#define LAYOUT_STORE_H

#include <stdbool.h>

#include "core/utils/types.h"

typedef struct {
    int match_id;
    int x;
    int y;
    int width;
    int height;
    int desktop;
    bool maximized_vert;
    bool maximized_horz;
    bool fullscreen;
    bool restore_desktop;
    bool disabled;
} LayoutRecord;

typedef struct {
    LayoutRecord records[MAX_WINDOWS];
    int count;
    char path[512];
} LayoutStore;

void layout_store_init(LayoutStore *store);
void layout_store_init_with_path(LayoutStore *store, const char *path);

bool layout_store_set(LayoutStore *store, int match_id,
                      int x, int y, int width, int height, int desktop,
                      bool maximized_vert, bool maximized_horz, bool fullscreen,
                      bool restore_desktop, bool disabled);
const LayoutRecord *layout_store_get(const LayoutStore *store, int match_id);
bool layout_store_clear(LayoutStore *store, int match_id);

bool layout_store_save(const LayoutStore *store);
bool layout_store_load(LayoutStore *store);

int layout_store_collect_ids(const LayoutStore *store, int *out, int max);

#endif
