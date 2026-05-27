#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "harpoon/harpoon.h"
#include "harpoon/harpoon_config.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s (line %d)\n", name, __LINE__); fail++; } \
} while (0)

static void set_test_home(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/cofi-harpoon-config-char-%ld", (long)getpid());
    mkdir(path, 0755);
    setenv("HOME", path, 1);

    char cfg[320];
    snprintf(cfg, sizeof(cfg), "%s/.config", path);
    mkdir(cfg, 0755);
    snprintf(cfg, sizeof(cfg), "%s/.config/cofi", path);
    mkdir(cfg, 0755);
}

static void write_fixture(const char *json) {
    const char *home = getenv("HOME");
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/cofi/harpoon.json", home);
    FILE *f = fopen(path, "w");
    if (!f) {
        return;
    }
    fputs(json, f);
    fclose(f);
}

static void test_load_hydrates_only_windows_slots(void) {
    set_test_home();
    write_fixture(
        "{\n"
        "  \"slots\": [\n"
        "    {\"slot\":\"1\",\"tab\":\"windows\",\"payload\":\"42\"},\n"
        "    {\"slot\":\"2\",\"tab\":\"sinks\",\"payload\":\"999\"},\n"
        "    {\"slot\":\"a\",\"tab\":\"windows\",\"payload\":\"77\"}\n"
        "  ]\n"
        "}\n");

    HarpoonManager harpoon;
    init_harpoon_manager(&harpoon);
    load_harpoon_slots(&harpoon);

    ASSERT_TRUE("windows slot 1 loaded", harpoon.slots[1].assigned == 1 && harpoon.slots[1].match_id == 42);
    ASSERT_TRUE("windows slot a loaded", harpoon.slots[10].assigned == 1 && harpoon.slots[10].match_id == 77);
    ASSERT_TRUE("non-windows tab not hydrated into harpoon slot",
                harpoon.slots[2].assigned == 0 && harpoon.slots[2].match_id == 0);
}

int main(void) {
    test_load_hydrates_only_windows_slots();
    printf("\nHarpoon config characterization: %d passed, %d failed\n", pass, fail);
    return fail == 0 ? 0 : 1;
}

