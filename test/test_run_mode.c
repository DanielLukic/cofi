#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "run/run_mode.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

#include "run/run_mode.c"

static void test_extract_run_command_strips_prefix_and_whitespace(void) {
    char command[256];

    ASSERT_TRUE("extract strips ! and whitespace",
                extract_run_command("!   echo hi  ", command, sizeof(command)) &&
                strcmp(command, "echo hi") == 0);
}

static void test_extract_run_command_accepts_raw_entry_text(void) {
    char command[256];

    ASSERT_TRUE("extract accepts raw run entry without !",
                extract_run_command("echo hi", command, sizeof(command)) &&
                strcmp(command, "echo hi") == 0);
}

static void test_extract_run_command_rejects_empty_and_whitespace_only(void) {
    char command[256];

    ASSERT_TRUE("extract rejects bare prompt",
                !extract_run_command("!", command, sizeof(command)));
    ASSERT_TRUE("extract rejects whitespace-only command",
                !extract_run_command("!   \t  ", command, sizeof(command)));
}

static void test_run_history_is_session_only_ring_with_dedup_of_latest(void) {
    RunMode run_mode;

    init_run_mode(&run_mode);
    add_run_history_entry(&run_mode, "echo one");
    add_run_history_entry(&run_mode, "echo one");
    add_run_history_entry(&run_mode, "echo two");

    ASSERT_TRUE("history keeps only distinct latest command",
                run_mode.history_count == 2);
    ASSERT_TRUE("history newest first",
                strcmp(run_mode.history[0], "echo two") == 0);
    ASSERT_TRUE("history retains prior command",
                strcmp(run_mode.history[1], "echo one") == 0);
}

int main(void) {
    printf("Run mode tests\n");
    printf("==============\n\n");

    test_extract_run_command_strips_prefix_and_whitespace();
    test_extract_run_command_accepts_raw_entry_text();
    test_extract_run_command_rejects_empty_and_whitespace_only();
    test_run_history_is_session_only_ring_with_dedup_of_latest();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
