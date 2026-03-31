#include <stdio.h>
#include <string.h>
#include "../src/command_parser.h"

static int tests_passed = 0;
static int tests_failed = 0;

static void check(const char *desc, const char *input,
                   const char *expected_cmd, const char *expected_arg) {
    char cmd[64] = {0};
    char arg[64] = {0};

    int ok = parse_command_and_arg(input, cmd, arg, sizeof(cmd), sizeof(arg));
    if (!ok) {
        printf("FAIL: %s - returned failure for '%s'\n", desc, input);
        tests_failed++;
        return;
    }
    if (strcmp(cmd, expected_cmd) != 0 || strcmp(arg, expected_arg) != 0) {
        printf("FAIL: %s - input='%s' expected cmd='%s' arg='%s', got cmd='%s' arg='%s'\n",
               desc, input, expected_cmd, expected_arg, cmd, arg);
        tests_failed++;
        return;
    }
    printf("PASS: %s\n", desc);
    tests_passed++;
}

static void check_empty_result(const char *desc, const char *input) {
    char cmd[64] = {0};
    char arg[64] = {0};

    int ok = parse_command_and_arg(input, cmd, arg, sizeof(cmd), sizeof(arg));
    if (ok && cmd[0] == '\0' && arg[0] == '\0') {
        printf("PASS: %s\n", desc);
        tests_passed++;
    } else {
        printf("FAIL: %s - expected empty, got cmd='%s' arg='%s' (ok=%d)\n",
               desc, cmd, arg, ok);
        tests_failed++;
    }
}

/* --- All compact form aliases --- */

static void test_workspace_aliases(void) {
    printf("\n--- Workspace command aliases ---\n");

    // cw primary and alias
    check("cw with space", "cw 5", "cw", "5");
    check("cw compact", "cw5", "cw", "5");
    check("cw bare", "cw", "cw", "");
    check("change-workspace with space", "change-workspace 3", "change-workspace", "3");
    // change-workspace compact: "change-workspace3" -- too long, treated as unknown
    check("change-workspace bare", "change-workspace", "change-workspace", "");

    // cw with direction args
    check("cw h", "cwh", "cw", "h");
    check("cw j", "cwj", "cw", "j");
    check("cw k", "cwk", "cw", "k");
    check("cw l", "cwl", "cw", "l");

    // jw primary and aliases
    check("jw compact", "jw4", "jw", "4");
    check("jw bare", "jw", "jw", "");
    check("j compact", "j3", "jw", "3");
    check("j with space", "j 7", "j", "7");
    check("j bare", "j", "j", "");
    check("jump-workspace with space", "jump-workspace 1", "jump-workspace", "1");
    check("jw direction h", "jwh", "jw", "h");
    check("j direction l", "jl", "jw", "l");
}

static void test_tile_aliases(void) {
    printf("\n--- Tile command aliases ---\n");

    // tw primary
    check("tw compact L", "twL", "tw", "L");
    check("tw compact 5", "tw5", "tw", "5");
    check("tw bare", "tw", "tw", "");
    check("tw with space", "tw R", "tw", "R");

    // t alias
    check("t compact L", "tL", "tw", "L");
    check("t compact R", "tR", "tw", "R");
    check("t compact T", "tT", "tw", "T");
    check("t compact B", "tB", "tw", "B");
    check("t compact F", "tF", "tw", "F");
    check("t compact C", "tC", "tw", "C");
    check("t compact grid 1", "t1", "tw", "1");
    check("t compact grid 9", "t9", "tw", "9");
    check("t lowercase l", "tl", "tw", "l");
    check("t lowercase r", "tr", "tw", "r");
    check("t lowercase t", "tt", "tw", "t");
    check("t lowercase b", "tb", "tw", "b");
    check("t lowercase f", "tf", "tw", "f");
    check("t lowercase c", "tc", "tw", "c");
    check("t bare", "t", "t", "");

    // tile-window alias
    check("tile-window with space", "tile-window L", "tile-window", "L");
    check("tile-window bare", "tile-window", "tile-window", "");

    // Multi-char tile args (e.g., r4, l2, c1)
    check("tr4 direct", "tr4", "tw", "r4");
    check("tl2 direct", "tl2", "tw", "l2");
    check("tc1 direct", "tc1", "tw", "c1");
}

