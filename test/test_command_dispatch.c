#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>

// Include command metadata and parser APIs under test.
#include "../src/command_registry.h"
#include "../src/core_commands.h"
#include "../src/command_api.h"
#include "../src/command_parser.h"
#include "../src/cofi_tab_provider.h"

// Stub all command handlers — we only need the table metadata, not execution.
#define STUB(name) gboolean name(AppData *a, WindowInfo *w, const char *s) { \
    (void)a; (void)w; (void)s; return TRUE; }

STUB(cmd_always_below) STUB(cmd_assign_name) STUB(cmd_assign_slots)
STUB(cmd_always_on_top) STUB(cmd_close_window)
STUB(cmd_change_workspace) STUB(cmd_every_workspace) STUB(cmd_horizontal_maximize)
STUB(cmd_jump_workspace) STUB(cmd_jump_slot) STUB(cmd_move_all_to_workspace)
STUB(cmd_minimize_window) STUB(cmd_mouse) STUB(cmd_maximize_window)
STUB(cmd_pull_window) STUB(cmd_rename_workspace) STUB(cmd_show)
STUB(cmd_set_config) STUB(cmd_skip_taskbar) STUB(cmd_swap_windows)
STUB(cmd_toggle_monitor) STUB(cmd_tile_window) STUB(cmd_vertical_maximize)
STUB(cmd_calc)
STUB(cmd_run) STUB(cmd_help)

static int tests_passed = 0;
static int tests_failed = 0;

static void register_provider_with_command(const CommandSpec *spec) {
    CofiTabProvider provider;
    cofi_init_provider_defaults(&provider);
    provider.id = spec->owner_provider_id;
    provider.tab_mode = COFI_PROVIDER_DYNAMIC_TAB;
    if (cofi_register_tab_provider(&provider) >= 0) {
        cofi_register_command(spec);
    }
}

static const CommandSpec s_provider_commands[] = {
    {.primary = "sessions", .aliases = {"session", NULL}, .owner_provider_id = "sessions", .handler = cmd_run, .description = "Search Claude and Codex sessions", .help_format = "sessions, session [TERMS | REFINE]", .keeps_open_on_hotkey_auto = 1},
    {.primary = "profiles", .aliases = {"chrome", "browser", "browsers", NULL}, .owner_provider_id = "profiles", .handler = cmd_run, .description = "Switch to browser profiles tab", .help_format = "profiles, chrome [@SLOT|PROFILE]", .keeps_open_on_hotkey_auto = 1},
    {.primary = "calc", .aliases = {"ca", NULL}, .owner_provider_id = "calc", .handler = cmd_run, .description = "Switch to calculator", .help_format = "calc, ca", .keeps_open_on_hotkey_auto = 1},
    {.primary = "run", .aliases = {"r", NULL}, .owner_provider_id = "run", .handler = cmd_run, .description = "Switch to run mode", .help_format = "run, r", .keeps_open_on_hotkey_auto = 1},
    {.primary = "sinks", .aliases = {"sink", NULL}, .owner_provider_id = "sinks", .handler = cmd_run, .description = "Switch to audio sinks tab", .help_format = "sinks, sink [@SLOT|SINK]", .keeps_open_on_hotkey_auto = 1},
    {.primary = "proc", .aliases = {"ps", NULL}, .owner_provider_id = "proc", .handler = cmd_run, .description = "Switch to process manager tab", .help_format = "proc, ps", .keeps_open_on_hotkey_auto = 1},
    {.primary = "projects", .aliases = {"project", "tmux", "tx", "zj", "zellij"}, .owner_provider_id = "projects", .handler = cmd_run, .description = "Switch to projects tab", .help_format = "projects, project, tmux, tx, zj, zellij [@SLOT|SESSION]", .keeps_open_on_hotkey_auto = 1},
    {.primary = "workspaces", .aliases = {"ws", NULL}, .owner_provider_id = "workspaces", .handler = cmd_run, .description = "Switch to Workspaces tab", .help_format = "workspaces, ws", .keeps_open_on_hotkey_auto = 1},
    {.primary = "harpoon", .aliases = {"hp", NULL}, .owner_provider_id = "harpoon", .handler = cmd_run, .description = "Switch to Harpoon tab", .help_format = "harpoon, hp", .keeps_open_on_hotkey_auto = 1},
    {.primary = "names", .aliases = {"nm", NULL}, .owner_provider_id = "names", .handler = cmd_run, .description = "Switch to Names tab", .help_format = "names, nm", .keeps_open_on_hotkey_auto = 1},
    {.primary = "rules", .aliases = {"rl", NULL}, .owner_provider_id = "rules", .handler = cmd_run, .description = "Switch to Rules tab", .help_format = "rules, rl", .keeps_open_on_hotkey_auto = 1},
    {.primary = "config", .aliases = {"conf", "cfg", NULL}, .owner_provider_id = "config", .handler = cmd_run, .description = "Show current configuration", .help_format = "config, conf", .keeps_open_on_hotkey_auto = 1},
    {.primary = "hotkeys", .aliases = {"hotkey", "hk", NULL}, .owner_provider_id = "hotkeys", .handler = cmd_run, .description = "Manage system hotkey bindings", .help_format = "hotkeys [key] [command]", .keeps_open_on_hotkey_auto = 1},
    {.primary = "apps", .aliases = {"applications", "app", NULL}, .owner_provider_id = "apps", .handler = cmd_run, .description = "Switch to applications tab", .help_format = "apps, app, applications", .keeps_open_on_hotkey_auto = 1},
};

