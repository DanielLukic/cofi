#include <stdio.h>
#include <string.h>
#include "../src/command_parser.h"

static int tests_passed = 0;
static int tests_failed = 0;

static void assert_true(const char *name, int condition) {
    if (condition) {
        printf("PASS: %s\n", name);
        tests_passed++;
    } else {
        printf("FAIL: %s\n", name);
        tests_failed++;
    }
}

static void test_parse_command_for_execution_alias_resolution(void) {
    char cmd[64] = {0};
    char arg[64] = {0};

    assert_true("change-workspace resolves to cw",
                parse_command_for_execution("change-workspace 3", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "cw") == 0 && strcmp(arg, "3") == 0);

    assert_true("compact j5 resolves to jw",
                parse_command_for_execution("j5", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "jw") == 0 && strcmp(arg, "5") == 0);

    assert_true("tile alias tL resolves to tw",
                parse_command_for_execution("tL", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "tw") == 0 && strcmp(arg, "L") == 0);

    assert_true("hotkey alias resolves to hotkeys",
                parse_command_for_execution("hotkey Mod4+w show windows", cmd, arg, sizeof(cmd), sizeof(arg)) &&
                strcmp(cmd, "hotkeys") == 0 && strcmp(arg, "Mod4+w show windows") == 0);
}

static void test_next_command_segment(void) {
    char chain[128] = "  cw1,  tL , , jw2  ";
    char *cursor = chain;
    char segment[64] = {0};

    assert_true("first segment cw1", next_command_segment(&cursor, segment, sizeof(segment)) && strcmp(segment, "cw1") == 0);
    assert_true("second segment tL", next_command_segment(&cursor, segment, sizeof(segment)) && strcmp(segment, "tL") == 0);
    assert_true("third segment jw2", next_command_segment(&cursor, segment, sizeof(segment)) && strcmp(segment, "jw2") == 0);
    assert_true("no more segments", !next_command_segment(&cursor, segment, sizeof(segment)));
}

int main(void) {
    printf("Command parser execution-path tests\n");
    printf("===================================\n\n");

    test_parse_command_for_execution_alias_resolution();
    test_next_command_segment();

    printf("\n===================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);
    return tests_failed ? 1 : 0;
}
