#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>

// Include command metadata and parser APIs under test.
#include "../src/command_definitions.h"
#include "../src/command_api.h"
#include "../src/command_parser.h"

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
STUB(cmd_workspaces) STUB(cmd_harpoon) STUB(cmd_names) STUB(cmd_help)

static int tests_passed = 0;
static int tests_failed = 0;

typedef struct {
    int seen;
    int fail_at;
    const char *expected[8];
} SegmentVisitState;

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
    ASSERT_ACTIVATES("harpoon", 0);   // harpoon: surfaces tab
    ASSERT_ACTIVATES("help",    0);   // help: shows help text
    ASSERT_ACTIVATES("hotkeys", 0);   // hotkeys: manages bindings
    ASSERT_ACTIVATES("jw",      0);   // jump-workspace: switches desktop, no window
    ASSERT_ACTIVATES("maw",     0);   // move-all: moves multiple windows
    ASSERT_ACTIVATES("miw",     0);   // minimize: handles activation directly
    ASSERT_ACTIVATES("mouse",   0);   // mouse: moves cursor
    ASSERT_ACTIVATES("names",   0);   // names: surfaces tab
    ASSERT_ACTIVATES("rw",      0);   // rename-workspace: shows overlay
    ASSERT_ACTIVATES("set",     0);   // set: changes config
    ASSERT_ACTIVATES("show",    0);   // show: switches view
    ASSERT_ACTIVATES("sw",      0);   // swap-windows: swaps geometry only
    ASSERT_ACTIVATES("workspaces", 0); // workspaces: surfaces tab
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
    ASSERT_KEEP_OPEN("harpoon", 1);
    ASSERT_KEEP_OPEN("names", 1);
    ASSERT_KEEP_OPEN("workspaces", 1);

    ASSERT_KEEP_OPEN("jw", 0);
    ASSERT_KEEP_OPEN("cw", 0);
    ASSERT_KEEP_OPEN("maw", 0);
    ASSERT_KEEP_OPEN("tw", 0);
    ASSERT_KEEP_OPEN("mw", 0);
}

static gboolean record_segment_visitor(const char *segment, void *user_data) {
    SegmentVisitState *state = user_data;
    if (state->expected[state->seen] && strcmp(segment, state->expected[state->seen]) != 0) {
        return FALSE;
    }

    state->seen++;
    if (state->fail_at > 0 && state->seen >= state->fail_at) {
        return FALSE;
    }
    return TRUE;
}

static void test_should_keep_open_runtime_policy(void) {
    printf("\n--- should_keep_open_on_hotkey_auto runtime policy ---\n");

    if (should_keep_open_on_hotkey_auto("show")) { printf("PASS: show keeps open\n"); tests_passed++; }
    else { printf("FAIL: show should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("cw")) { printf("PASS: cw (no arg) keeps open\n"); tests_passed++; }
    else { printf("FAIL: cw with no arg should keep open\n"); tests_failed++; }

    if (!should_keep_open_on_hotkey_auto("cw2")) { printf("PASS: cw2 does not keep open\n"); tests_passed++; }
    else { printf("FAIL: cw2 should not keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("cw2,show windows")) { printf("PASS: chain keeps open when any segment does\n"); tests_passed++; }
    else { printf("FAIL: chain with show should keep open\n"); tests_failed++; }

    if (!should_keep_open_on_hotkey_auto("mw,cw2")) { printf("PASS: non-UI chain does not keep open\n"); tests_passed++; }
    else { printf("FAIL: mw,cw2 should not keep open\n"); tests_failed++; }
}

static void test_command_chain_semantics(void) {
    printf("\n--- Command chain parsing and stop-on-failure ---\n");

    SegmentVisitState parse_state = {
        .seen = 0,
        .fail_at = 0,
        .expected = {"cw1", "twL", "jw2", NULL}
    };

    gboolean parsed = visit_command_segments("  cw1, , twL , jw2  ", record_segment_visitor, &parse_state);
    if (parsed && parse_state.seen == 3) {
        printf("PASS: comma chain parses expected segments\n");
        tests_passed++;
    } else {
        printf("FAIL: comma chain parsing mismatch (ok=%d seen=%d)\n", parsed, parse_state.seen);
        tests_failed++;
    }

    SegmentVisitState stop_state = {
        .seen = 0,
        .fail_at = 2,
        .expected = {"cw1", "twL", "jw2", NULL}
    };

    gboolean stop_ok = visit_command_segments("cw1,twL,jw2", record_segment_visitor, &stop_state);
    if (!stop_ok && stop_state.seen == 2) {
        printf("PASS: chain stops when visitor fails\n");
        tests_passed++;
    } else {
        printf("FAIL: stop-on-failure broken (ok=%d seen=%d)\n", stop_ok, stop_state.seen);
        tests_failed++;
    }
}

static void test_alias_drift_guard(void) {
    printf("\n--- Alias drift guard (definitions vs parser) ---\n");
    for (int i = 0; COMMAND_DEFINITIONS[i].primary; i++) {
        char resolved[64] = {0};
        const char *primary = COMMAND_DEFINITIONS[i].primary;

        if (!resolve_command_primary(primary, resolved, sizeof(resolved)) || strcmp(resolved, primary) != 0) {
            printf("FAIL: primary '%s' does not resolve to itself\n", primary);
            tests_failed++;
            continue;
        }

        for (int a = 0; a < 5 && COMMAND_DEFINITIONS[i].aliases[a]; a++) {
            const char *alias = COMMAND_DEFINITIONS[i].aliases[a];
            if (!resolve_command_primary(alias, resolved, sizeof(resolved)) || strcmp(resolved, primary) != 0) {
                printf("FAIL: alias '%s' does not resolve to '%s'\n", alias, primary);
                tests_failed++;
            } else {
                tests_passed++;
            }
        }
    }
}

static void test_all_commands_covered(void) {
    printf("\n--- Coverage check ---\n");
    int table_count = 0;
    for (int i = 0; COMMAND_DEFINITIONS[i].primary; i++) {
        table_count++;
    }
    // 11 activating + 17 non-activating = 28 commands
    if (table_count == 28) {
        printf("PASS: command table has %d commands (all covered)\n", table_count);
        tests_passed++;
    } else {
        printf("FAIL: command table has %d commands, test expects 28 — update test!\n", table_count);
        tests_failed++;
    }
}

int main(void) {
    printf("Command dispatch tests\n");
    printf("======================\n\n");

    test_activates_field();
    test_keep_open_on_hotkey_auto_field();
    test_should_keep_open_runtime_policy();
    test_command_chain_semantics();
    test_alias_drift_guard();
    test_all_commands_covered();

    printf("\n=====================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
