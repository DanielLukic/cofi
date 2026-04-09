#include <stdio.h>
#include <string.h>

#include "../src/command_definitions.h"
#include "../src/command_handlers_window.h"
#include "../src/command_handlers_workspace.h"
#include "../src/command_handlers_tiling.h"
#include "../src/command_handlers_ui.h"

#define STUB_HANDLER(name) \
    gboolean name(AppData *app, WindowInfo *window, const char *args) { \
        (void)app; (void)window; (void)args; \
        return FALSE; \
    }

STUB_HANDLER(cmd_change_workspace)
STUB_HANDLER(cmd_pull_window)
STUB_HANDLER(cmd_toggle_monitor)
STUB_HANDLER(cmd_skip_taskbar)
STUB_HANDLER(cmd_always_on_top)
STUB_HANDLER(cmd_always_below)
STUB_HANDLER(cmd_every_workspace)
STUB_HANDLER(cmd_close_window)
STUB_HANDLER(cmd_minimize_window)
STUB_HANDLER(cmd_maximize_window)
STUB_HANDLER(cmd_horizontal_maximize)
STUB_HANDLER(cmd_vertical_maximize)
STUB_HANDLER(cmd_jump_workspace)
STUB_HANDLER(cmd_rename_workspace)
STUB_HANDLER(cmd_tile_window)
STUB_HANDLER(cmd_assign_name)
STUB_HANDLER(cmd_help)
STUB_HANDLER(cmd_mouse)
STUB_HANDLER(cmd_move_all_to_workspace)
STUB_HANDLER(cmd_swap_windows)
STUB_HANDLER(cmd_assign_slots)
STUB_HANDLER(cmd_set_config)
STUB_HANDLER(cmd_show_config)
STUB_HANDLER(cmd_show)
STUB_HANDLER(cmd_hotkeys)

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

static const CommandDef *find_command(const char *primary) {
    for (int i = 0; COMMAND_DEFINITIONS[i].primary; i++) {
        if (strcmp(COMMAND_DEFINITIONS[i].primary, primary) == 0) {
            return &COMMAND_DEFINITIONS[i];
        }
    }
    return NULL;
}

static void test_domain_handler_mappings(void) {
    const CommandDef *cw = find_command("cw");
    const CommandDef *tw = find_command("tw");
    const CommandDef *show = find_command("show");
    const CommandDef *sw = find_command("sw");

    ASSERT_TRUE("cw exists", cw != NULL);
    ASSERT_TRUE("tw exists", tw != NULL);
    ASSERT_TRUE("show exists", show != NULL);
    ASSERT_TRUE("sw exists", sw != NULL);

    ASSERT_TRUE("cw mapped to workspace domain", cw && cw->handler == cmd_change_workspace);
    ASSERT_TRUE("tw mapped to tiling domain", tw && tw->handler == cmd_tile_window);
    ASSERT_TRUE("show mapped to ui domain", show && show->handler == cmd_show);
    ASSERT_TRUE("sw mapped to window domain", sw && sw->handler == cmd_swap_windows);
}

int main(void) {
    printf("Command handler split tests\n");
    printf("===========================\n\n");

    test_domain_handler_mappings();

    printf("\n===========================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
