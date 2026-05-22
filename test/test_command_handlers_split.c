#include <stdio.h>
#include <string.h>

#include "../src/command_registry.h"
#include "../src/core_commands.h"
#include "../src/command_handlers_window.h"
#include "../src/command_handlers_workspace.h"
#include "../src/command_handlers_tiling.h"
#include "../src/command_handlers_ui.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(msg, cond) \
    do { \
        tests_run++; \
        if (cond) { \
            tests_passed++; \
            printf("PASS: %s\n", msg); \
        } else { \
            printf("FAIL: %s\n", msg); \
        } \
    } while (0)

static void test_domain_handler_mappings(void) {
    const CommandSpec *cw = cofi_command_by_primary("cw");
    const CommandSpec *save_layout = cofi_command_by_primary("save-layout");
    const CommandSpec *restore_layout = cofi_command_by_primary("restore-layout");
    const CommandSpec *clear_layout = cofi_command_by_primary("clear-layout");
    const CommandSpec *tw = cofi_command_by_primary("tw");
    const CommandSpec *show = cofi_command_by_primary("show");
    const CommandSpec *sw = cofi_command_by_primary("sw");

    ASSERT_TRUE("cw exists", cw != NULL);
    ASSERT_TRUE("tw exists", tw != NULL);
    ASSERT_TRUE("save-layout exists", save_layout != NULL);
    ASSERT_TRUE("restore-layout exists", restore_layout != NULL);
    ASSERT_TRUE("clear-layout exists", clear_layout != NULL);
    ASSERT_TRUE("show exists", show != NULL);
    ASSERT_TRUE("sw exists", sw != NULL);

    ASSERT_TRUE("cw mapped to workspace domain", cw && cw->handler == cmd_change_workspace);
    ASSERT_TRUE("save-layout mapped to window domain", save_layout && save_layout->handler == cmd_save_layout);
    ASSERT_TRUE("restore-layout mapped to window domain", restore_layout && restore_layout->handler == cmd_restore_layout);
    ASSERT_TRUE("clear-layout mapped to window domain", clear_layout && clear_layout->handler == cmd_clear_layout);
    ASSERT_TRUE("tw mapped to tiling domain", tw && tw->handler == cmd_tile_window);
    ASSERT_TRUE("show mapped to ui domain", show && show->handler == cmd_show);
    ASSERT_TRUE("sw mapped to window domain", sw && sw->handler == cmd_swap_windows);
}

int main(void) {
    printf("Command handler split tests\n");
    printf("===========================\n\n");

    cofi_command_registry_reset();
    cofi_register_core_commands();

    test_domain_handler_mappings();

    printf("\n===========================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