static void register_provider_commands(void) {
    for (size_t i = 0; i < sizeof(s_provider_commands) / sizeof(s_provider_commands[0]); i++) {
        register_provider_with_command(&s_provider_commands[i]);
    }
}

typedef struct {
    int seen;
    int fail_at;
    const char *expected[8];
} SegmentVisitState;

#define ASSERT_ACTIVATES(cmd_name, expected) do { \
    const CommandSpec *spec = cofi_command_by_primary((cmd_name)); \
    if (!spec) { \
        printf("FAIL: %s not found in command registry\n", (cmd_name)); \
        tests_failed++; \
    } else if (spec->activates != (expected)) { \
        printf("FAIL: %s .activates — expected %d, got %d\n", \
               (cmd_name), (expected), spec->activates); \
        tests_failed++; \
    } else { \
        printf("PASS: %s .activates = %d\n", (cmd_name), (expected)); \
        tests_passed++; \
    } \
} while (0)

#define ASSERT_KEEP_OPEN(cmd_name, expected) do { \
    const CommandSpec *spec = cofi_command_by_primary((cmd_name)); \
    if (!spec) { \
        printf("FAIL: %s not found in command registry\n", (cmd_name)); \
        tests_failed++; \
    } else if (spec->keeps_open_on_hotkey_auto != (expected)) { \
        printf("FAIL: %s .keeps_open_on_hotkey_auto — expected %d, got %d\n", \
               (cmd_name), (expected), spec->keeps_open_on_hotkey_auto); \
        tests_failed++; \
    } else { \
        printf("PASS: %s .keeps_open_on_hotkey_auto = %d\n", (cmd_name), (expected)); \
        tests_passed++; \
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
    ASSERT_ACTIVATES("help",    0);   // help: shows help text
    ASSERT_ACTIVATES("jw",      0);   // jump-workspace: switches desktop, no window
    ASSERT_ACTIVATES("jump-slot", 0); // jump-slot: activates internally
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
    ASSERT_KEEP_OPEN("set", 1);
    ASSERT_KEEP_OPEN("an", 1);
    ASSERT_KEEP_OPEN("rw", 1);

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

    if (should_keep_open_on_hotkey_auto("session")) { printf("PASS: sessions provider alias keeps open\n"); tests_passed++; }
    else { printf("FAIL: sessions provider alias should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("profiles")) { printf("PASS: profiles provider command keeps open\n"); tests_passed++; }
    else { printf("FAIL: profiles provider command should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("calc")) { printf("PASS: calc provider command keeps open\n"); tests_passed++; }
    else { printf("FAIL: calc provider command should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("run")) { printf("PASS: run provider command keeps open\n"); tests_passed++; }
    else { printf("FAIL: run provider command should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("sinks")) { printf("PASS: sinks provider command keeps open\n"); tests_passed++; }
    else { printf("FAIL: sinks provider command should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("proc")) { printf("PASS: proc provider command keeps open\n"); tests_passed++; }
    else { printf("FAIL: proc provider command should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("tmux")) { printf("PASS: projects provider alias keeps open\n"); tests_passed++; }
    else { printf("FAIL: projects provider alias should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("projects")) { printf("PASS: projects provider command keeps open\n"); tests_passed++; }
    else { printf("FAIL: projects provider command should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("workspaces")) { printf("PASS: workspaces provider command keeps open\n"); tests_passed++; }
    else { printf("FAIL: workspaces provider command should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("ws")) { printf("PASS: workspaces provider alias keeps open\n"); tests_passed++; }
    else { printf("FAIL: workspaces provider alias should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("harpoon")) { printf("PASS: harpoon provider command keeps open\n"); tests_passed++; }
    else { printf("FAIL: harpoon provider command should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("hp")) { printf("PASS: harpoon provider alias keeps open\n"); tests_passed++; }
    else { printf("FAIL: harpoon provider alias should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("names")) { printf("PASS: names provider command keeps open\n"); tests_passed++; }
    else { printf("FAIL: names provider command should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("nm")) { printf("PASS: names provider alias keeps open\n"); tests_passed++; }
    else { printf("FAIL: names provider alias should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("rules")) { printf("PASS: rules provider command keeps open\n"); tests_passed++; }
    else { printf("FAIL: rules provider command should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("rl")) { printf("PASS: rules provider alias keeps open\n"); tests_passed++; }
    else { printf("FAIL: rules provider alias should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("config")) { printf("PASS: config provider command keeps open\n"); tests_passed++; }
    else { printf("FAIL: config provider command should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("conf")) { printf("PASS: config provider alias conf keeps open\n"); tests_passed++; }
    else { printf("FAIL: config provider alias conf should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("cfg")) { printf("PASS: config provider alias cfg keeps open\n"); tests_passed++; }
    else { printf("FAIL: config provider alias cfg should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("hotkeys")) { printf("PASS: hotkeys provider command keeps open\n"); tests_passed++; }
    else { printf("FAIL: hotkeys provider command should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("hotkey")) { printf("PASS: hotkeys provider alias hotkey keeps open\n"); tests_passed++; }
    else { printf("FAIL: hotkeys provider alias hotkey should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("hk")) { printf("PASS: hotkeys provider alias hk keeps open\n"); tests_passed++; }
    else { printf("FAIL: hotkeys provider alias hk should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("apps")) { printf("PASS: apps provider command keeps open\n"); tests_passed++; }
    else { printf("FAIL: apps provider command should keep open\n"); tests_failed++; }

    if (should_keep_open_on_hotkey_auto("app")) { printf("PASS: apps provider alias app keeps open\n"); tests_passed++; }
    else { printf("FAIL: apps provider alias app should keep open\n"); tests_failed++; }

    int proc_id = cofi_get_provider_id("proc");
    cofi_set_provider_enabled(proc_id, 0);
    if (!should_keep_open_on_hotkey_auto("proc")) { printf("PASS: disabled proc command does not keep open\n"); tests_passed++; }
    else { printf("FAIL: disabled proc command should not keep open\n"); tests_failed++; }
    cofi_set_provider_enabled(proc_id, 1);

    int projects_id = cofi_get_provider_id("projects");
    cofi_set_provider_enabled(projects_id, 0);
    if (!should_keep_open_on_hotkey_auto("tmux")) { printf("PASS: disabled projects alias does not keep open\n"); tests_passed++; }
    else { printf("FAIL: disabled projects alias should not keep open\n"); tests_failed++; }
    cofi_set_provider_enabled(projects_id, 1);

    int workspaces_id = cofi_get_provider_id("workspaces");
    cofi_set_provider_enabled(workspaces_id, 0);
    if (!should_keep_open_on_hotkey_auto("ws")) { printf("PASS: disabled workspaces alias does not keep open\n"); tests_passed++; }
    else { printf("FAIL: disabled workspaces alias should not keep open\n"); tests_failed++; }
    cofi_set_provider_enabled(workspaces_id, 1);

    int names_id = cofi_get_provider_id("names");
    cofi_set_provider_enabled(names_id, 0);
    if (!should_keep_open_on_hotkey_auto("nm")) { printf("PASS: disabled names alias does not keep open\n"); tests_passed++; }
    else { printf("FAIL: disabled names alias should not keep open\n"); tests_failed++; }
    cofi_set_provider_enabled(names_id, 1);

    int rules_id = cofi_get_provider_id("rules");
    cofi_set_provider_enabled(rules_id, 0);
    if (!should_keep_open_on_hotkey_auto("rl")) { printf("PASS: disabled rules alias does not keep open\n"); tests_passed++; }
    else { printf("FAIL: disabled rules alias should not keep open\n"); tests_failed++; }
    cofi_set_provider_enabled(rules_id, 1);

    int harpoon_id = cofi_get_provider_id("harpoon");
    cofi_set_provider_enabled(harpoon_id, 0);
    if (!should_keep_open_on_hotkey_auto("hp")) { printf("PASS: disabled harpoon alias does not keep open\n"); tests_passed++; }
    else { printf("FAIL: disabled harpoon alias should not keep open\n"); tests_failed++; }
    cofi_set_provider_enabled(harpoon_id, 1);

    int config_id = cofi_get_provider_id("config");
    cofi_set_provider_enabled(config_id, 0);
    if (!should_keep_open_on_hotkey_auto("cfg")) { printf("PASS: disabled config alias does not keep open\n"); tests_passed++; }
    else { printf("FAIL: disabled config alias should not keep open\n"); tests_failed++; }
    cofi_set_provider_enabled(config_id, 1);

    int hotkeys_id = cofi_get_provider_id("hotkeys");
    cofi_set_provider_enabled(hotkeys_id, 0);
    if (!should_keep_open_on_hotkey_auto("hk")) { printf("PASS: disabled hotkeys alias does not keep open\n"); tests_passed++; }
    else { printf("FAIL: disabled hotkeys alias should not keep open\n"); tests_failed++; }
    cofi_set_provider_enabled(hotkeys_id, 1);

    int apps_id = cofi_get_provider_id("apps");
    cofi_set_provider_enabled(apps_id, 0);
    if (!should_keep_open_on_hotkey_auto("app")) { printf("PASS: disabled apps alias does not keep open\n"); tests_passed++; }
    else { printf("FAIL: disabled apps alias should not keep open\n"); tests_failed++; }
    cofi_set_provider_enabled(apps_id, 1);

    int sessions_id = cofi_get_provider_id("sessions");
    cofi_set_provider_enabled(sessions_id, 0);
    if (!should_keep_open_on_hotkey_auto("session")) { printf("PASS: disabled sessions alias does not keep open\n"); tests_passed++; }
    else { printf("FAIL: disabled sessions alias should not keep open\n"); tests_failed++; }
    cofi_set_provider_enabled(sessions_id, 1);
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

static void test_window_state_alias_arg_resolution(void) {
    printf("\n--- window-state alias arg resolution ---\n");

    char primary[64] = {0};
    char arg[64] = {0};
    gboolean ok = parse_command_for_execution("skip-taskbar on", primary, arg, sizeof(primary), sizeof(arg));
    if (ok && strcmp(primary, "sb") == 0 && strcmp(arg, "on") == 0) {
        printf("PASS: skip-taskbar on resolves to sb + on\n");
        tests_passed++;
    } else {
        printf("FAIL: skip-taskbar on resolve mismatch (ok=%d primary='%s' arg='%s')\n", ok, primary, arg);
        tests_failed++;
    }

    memset(primary, 0, sizeof(primary));
    memset(arg, 0, sizeof(arg));
    ok = parse_command_for_execution("sb off", primary, arg, sizeof(primary), sizeof(arg));
    if (ok && strcmp(primary, "sb") == 0 && strcmp(arg, "off") == 0) {
        printf("PASS: sb off keeps arg off\n");
        tests_passed++;
    } else {
        printf("FAIL: sb off resolve mismatch (ok=%d primary='%s' arg='%s')\n", ok, primary, arg);
        tests_failed++;
    }

    memset(primary, 0, sizeof(primary));
    memset(arg, 0, sizeof(arg));
    ok = parse_command_for_execution("ab+", primary, arg, sizeof(primary), sizeof(arg));
    if (ok && strcmp(primary, "ab") == 0 && strcmp(arg, "+") == 0) {
        printf("PASS: ab+ resolves to ab + +\n");
        tests_passed++;
    } else {
        printf("FAIL: ab+ resolve mismatch (ok=%d primary='%s' arg='%s')\n", ok, primary, arg);
        tests_failed++;
    }

    memset(primary, 0, sizeof(primary));
    memset(arg, 0, sizeof(arg));
    ok = parse_command_for_execution("at-", primary, arg, sizeof(primary), sizeof(arg));
    if (ok && strcmp(primary, "aot") == 0 && strcmp(arg, "-") == 0) {
        printf("PASS: at- resolves to aot + -\n");
        tests_passed++;
    } else {
        printf("FAIL: at- resolve mismatch (ok=%d primary='%s' arg='%s')\n", ok, primary, arg);
        tests_failed++;
    }

    memset(primary, 0, sizeof(primary));
    memset(arg, 0, sizeof(arg));
    ok = parse_command_for_execution("every-workspace+", primary, arg, sizeof(primary), sizeof(arg));
    if (ok && strcmp(primary, "ew") == 0 && strcmp(arg, "+") == 0) {
        printf("PASS: every-workspace+ resolves to ew + +\n");
        tests_passed++;
    } else {
        printf("FAIL: every-workspace+ resolve mismatch (ok=%d primary='%s' arg='%s')\n", ok, primary, arg);
        tests_failed++;
    }
}

static void test_alias_drift_guard(void) {
    printf("\n--- Alias drift guard (registry vs parser) ---\n");
    for (int i = 0; i < cofi_command_count(); i++) {
        const CommandSpec *spec = cofi_command_at(i);
        char resolved[64] = {0};
        const char *primary = spec->primary;

        if (!resolve_command_primary(primary, resolved, sizeof(resolved)) || strcmp(resolved, primary) != 0) {
            printf("FAIL: primary '%s' does not resolve to itself\n", primary);
            tests_failed++;
            continue;
        }

        for (int a = 0; a < 5 && spec->aliases[a]; a++) {
            const char *alias = spec->aliases[a];
            if (!resolve_command_primary(alias, resolved, sizeof(resolved)) || strcmp(resolved, primary) != 0) {
                printf("FAIL: alias '%s' does not resolve to '%s'\n", alias, primary);
                tests_failed++;
            } else {
                tests_passed++;
            }
        }
    }
}

static void test_provider_command_alias_resolution(void) {
    printf("\n--- Provider command alias resolution ---\n");
    char resolved[64] = {0};

    if (resolve_command_primary("profiles", resolved, sizeof(resolved)) &&
        strcmp(resolved, "profiles") == 0) {
        printf("PASS: provider primary profiles resolves to itself\n");
        tests_passed++;
    } else {
        printf("FAIL: provider primary profiles did not resolve\n");
        tests_failed++;
    }

    if (resolve_command_primary("chrome", resolved, sizeof(resolved)) &&
        strcmp(resolved, "profiles") == 0) {
        printf("PASS: provider alias chrome resolves to profiles\n");
        tests_passed++;
    } else {
        printf("FAIL: provider alias chrome did not resolve to profiles\n");
        tests_failed++;
    }

    if (resolve_command_primary("ca", resolved, sizeof(resolved)) &&
        strcmp(resolved, "calc") == 0) {
        printf("PASS: provider alias ca resolves to calc\n");
        tests_passed++;
    } else {
        printf("FAIL: provider alias ca did not resolve to calc\n");
        tests_failed++;
    }

    if (resolve_command_primary("r", resolved, sizeof(resolved)) &&
        strcmp(resolved, "run") == 0) {
        printf("PASS: provider alias r resolves to run\n");
        tests_passed++;
    } else {
        printf("FAIL: provider alias r did not resolve to run\n");
        tests_failed++;
    }

    if (resolve_command_primary("sink", resolved, sizeof(resolved)) &&
        strcmp(resolved, "sinks") == 0) {
        printf("PASS: provider alias sink resolves to sinks\n");
        tests_passed++;
    } else {
        printf("FAIL: provider alias sink did not resolve to sinks\n");
        tests_failed++;
    }

    if (resolve_command_primary("ps", resolved, sizeof(resolved)) &&
        strcmp(resolved, "proc") == 0) {
        printf("PASS: provider alias ps resolves to proc\n");
        tests_passed++;
    } else {
        printf("FAIL: provider alias ps did not resolve to proc\n");
        tests_failed++;
    }

    if (resolve_command_primary("zj", resolved, sizeof(resolved)) &&
        strcmp(resolved, "projects") == 0) {
        printf("PASS: provider alias zj resolves to projects\n");
        tests_passed++;
    } else {
        printf("FAIL: provider alias zj did not resolve to projects\n");
        tests_failed++;
    }

    if (resolve_command_primary("ws", resolved, sizeof(resolved)) &&
        strcmp(resolved, "workspaces") == 0) {
        printf("PASS: provider alias ws resolves to workspaces\n");
        tests_passed++;
    } else {
        printf("FAIL: provider alias ws did not resolve to workspaces\n");
        tests_failed++;
    }

    if (resolve_command_primary("hp", resolved, sizeof(resolved)) &&
        strcmp(resolved, "harpoon") == 0) {
        printf("PASS: provider alias hp resolves to harpoon\n");
        tests_passed++;
    } else {
        printf("FAIL: provider alias hp did not resolve to harpoon\n");
        tests_failed++;
    }

    if (resolve_command_primary("nm", resolved, sizeof(resolved)) &&
        strcmp(resolved, "names") == 0) {
        printf("PASS: provider alias nm resolves to names\n");
        tests_passed++;
    } else {
        printf("FAIL: provider alias nm did not resolve to names\n");
        tests_failed++;
    }

    if (resolve_command_primary("rl", resolved, sizeof(resolved)) &&
        strcmp(resolved, "rules") == 0) {
        printf("PASS: provider alias rl resolves to rules\n");
        tests_passed++;
    } else {
        printf("FAIL: provider alias rl did not resolve to rules\n");
        tests_failed++;
    }

    if (resolve_command_primary("conf", resolved, sizeof(resolved)) &&
        strcmp(resolved, "config") == 0) {
        printf("PASS: provider alias conf resolves to config\n");
        tests_passed++;
    } else {
        printf("FAIL: provider alias conf did not resolve to config\n");
        tests_failed++;
    }

    if (resolve_command_primary("cfg", resolved, sizeof(resolved)) &&
        strcmp(resolved, "config") == 0) {
        printf("PASS: provider alias cfg resolves to config\n");
        tests_passed++;
    } else {
        printf("FAIL: provider alias cfg did not resolve to config\n");
        tests_failed++;
    }

    if (resolve_command_primary("hotkey", resolved, sizeof(resolved)) &&
        strcmp(resolved, "hotkeys") == 0) {
        printf("PASS: provider alias hotkey resolves to hotkeys\n");
        tests_passed++;
    } else {
        printf("FAIL: provider alias hotkey did not resolve to hotkeys\n");
        tests_failed++;
    }

    if (resolve_command_primary("hk", resolved, sizeof(resolved)) &&
        strcmp(resolved, "hotkeys") == 0) {
        printf("PASS: provider alias hk resolves to hotkeys\n");
        tests_passed++;
    } else {
        printf("FAIL: provider alias hk did not resolve to hotkeys\n");
        tests_failed++;
    }

    if (resolve_command_primary("app", resolved, sizeof(resolved)) &&
        strcmp(resolved, "apps") == 0) {
        printf("PASS: provider alias app resolves to apps\n");
        tests_passed++;
    } else {
        printf("FAIL: provider alias app did not resolve to apps\n");
        tests_failed++;
    }

    if (resolve_command_primary("session", resolved, sizeof(resolved)) &&
        strcmp(resolved, "sessions") == 0) {
        printf("PASS: provider alias session resolves to sessions\n");
        tests_passed++;
    } else {
        printf("FAIL: provider alias session did not resolve to sessions\n");
        tests_failed++;
    }
}

static void test_all_parse_defs_have_owner(void) {
    printf("\n--- Command ownership guard ---\n");
    for (int i = 0; i < cofi_command_count(); i++) {
        const CommandSpec *spec = cofi_command_at(i);
        const char *owner = spec->owner_provider_id;
        if (owner && owner[0] != '\0') {
            printf("PASS: %s has owner %s\n", spec->primary, owner);
            tests_passed++;
        } else {
            printf("FAIL: %s has no command owner\n", spec->primary);
            tests_failed++;
        }
    }
}

static void test_all_commands_covered(void) {
    printf("\n--- Coverage check ---\n");
    int table_count = cofi_command_count();
    // 24 core commands + 14 provider-owned commands.
    if (table_count == 38) {
        printf("PASS: command registry has %d commands (all covered)\n", table_count);
        tests_passed++;
    } else {
        printf("FAIL: command registry has %d commands, test expects 38 - update test!\n", table_count);
        tests_failed++;
    }
}

int main(void) {
    printf("Command dispatch tests\n");
    printf("======================\n\n");

    cofi_registry_reset();
    cofi_command_registry_reset();
    cofi_register_core_commands();
    register_provider_commands();

    test_activates_field();
    test_keep_open_on_hotkey_auto_field();
    test_should_keep_open_runtime_policy();
    test_command_chain_semantics();
    test_window_state_alias_arg_resolution();
    test_alias_drift_guard();
    test_provider_command_alias_resolution();
    test_all_parse_defs_have_owner();
    test_all_commands_covered();

    printf("\n=====================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
