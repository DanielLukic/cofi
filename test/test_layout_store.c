#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../src/layout_store.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(name, cond) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
        printf("PASS: %s\n", name); \
    } else { \
        printf("FAIL: %s (line %d)\n", name, __LINE__); \
    } \
} while (0)

static void set_test_home(const char *suffix) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/cofi-layout-store-%s-%ld", suffix, (long)getpid());
    mkdir(path, 0755);
    setenv("HOME", path, 1);
}

static void test_layout_store_crud_and_persist(void) {
    set_test_home("crud");

    LayoutStore store;
    layout_store_init(&store);

    ASSERT_TRUE("first layout insert succeeds",
                layout_store_set(&store, 7, 10, 20, 300, 400, 2));
    ASSERT_TRUE("second layout insert succeeds",
                layout_store_set(&store, 9, 30, 40, 500, 600, 4));
    ASSERT_TRUE("two records stored", store.count == 2);

    const LayoutRecord *first = layout_store_get(&store, 7);
    ASSERT_TRUE("record lookup finds first layout", first != NULL);
    ASSERT_TRUE("first layout fields stored",
                first && first->x == 10 && first->y == 20 &&
                first->width == 300 && first->height == 400 &&
                first->desktop == 2);

    ASSERT_TRUE("upsert updates existing layout",
                layout_store_set(&store, 7, 11, 22, 333, 444, 5));
    ASSERT_TRUE("upsert keeps record count stable", store.count == 2);
    first = layout_store_get(&store, 7);
    ASSERT_TRUE("updated layout fields replaced",
                first && first->x == 11 && first->y == 22 &&
                first->width == 333 && first->height == 444 &&
                first->desktop == 5);

    ASSERT_TRUE("store persists to layouts.json", layout_store_save(&store));

    LayoutStore loaded;
    layout_store_init(&loaded);
    ASSERT_TRUE("store reloads from layouts.json", layout_store_load(&loaded));
    ASSERT_TRUE("reloaded record count matches", loaded.count == 2);
    ASSERT_TRUE("reloaded updated record preserved",
                loaded.records[0].match_id == 7 &&
                loaded.records[0].x == 11 &&
                loaded.records[0].desktop == 5);

    int ids[MAX_WINDOWS] = {0};
    int count = layout_store_collect_ids(&loaded, ids, MAX_WINDOWS);
    ASSERT_TRUE("collect_ids returns both layout ids", count == 2);
    ASSERT_TRUE("collect_ids preserves insertion order", ids[0] == 7 && ids[1] == 9);

    ASSERT_TRUE("clear removes stored record", layout_store_clear(&loaded, 7));
    ASSERT_TRUE("cleared record no longer resolves", layout_store_get(&loaded, 7) == NULL);
    ASSERT_TRUE("remaining record shifts down", loaded.count == 1 && loaded.records[0].match_id == 9);
}

int main(void) {
    printf("Layout store tests\n");
    printf("==================\n\n");

    test_layout_store_crud_and_persist();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
