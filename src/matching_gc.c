#include "matching_gc.h"

#include "app_data.h"
#include "layout_store.h"
#include "log.h"
#include "match_entry.h"
#include "match_entry_config.h"

int matching_run_gc(AppData *app) {
    if (!app) {
        return 0;
    }

    int referenced_ids[MAX_HARPOON_SLOTS + (MAX_WINDOWS * 2)];
    int referenced_count = 0;

    for (int i = 0; i < MAX_HARPOON_SLOTS; i++) {
        if (!app->harpoon.slots[i].assigned || app->harpoon.slots[i].match_id <= 0) {
            continue;
        }
        referenced_ids[referenced_count++] = app->harpoon.slots[i].match_id;
    }

    referenced_count += match_entry_collect_labeled_ids(
        &app->matching,
        referenced_ids + referenced_count,
        (int)(sizeof(referenced_ids) / sizeof(referenced_ids[0])) - referenced_count);

    referenced_count += layout_store_collect_ids(
        &app->layouts,
        referenced_ids + referenced_count,
        (int)(sizeof(referenced_ids) / sizeof(referenced_ids[0])) - referenced_count);

    int removed = match_entry_gc(&app->matching, referenced_ids, referenced_count);
    if (removed > 0) {
        save_match_entries(&app->matching);
        log_info("GC removed %d unreferenced matching entries", removed);
    }

    return removed;
}
