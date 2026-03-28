#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/hotkey_config.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_INT(desc, expected, actual) do { \
    if ((expected) != (actual)) { \
        printf("FAIL: %s — expected %d, got %d\n", (desc), (expected), (actual)); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } \
} while (0)

#define ASSERT_STR(desc, expected, actual) do { \
    if (strcmp((expected), (actual)) != 0) { \
        printf("FAIL: %s — expected '%s', got '%s'\n", (desc), (expected), (actual)); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", (desc)); \
        tests_passed++; \
    } \
} while (0)

static void test_parse_hotkey_command(void) {
    char key[64], cmd[256];
    int result;

    // Show bindings: no args
    result = parse_hotkey_command("", key, sizeof(key), cmd, sizeof(cmd));
    ASSERT_INT("empty args → show", 0, result);

    result = parse_hotkey_command(NULL, key, sizeof(key), cmd, sizeof(cmd));
    ASSERT_INT("null args → show", 0, result);

    // Show bindings: "list" keyword
    result = parse_hotkey_command("list", key, sizeof(key), cmd, sizeof(cmd));
    ASSERT_INT("'list' → show", 0, result);

    // Bind: key + command
    result = parse_hotkey_command("Mod4+1 jw 1", key, sizeof(key), cmd, sizeof(cmd));
    ASSERT_INT("key+cmd → bind", 1, result);
    ASSERT_STR("bind key", "Mod4+1", key);
    ASSERT_STR("bind cmd", "jw 1", cmd);

    // Bind: with "set" keyword
    result = parse_hotkey_command("set Mod4+1 jw 1", key, sizeof(key), cmd, sizeof(cmd));
    ASSERT_INT("set key+cmd → bind", 1, result);
    ASSERT_STR("set bind key", "Mod4+1", key);
    ASSERT_STR("set bind cmd", "jw 1", cmd);

    // Bind: with "add" keyword
    result = parse_hotkey_command("add Mod1+Tab show windows", key, sizeof(key), cmd, sizeof(cmd));
    ASSERT_INT("add key+cmd → bind", 1, result);
    ASSERT_STR("add bind key", "Mod1+Tab", key);
    ASSERT_STR("add bind cmd", "show windows", cmd);

    // Unbind: key only
    result = parse_hotkey_command("Mod4+1", key, sizeof(key), cmd, sizeof(cmd));
    ASSERT_INT("key only → unbind", 2, result);
    ASSERT_STR("unbind key", "Mod4+1", key);

    // Unbind: with "del" keyword
    result = parse_hotkey_command("del Mod4+1", key, sizeof(key), cmd, sizeof(cmd));
    ASSERT_INT("del key → unbind", 2, result);
    ASSERT_STR("del unbind key", "Mod4+1", key);

    // Unbind: with "rm" keyword
    result = parse_hotkey_command("rm Mod1+BackSpace", key, sizeof(key), cmd, sizeof(cmd));
    ASSERT_INT("rm key → unbind", 2, result);
    ASSERT_STR("rm unbind key", "Mod1+BackSpace", key);

    // Unbind: with "remove" keyword
    result = parse_hotkey_command("remove Mod4+space", key, sizeof(key), cmd, sizeof(cmd));
    ASSERT_INT("remove key → unbind", 2, result);
    ASSERT_STR("remove unbind key", "Mod4+space", key);
}

static void test_add_remove_bindings(void) {
    HotkeyConfig config;
    init_hotkey_config(&config);

    ASSERT_INT("initially empty", 0, config.count);

    // Add bindings
    ASSERT_INT("add first", 1, add_hotkey_binding(&config, "Mod1+Tab", "show windows"));
    ASSERT_INT("count after add", 1, config.count);
    ASSERT_STR("first key", "Mod1+Tab", config.bindings[0].key);
    ASSERT_STR("first cmd", "show windows", config.bindings[0].command);

    ASSERT_INT("add second", 1, add_hotkey_binding(&config, "Mod4+1", "jw 1"));
    ASSERT_INT("count after second add", 2, config.count);

    // Update existing binding (same key, new command)
    ASSERT_INT("update existing", 1, add_hotkey_binding(&config, "Mod1+Tab", "show command"));
    ASSERT_INT("count unchanged after update", 2, config.count);
    ASSERT_STR("updated cmd", "show command", config.bindings[0].command);

    // Find
    ASSERT_INT("find existing", 0, find_hotkey_binding(&config, "Mod1+Tab"));
    ASSERT_INT("find second", 1, find_hotkey_binding(&config, "Mod4+1"));
    ASSERT_INT("find nonexistent", -1, find_hotkey_binding(&config, "Mod4+9"));

    // Remove
    ASSERT_INT("remove existing", 1, remove_hotkey_binding(&config, "Mod1+Tab"));
    ASSERT_INT("count after remove", 1, config.count);
    ASSERT_INT("removed not found", -1, find_hotkey_binding(&config, "Mod1+Tab"));
    ASSERT_INT("remove nonexistent", 0, remove_hotkey_binding(&config, "Mod1+Tab"));
}

static void test_save_load_roundtrip(void) {
    // Use temp dir
    char tmpdir[] = "/tmp/cofi_hk_test_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        printf("FAIL: mkdtemp\n");
        tests_failed++;
        return;
    }
    setenv("HOME", tmpdir, 1);

    HotkeyConfig original, loaded;
    init_hotkey_config(&original);

    add_hotkey_binding(&original, "Mod1+Tab", "show windows");
    add_hotkey_binding(&original, "Mod1+grave", "show command");
    add_hotkey_binding(&original, "Mod4+1", "jw 1");

    ASSERT_INT("save", 1, save_hotkey_config(&original));

    init_hotkey_config(&loaded);
    ASSERT_INT("load", 1, load_hotkey_config(&loaded));
    ASSERT_INT("loaded count", 3, loaded.count);
    ASSERT_STR("loaded key 0", "Mod1+Tab", loaded.bindings[0].key);
    ASSERT_STR("loaded cmd 0", "show windows", loaded.bindings[0].command);
    ASSERT_STR("loaded key 2", "Mod4+1", loaded.bindings[2].key);
    ASSERT_STR("loaded cmd 2", "jw 1", loaded.bindings[2].command);

    // Cleanup
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);
}

static void test_format_display(void) {
    HotkeyConfig config;
    init_hotkey_config(&config);
    add_hotkey_binding(&config, "Mod1+Tab", "show windows");
    add_hotkey_binding(&config, "Mod4+1", "jw 1");

    char buf[2048] = {0};
    int len = format_hotkey_display(&config, buf, sizeof(buf));

    if (len <= 0) {
        printf("FAIL: format returned %d\n", len);
        tests_failed++;
        return;
    }

    if (strstr(buf, "Mod1+Tab") && strstr(buf, "show windows")) {
        printf("PASS: display contains first binding\n");
        tests_passed++;
    } else {
        printf("FAIL: display missing first binding\n");
        tests_failed++;
    }

    if (strstr(buf, "Mod4+1") && strstr(buf, "jw 1")) {
        printf("PASS: display contains second binding\n");
        tests_passed++;
    } else {
        printf("FAIL: display missing second binding\n");
        tests_failed++;
    }
}

int main(void) {
    printf("Hotkey config tests\n");
    printf("===================\n\n");

    test_parse_hotkey_command();
    test_add_remove_bindings();
    test_save_load_roundtrip();
    test_format_display();

    printf("\n=====================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
