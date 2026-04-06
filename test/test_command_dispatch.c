#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>

// Include the header first (it forward-declares AppData, WindowInfo, handlers)
#include "../src/command_definitions.h"

// Stub all command handlers — we only need the table metadata, not execution.
#define STUB(name) gboolean name(AppData *a, WindowInfo *w, const char *s) { \
    (void)a; (void)w; (void)s; return TRUE; }

STUB(cmd_always_below) STUB(cmd_assign_name) STUB(cmd_assign_slots)
STUB(cmd_always_on_top) STUB(cmd_close_window) STUB(cmd_show_config)
STUB(cmd_change_workspace) STUB(cmd_every_workspace) STUB(cmd_horizontal_maximize)
STUB(cmd_hotkeys) STUB(cmd_jump_workspace) STUB(cmd_move_all_to_workspace)
STUB(cmd_minimize_window) STUB(cmd_mouse) STUB(cmd_maximize_window)
STUB(cmd_pull_window) STUB(cmd_rename_workspace) STUB(cmd_show)
STUB(cmd_set_config) STUB(cmd_skip_taskbar) STUB(cmd_swap_windows)
STUB(cmd_toggle_monitor) STUB(cmd_tile_window) STUB(cmd_vertical_maximize)
STUB(cmd_help)

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_ACTIVATES(cmd_name, expected) do { \
    int found = 0; \
    for (int i = 0; COMMAND_DEFINITIONS[i].primary; i++) { \
        if (strcmp(COMMAND_DEFINITIONS[i].primary, (cmd_name)) == 0) { \
            found = 1; \
            if (COMMAND_DEFINITIONS[i].activates != (expected)) { \
                printf("FAIL: %s .activates — expected %d, got %d\n", \
                       (cmd_name), (expected), COMMAND_DEFINITIONS[i].activates); \
                tests_failed++; \
            } else { \
                printf("PASS: %s .activates = %d\n", (cmd_name), (expected)); \
                tests_passed++; \
            } \
            break; \
        } \
    } \
    if (!found) { \
        printf("FAIL: %s not found in COMMAND_DEFINITIONS\n", (cmd_name)); \
        tests_failed++; \
    } \
} while (0)

#define ASSERT_KEEP_OPEN(cmd_name, expected) do { \
    int found = 0; \
    for (int i = 0; COMMAND_DEFINITIONS[i].primary; i++) { \
        if (strcmp(COMMAND_DEFINITIONS[i].primary, (cmd_name)) == 0) { \
            found = 1; \
            if (COMMAND_DEFINITIONS[i].keeps_open_on_hotkey_auto != (expected)) { \
                printf("FAIL: %s .keeps_open_on_hotkey_auto — expected %d, got %d\n", \
                       (cmd_name), (expected), COMMAND_DEFINITIONS[i].keeps_open_on_hotkey_auto); \
                tests_failed++; \
            } else { \
                printf("PASS: %s .keeps_open_on_hotkey_auto = %d\n", (cmd_name), (expected)); \
                tests_passed++; \
            } \
            break; \
        } \
    } \
    if (!found) { \
        printf("FAIL: %s not found in COMMAND_DEFINITIONS\n", (cmd_name)); \
        tests_failed++; \
    } \
} while (0)

// Verify that every command has the correct .activates value.
// Commands that activate: they modify a window property/position and need
// the dispatcher to focus the target window after (in interactive mode).
// Commands that don't: they close/minimize windows, show UI, change config,
// or handle activation themselves.
static void test_activates_field(void) {
    printf("--- Commands that activate (dispatcher focuses target window) ---\n");
    ASSERT_ACTIVATES("ab",  1);   // always-below: toggles state
    ASSERT_ACTIVATES("aot", 1);   // always-on-top: toggles state
    ASSERT_ACTIVATES("cw",  1);   // change-workspace: moves window
    ASSERT_ACTIVATES("ew",  1);   // every-workspace: toggles sticky
    ASSERT_ACTIVATES("hmw", 1);   // horizontal-maximize: toggles state
    ASSERT_ACTIVATES("mw",  1);   // maximize-window: toggles state
    ASSERT_ACTIVATES("pw",  1);   // pull-window: moves to current desktop
    ASSERT_ACTIVATES("sb",  1);   // skip-taskbar: toggles state
    ASSERT_ACTIVATES("tm",  1);   // toggle-monitor: moves to next monitor
    ASSERT_ACTIVATES("tw",  1);   // tile-window: repositions window
    ASSERT_ACTIVATES("vmw", 1);   // vertical-maximize: toggles state

    printf("\n--- Commands that do NOT activate ---\n");
    ASSERT_ACTIVATES("an",      0);   // assign-name: shows overlay
    ASSERT_ACTIVATES("as",      0);   // assign-slots: assigns workspace slots
    ASSERT_ACTIVATES("cl",      0);   // close: window is closing
    ASSERT_ACTIVATES("config",  0);   // config: shows config tab
    ASSERT_ACTIVATES("help",    0);   // help: shows help text
    ASSERT_ACTIVATES("hotkeys", 0);   // hotkeys: manages bindings
    ASSERT_ACTIVATES("jw",      0);   // jump-workspace: switches desktop, no window
    ASSERT_ACTIVATES("maw",     0);   // move-all: moves multiple windows
    ASSERT_ACTIVATES("miw",     0);   // minimize: handles activation directly
    ASSERT_ACTIVATES("mouse",   0);   // mouse: moves cursor
    ASSERT_ACTIVATES("rw",      0);   // rename-workspace: shows overlay
    ASSERT_ACTIVATES("set",     0);   // set: changes config
    ASSERT_ACTIVATES("show",    0);   // show: switches view
    ASSERT_ACTIVATES("sw",      0);   // swap-windows: swaps geometry only
}

// Verify no command is missing from the test
static void test_keep_open_on_hotkey_auto_field(void) {
    printf("\n--- Hotkey auto-! keep-open metadata ---\n");
    ASSERT_KEEP_OPEN("show", 1);
    ASSERT_KEEP_OPEN("help", 1);
    ASSERT_KEEP_OPEN("config", 1);
    ASSERT_KEEP_OPEN("set", 1);
    ASSERT_KEEP_OPEN("an", 1);
    ASSERT_KEEP_OPEN("rw", 1);
    ASSERT_KEEP_OPEN("hotkeys", 1);

    ASSERT_KEEP_OPEN("jw", 0);
    ASSERT_KEEP_OPEN("cw", 0);
    ASSERT_KEEP_OPEN("maw", 0);
    ASSERT_KEEP_OPEN("tw", 0);
    ASSERT_KEEP_OPEN("mw", 0);
}

static void test_all_commands_covered(void) {
    printf("\n--- Coverage check ---\n");
    int table_count = 0;
    for (int i = 0; COMMAND_DEFINITIONS[i].primary; i++) {
        table_count++;
    }
    // 11 activating + 14 non-activating = 25 commands
    if (table_count == 25) {
        printf("PASS: command table has %d commands (all covered)\n", table_count);
        tests_passed++;
    } else {
        printf("FAIL: command table has %d commands, test expects 25 — update test!\n", table_count);
        tests_failed++;
    }
}

int main(void) {
    printf("Command dispatch tests\n");
    printf("======================\n\n");

    test_activates_field();
    test_keep_open_on_hotkey_auto_field();
    test_all_commands_covered();

    printf("\n=====================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
