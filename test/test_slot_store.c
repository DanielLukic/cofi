#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/slot_store.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s (line %d)\n", name, __LINE__); fail++; } \
} while (0)

static void test_assign_lookup_and_payload_reverse_lookup(void) {
    SlotStore store;
    slot_store_init(&store);

    slot_assign(&store, 'a', "windows", "class\tinstance\ttype\ttitle");
    slot_assign(&store, 'a', "sinks", "alsa_output.same-key");
    slot_assign(&store, '1', "sinks", "alsa_output.primary");

    ASSERT_TRUE("lookup windows slot", strcmp(slot_lookup(&store, "windows", 'a'),
                                             "class\tinstance\ttype\ttitle") == 0);
    ASSERT_TRUE("same key can be assigned in sinks namespace",
                strcmp(slot_lookup(&store, "sinks", 'a'), "alsa_output.same-key") == 0);
    ASSERT_TRUE("reverse lookup returns sink slot",
                slot_for_payload(&store, "sinks", "alsa_output.primary") == '1');
    ASSERT_TRUE("reverse lookup returns own namespace only",
                slot_for_payload(&store, "windows", "alsa_output.same-key") == '\0');

    slot_clear(&store, "sinks", 'a');
    ASSERT_TRUE("clearing sinks slot leaves windows slot",
                strcmp(slot_lookup(&store, "windows", 'a'),
                       "class\tinstance\ttype\ttitle") == 0);
    ASSERT_TRUE("clearing sinks slot removes only sinks slot",
                slot_lookup(&store, "sinks", 'a') == NULL);

    slot_store_free(&store);
}

static void test_save_load_round_trip(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/cofi-slot-store-%ld.json", (long)getpid());

    SlotStore saved;
    slot_store_init_with_path(&saved, path);
    slot_assign(&saved, 'z', "windows", "321\nClass\nInst\nNormal\nWindow Z");
    slot_assign(&saved, 'z', "sinks", "alsa_output.usb-DAC.analog-stereo");
    slot_assign(&saved, '2', "windows", "123\tClass\tInst\tNormal\tTitle with \"quotes\"");

    ASSERT_TRUE("save succeeds", slot_save(&saved));

    SlotStore loaded;
    slot_store_init_with_path(&loaded, path);
    ASSERT_TRUE("load succeeds", slot_load(&loaded));
    ASSERT_TRUE("loads sink payload",
                strcmp(slot_lookup(&loaded, "sinks", 'z'),
                       "alsa_output.usb-DAC.analog-stereo") == 0);
    ASSERT_TRUE("loads same-key window payload independently",
                strcmp(slot_lookup(&loaded, "windows", 'z'),
                       "321\nClass\nInst\nNormal\nWindow Z") == 0);
    ASSERT_TRUE("loads escaped window payload",
                strcmp(slot_lookup(&loaded, "windows", '2'),
                       "123\tClass\tInst\tNormal\tTitle with \"quotes\"") == 0);

    unlink(path);
    slot_store_free(&saved);
    slot_store_free(&loaded);
}

int main(void) {
    test_assign_lookup_and_payload_reverse_lookup();
    test_save_load_round_trip();

    printf("\nSlot store tests: %d passed, %d failed\n", pass, fail);
    return fail == 0 ? 0 : 1;
}