static void test_mouse_aliases(void) {
    printf("\n--- Mouse command aliases ---\n");

    // mouse primary
    check("mouse bare", "mouse", "mouse", "");
    check("mouse away", "mouse away", "mouse", "away");
    check("mouse show", "mouse show", "mouse", "show");
    check("mouse hide", "mouse hide", "mouse", "hide");

    // m alias
    check("m bare", "m", "m", "");
    check("m with space arg", "m show", "m", "show");

    // ma alias - "ma" is exact, "mah" expands
    check("ma bare", "ma", "ma", "");
    check("mah compact", "mah", "mouse", "h");
    check("mas compact", "mas", "mouse", "s");
    check("maa compact", "maa", "mouse", "a");

    // ms alias
    check("ms bare", "ms", "ms", "");
    check("msa compact", "msa", "mouse", "a");
    check("mss compact", "mss", "mouse", "s");
    check("msh compact", "msh", "mouse", "h");

    // mh alias
    check("mh bare", "mh", "mh", "");
    check("mha compact", "mha", "mouse", "a");
    check("mhs compact", "mhs", "mouse", "s");
    check("mhh compact", "mhh", "mouse", "h");
}

static void test_move_all_workspace(void) {
    printf("\n--- Move-all-to-workspace aliases ---\n");

    check("maw bare", "maw", "maw", "");
    check("maw compact 3", "maw3", "maw", "3");
    check("maw compact h", "mawh", "maw", "h");
    check("maw compact j", "mawj", "maw", "j");
    check("maw compact k", "mawk", "maw", "k");
    check("maw compact l", "mawl", "maw", "l");
    check("maw with space", "maw 5", "maw", "5");
    check("move-all-to-workspace with space", "move-all-to-workspace 2", "move-all-to-workspace", "2");
}

/* --- Edge cases --- */

static void test_null_and_empty(void) {
    printf("\n--- NULL and empty inputs ---\n");

    char cmd[64], arg[64];
    int ok;

    // NULL input
    ok = parse_command_and_arg(NULL, cmd, arg, sizeof(cmd), sizeof(arg));
    if (!ok) {
        printf("PASS: NULL input returns FALSE\n");
        tests_passed++;
    } else {
        printf("FAIL: NULL input should return FALSE\n");
        tests_failed++;
    }

    // Empty string
    check_empty_result("empty string", "");

    // Whitespace only
    check_empty_result("whitespace only", "   ");
    check_empty_result("tabs only", "\t\t");

    // NULL buffers
    ok = parse_command_and_arg("test", NULL, arg, sizeof(cmd), sizeof(arg));
    if (!ok) {
        printf("PASS: NULL cmd_out returns FALSE\n");
        tests_passed++;
    } else {
        printf("FAIL: NULL cmd_out should return FALSE\n");
        tests_failed++;
    }

    ok = parse_command_and_arg("test", cmd, NULL, sizeof(cmd), sizeof(arg));
    if (!ok) {
        printf("PASS: NULL arg_out returns FALSE\n");
        tests_passed++;
    } else {
        printf("FAIL: NULL arg_out should return FALSE\n");
        tests_failed++;
    }

    // Zero-size buffers
    ok = parse_command_and_arg("test", cmd, arg, 0, sizeof(arg));
    if (!ok) {
        printf("PASS: zero cmd_size returns FALSE\n");
        tests_passed++;
    } else {
        printf("FAIL: zero cmd_size should return FALSE\n");
        tests_failed++;
    }
}

