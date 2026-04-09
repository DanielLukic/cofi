#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/app_init.h"
#include "../src/hotkeys.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("  PASS: %s\n", name); pass++; } \
    else { printf("  FAIL: %s\n", name); fail++; } \
} while (0)

static void test_init_hotkey_grab_state_resets_fields(void) {
    printf("\n--- init_hotkey_grab_state resets fields ---\n");

    HotkeyGrabState state;
    memset(&state, 0xAB, sizeof(state));

    init_hotkey_grab_state(&state);

    ASSERT_TRUE("grabbed_count reset to zero", state.grabbed_count == 0);
    ASSERT_TRUE("first key name cleared", state.grabbed_hotkeys[0].key_name[0] == '\0');
    ASSERT_TRUE("first command cleared", state.grabbed_hotkeys[0].command[0] == '\0');
}

static void test_populate_hotkey_grab_state_counts_valid_bindings(void) {
    printf("\n--- populate_hotkey_grab_state counts valid bindings ---\n");

    HotkeyConfig config;
    memset(&config, 0, sizeof(config));

    strcpy(config.bindings[0].key, "Mod1+Tab");
    strcpy(config.bindings[0].command, "show windows!");

    strcpy(config.bindings[1].key, "Bogus+Key");
    strcpy(config.bindings[1].command, "bad");

    strcpy(config.bindings[2].key, "Control+Return");
    strcpy(config.bindings[2].command, "show command!");

    config.count = 3;

    HotkeyGrabState state;
    memset(&state, 0, sizeof(state));

    int populated = populate_hotkey_grab_state(&config, &state);

    ASSERT_TRUE("only valid bindings are counted", populated == 2);
    ASSERT_TRUE("state grabbed_count matches populated", state.grabbed_count == 2);
    ASSERT_TRUE("first key copied", strcmp(state.grabbed_hotkeys[0].key_name, "Mod1+Tab") == 0);
    ASSERT_TRUE("second key copied", strcmp(state.grabbed_hotkeys[1].key_name, "Control+Return") == 0);
}

// --- app_init.c dependency stubs (for end-to-end init_app_data test path) ---
void init_selection(AppData *app) { (void)app; }
void init_harpoon_manager(HarpoonManager *harpoon) { (void)harpoon; }
void init_workspace_slots(WorkspaceSlotManager *manager) { (void)manager; }
void init_slot_overlay_state(SlotOverlayState *state) { (void)state; }
void init_window_highlight(WindowHighlight *highlight) { (void)highlight; }
void init_hotkey_config(HotkeyConfig *config) { config->count = 0; }
gboolean load_hotkey_config(HotkeyConfig *config) { (void)config; return TRUE; }
int add_hotkey_binding(HotkeyConfig *config, const char *key, const char *command) {
    (void)config; (void)key; (void)command;
    return 1;
}
gboolean save_hotkey_config(const HotkeyConfig *config) { (void)config; return TRUE; }
void init_named_window_manager(NamedWindowManager *manager) { (void)manager; }
void load_named_windows(NamedWindowManager *manager) { (void)manager; }
void init_rules_config(RulesConfig *config) { (void)config; }
gboolean load_rules_config(RulesConfig *config) { (void)config; return TRUE; }
void init_rule_state(RuleState *state) { (void)state; }
void init_command_mode(CommandMode *cmd) { (void)cmd; }
void init_layout_states(WorkspaceLayoutState *states, int count) {
    (void)states;
    (void)count;
}

// Unused init_x11/init_workspace symbols in app_init.o (stubbed for linker completeness)
void atom_cache_init(Display *display, AtomCache *cache) { (void)display; (void)cache; }
void get_window_list(AppData *app) { (void)app; }
bool check_and_reassign_windows(HarpoonManager *h, WindowInfo *w, int count) {
    (void)h; (void)w; (void)count;
    return false;
}
bool check_and_reassign_names(NamedWindowManager *n, WindowInfo *w, int count) {
    (void)n; (void)w; (void)count;
    return false;
}
void filter_windows(AppData *app, const char *query) { (void)app; (void)query; }
int get_number_of_desktops(Display *display) { (void)display; return 0; }
int get_current_desktop(Display *display) { (void)display; return 0; }
char **get_desktop_names(Display *display, int *count) { (void)display; if (count) *count = 0; return NULL; }
void safe_string_copy(char *dest, const char *src, int dest_size) {
    (void)dest; (void)src; (void)dest_size;
}
void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

static void test_init_app_data_initializes_hotkey_grab_state(void) {
    printf("\n--- init_app_data initializes hotkey_grab_state ---\n");

    AppData app;
    memset(&app, 0, sizeof(app));

    app.hotkey_grab_state.grabbed_count = 99;
    app.hotkey_grab_state.grabbed_hotkeys[0].key_name[0] = 'X';

    init_app_data(&app);

    ASSERT_TRUE("init_app_data resets grabbed_count through real init path",
                app.hotkey_grab_state.grabbed_count == 0);
    ASSERT_TRUE("init_app_data clears first key_name through real init path",
                app.hotkey_grab_state.grabbed_hotkeys[0].key_name[0] == '\0');
}

int main(void) {
    printf("Hotkey grab state tests\n");
    printf("=======================\n");

    test_init_hotkey_grab_state_resets_fields();
    test_populate_hotkey_grab_state_counts_valid_bindings();
    test_init_app_data_initializes_hotkey_grab_state();

    printf("\n=== Summary: %d/%d passed ===\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