static void test_trim_whitespace(void) {
    printf("\n--- trim_whitespace_in_place ---\n");

    char buf[64];

    strcpy(buf, "  hello  ");
    trim_whitespace_in_place(buf);
    if (strcmp(buf, "hello") == 0) {
        printf("PASS: trim both sides\n");
        tests_passed++;
    } else {
        printf("FAIL: trim both sides - got '%s'\n", buf);
        tests_failed++;
    }

    strcpy(buf, "hello");
    trim_whitespace_in_place(buf);
    if (strcmp(buf, "hello") == 0) {
        printf("PASS: no trimming needed\n");
        tests_passed++;
    } else {
        printf("FAIL: no trimming needed - got '%s'\n", buf);
        tests_failed++;
    }

    buf[0] = '\0';
    trim_whitespace_in_place(buf);
    if (buf[0] == '\0') {
        printf("PASS: empty string\n");
        tests_passed++;
    } else {
        printf("FAIL: empty string - got '%s'\n", buf);
        tests_failed++;
    }

    // NULL safe
    trim_whitespace_in_place(NULL);
    printf("PASS: NULL does not crash\n");
    tests_passed++;

    strcpy(buf, "   ");
    trim_whitespace_in_place(buf);
    if (buf[0] == '\0') {
        printf("PASS: all spaces becomes empty\n");
        tests_passed++;
    } else {
        printf("FAIL: all spaces - got '%s'\n", buf);
        tests_failed++;
    }
}

static void test_small_buffers(void) {
    printf("\n--- Small buffer sizes ---\n");

    char cmd[4] = {0};
    char arg[4] = {0};

    // Command gets truncated to fit buffer
    int ok = parse_command_and_arg("longcommand 123", cmd, arg, sizeof(cmd), sizeof(arg));
    if (ok) {
        printf("PASS: small buffer does not crash (cmd='%s' arg='%s')\n", cmd, arg);
        tests_passed++;
    } else {
        printf("FAIL: small buffer should not fail\n");
        tests_failed++;
    }

    // Size 1 buffers
    char cmd1[1] = {0};
    char arg1[1] = {0};
    ok = parse_command_and_arg("test arg", cmd1, arg1, 1, 1);
    if (ok) {
        printf("PASS: size-1 buffers do not crash\n");
        tests_passed++;
    } else {
        printf("FAIL: size-1 buffers should succeed\n");
        tests_failed++;
    }
}

static void test_commands_without_compact_form(void) {
    printf("\n--- Commands without compact form ---\n");

    // These commands don't have compact forms - they should pass through as-is
    check("help bare", "help", "help", "");
    check("help with arg", "help cw", "help", "cw");
    check("h alias bare", "h", "h", "");
    check("? alias bare", "?", "?", "");
    check("c bare", "c", "c", "");
    check("cl bare", "cl", "cl", "");
    check("close bare", "close", "close", "");
    check("tm bare", "tm", "tm", "");
    check("sb bare", "sb", "sb", "");
    check("pw bare", "pw", "pw", "");
    check("mw bare", "mw", "mw", "");
    check("miw bare", "miw", "miw", "");
    check("min bare", "min", "min", "");
    check("minimize-window bare", "minimize-window", "minimize-window", "");
    check("sw bare", "sw", "sw", "");
    check("ew bare", "ew", "ew", "");
    check("aot bare", "aot", "aot", "");
    check("ab bare", "ab", "ab", "");
    check("an bare", "an", "an", "");
    check("an with arg", "an myname", "an", "myname");
    check("rw bare", "rw", "rw", "");
    check("rw with arg", "rw 3", "rw", "3");
    check("set with args", "set close_on_focus_loss true", "set", "close_on_focus_loss true");
    check("config bare", "config", "config", "");
    check("show bare", "show", "show", "");
    check("show windows", "show windows", "show", "windows");
    check("show command", "show command", "show", "command");
    check("hotkeys bare", "hotkeys", "hotkeys", "");
    check("hotkeys with args", "hotkeys Mod4+1 jw 1", "hotkeys", "Mod4+1 jw 1");
}

static void test_multi_digit_args(void) {
    printf("\n--- Multi-digit arguments ---\n");

    check("cw10", "cw10", "cw", "10");
    check("cw99", "cw99", "cw", "99");
    check("jw12", "jw12", "jw", "12");
    check("j20", "j20", "jw", "20");
    check("maw15", "maw15", "maw", "15");
}

int main(void) {
    printf("Command Parser Alias & Edge Case Tests\n");
    printf("=======================================\n");

    test_workspace_aliases();
    test_tile_aliases();
    test_mouse_aliases();
    test_move_all_workspace();
    test_null_and_empty();
    test_trim_whitespace();
    test_small_buffers();
    test_commands_without_compact_form();
    test_multi_digit_args();

    printf("\n=====================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
