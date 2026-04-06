#include <stdio.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include "../src/utils.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("  PASS: %s\n", msg); } \
    else { printf("  FAIL: %s (line %d)\n", msg, __LINE__); } \
} while(0)

// --- Helpers ---

static gboolean parse_ok(const char *str, guint *key, GdkModifierType *mods) {
    char err[256] = {0};
    gboolean ok = parse_shortcut_with_error(str, key, mods, err, sizeof(err));
    if (!ok) printf("    (error: %s)\n", err);
    return ok;
}

static gboolean parse_fail(const char *str, char *err, size_t err_size) {
    guint key;
    GdkModifierType mods;
    return !parse_shortcut_with_error(str, &key, &mods, err, err_size);
}

static gboolean canonicalize_ok(const char *str, char *out, size_t out_size) {
    char err[256] = {0};
    gboolean ok = canonicalize_hotkey_shortcut(str, out, out_size, err, sizeof(err));
    if (!ok) printf("    (error: %s)\n", err);
    return ok;
}

static gboolean canonicalize_fail(const char *str, char *err, size_t err_size) {
    char out[128];
    return !canonicalize_hotkey_shortcut(str, out, sizeof(out), err, err_size);
}

static gboolean canonicalize_event_ok(GdkEventKey *event, char *out, size_t out_size) {
    char err[256] = {0};
    gboolean ok = canonicalize_hotkey_event(event, out, out_size, err, sizeof(err));
    if (!ok) {
        printf("    (error: %s)\n", err);
    }
    return ok;
}

static gboolean canonicalize_event_fail(GdkEventKey *event, char *err, size_t err_size) {
    char out[128];
    return !canonicalize_hotkey_event(event, out, sizeof(out), err, err_size);
}

// --- Test groups ---

static void test_case_insensitive_modifiers(void) {
    printf("\n--- Case-insensitive modifiers ---\n");
    guint key; GdkModifierType mods;

    ASSERT(parse_ok("Ctrl+a", &key, &mods) && (mods & GDK_CONTROL_MASK),
           "Ctrl+a (capitalized)");
    ASSERT(parse_ok("ctrl+a", &key, &mods) && (mods & GDK_CONTROL_MASK),
           "ctrl+a (lowercase)");
    ASSERT(parse_ok("CTRL+a", &key, &mods) && (mods & GDK_CONTROL_MASK),
           "CTRL+a (uppercase)");
    ASSERT(parse_ok("cTrL+a", &key, &mods) && (mods & GDK_CONTROL_MASK),
           "cTrL+a (mixed case)");

    ASSERT(parse_ok("Alt+b", &key, &mods) && (mods & GDK_MOD1_MASK),
           "Alt+b");
    ASSERT(parse_ok("alt+b", &key, &mods) && (mods & GDK_MOD1_MASK),
           "alt+b");
    ASSERT(parse_ok("ALT+b", &key, &mods) && (mods & GDK_MOD1_MASK),
           "ALT+b");

    ASSERT(parse_ok("Super+c", &key, &mods) && (mods & GDK_SUPER_MASK),
           "Super+c");
    ASSERT(parse_ok("super+c", &key, &mods) && (mods & GDK_SUPER_MASK),
           "super+c");
    ASSERT(parse_ok("SUPER+c", &key, &mods) && (mods & GDK_SUPER_MASK),
           "SUPER+c");

    ASSERT(parse_ok("Shift+d", &key, &mods) && (mods & GDK_SHIFT_MASK),
           "Shift+d");
    ASSERT(parse_ok("shift+d", &key, &mods) && (mods & GDK_SHIFT_MASK),
           "shift+d");
}

static void test_modifier_aliases(void) {
    printf("\n--- Modifier aliases ---\n");
    guint key; GdkModifierType mods;

    // Alt == Mod1
    ASSERT(parse_ok("Alt+x", &key, &mods) && (mods & GDK_MOD1_MASK),
           "Alt maps to Mod1");
    ASSERT(parse_ok("Mod1+x", &key, &mods) && (mods & GDK_MOD1_MASK),
           "Mod1 maps to Mod1");
    ASSERT(parse_ok("mod1+x", &key, &mods) && (mods & GDK_MOD1_MASK),
           "mod1 (lowercase) maps to Mod1");

    // Super == Mod4 == Win
    ASSERT(parse_ok("Super+x", &key, &mods) && (mods & GDK_SUPER_MASK),
           "Super maps to Super");
    ASSERT(parse_ok("Mod4+x", &key, &mods) && (mods & GDK_SUPER_MASK),
           "Mod4 maps to Super");
    ASSERT(parse_ok("Win+x", &key, &mods) && (mods & GDK_SUPER_MASK),
           "Win maps to Super");
    ASSERT(parse_ok("Windows+x", &key, &mods) && (mods & GDK_SUPER_MASK),
           "Windows maps to Super");

    // Ctrl == Control
    ASSERT(parse_ok("Ctrl+x", &key, &mods) && (mods & GDK_CONTROL_MASK),
           "Ctrl maps to Control");
    ASSERT(parse_ok("Control+x", &key, &mods) && (mods & GDK_CONTROL_MASK),
           "Control maps to Control");

    // Meta, Hyper
    ASSERT(parse_ok("Meta+x", &key, &mods) && (mods & GDK_META_MASK),
           "Meta modifier");
    ASSERT(parse_ok("Hyper+x", &key, &mods) && (mods & GDK_HYPER_MASK),
           "Hyper modifier");
}

static void test_key_aliases(void) {
    printf("\n--- Key aliases ---\n");
    guint key; GdkModifierType mods;

    // Enter == Return
    ASSERT(parse_ok("Alt+Enter", &key, &mods) && key == GDK_KEY_Return,
           "Enter maps to Return");
    ASSERT(parse_ok("Alt+Return", &key, &mods) && key == GDK_KEY_Return,
           "Return maps to Return");
    ASSERT(parse_ok("Alt+enter", &key, &mods) && key == GDK_KEY_Return,
           "enter (lowercase) maps to Return");

    // Esc == Escape
    ASSERT(parse_ok("Ctrl+Escape", &key, &mods) && key == GDK_KEY_Escape,
           "Escape key");
    ASSERT(parse_ok("Ctrl+Esc", &key, &mods) && key == GDK_KEY_Escape,
           "Esc alias");

    // BackSpace, Delete
    ASSERT(parse_ok("Alt+BackSpace", &key, &mods) && key == GDK_KEY_BackSpace,
           "BackSpace key");
    ASSERT(parse_ok("Alt+backspace", &key, &mods) && key == GDK_KEY_BackSpace,
           "backspace (lowercase)");
    ASSERT(parse_ok("Ctrl+Delete", &key, &mods) && key == GDK_KEY_Delete,
           "Delete key");
    ASSERT(parse_ok("Ctrl+Del", &key, &mods) && key == GDK_KEY_Delete,
           "Del alias");

    // Tab, Space
    ASSERT(parse_ok("Alt+Tab", &key, &mods) && key == GDK_KEY_Tab,
           "Tab key");
    ASSERT(parse_ok("Ctrl+Space", &key, &mods) && key == GDK_KEY_space,
           "Space key");

    // Arrow keys
    ASSERT(parse_ok("Alt+Up", &key, &mods) && key == GDK_KEY_Up, "Up key");
    ASSERT(parse_ok("Alt+Down", &key, &mods) && key == GDK_KEY_Down, "Down key");
    ASSERT(parse_ok("Alt+Left", &key, &mods) && key == GDK_KEY_Left, "Left key");
    ASSERT(parse_ok("Alt+Right", &key, &mods) && key == GDK_KEY_Right, "Right key");

    // Home, End, Page keys
    ASSERT(parse_ok("Ctrl+Home", &key, &mods) && key == GDK_KEY_Home, "Home key");
    ASSERT(parse_ok("Ctrl+End", &key, &mods) && key == GDK_KEY_End, "End key");
    ASSERT(parse_ok("Ctrl+PageUp", &key, &mods) && key == GDK_KEY_Page_Up, "PageUp key");
    ASSERT(parse_ok("Ctrl+PageDown", &key, &mods) && key == GDK_KEY_Page_Down, "PageDown key");

    // Function keys
    ASSERT(parse_ok("F1", &key, &mods) && key == GDK_KEY_F1, "F1 standalone");
    ASSERT(parse_ok("Ctrl+F5", &key, &mods) && key == GDK_KEY_F5, "Ctrl+F5");
    ASSERT(parse_ok("Alt+F12", &key, &mods) && key == GDK_KEY_F12, "Alt+F12");

    // Single character keys
    ASSERT(parse_ok("Ctrl+a", &key, &mods), "Single char 'a'");
    ASSERT(parse_ok("Alt+z", &key, &mods), "Single char 'z'");

    // Punctuation aliases
    ASSERT(parse_ok("Alt+grave", &key, &mods) && key == GDK_KEY_grave, "grave key");
    ASSERT(parse_ok("Alt+backslash", &key, &mods) && key == GDK_KEY_backslash, "backslash key");
}

static void test_numpad_keys(void) {
    printf("\n--- NumPad keys ---\n");
    guint key; GdkModifierType mods;

    ASSERT(parse_ok("KP_0", &key, &mods) && key == GDK_KEY_KP_0,
           "KP_0");
    ASSERT(parse_ok("kp_1", &key, &mods) && key == GDK_KEY_KP_1,
           "kp_1");
    ASSERT(parse_ok("Numpad2", &key, &mods) && key == GDK_KEY_KP_2,
           "Numpad2");
    ASSERT(parse_ok("Ctrl+KP_Add", &key, &mods) &&
           (mods & GDK_CONTROL_MASK) && key == GDK_KEY_KP_Add,
           "Ctrl+KP_Add");
    ASSERT(parse_ok("Ctrl+numpad_enter", &key, &mods) &&
           (mods & GDK_CONTROL_MASK) && key == GDK_KEY_KP_Enter,
           "Ctrl+numpad_enter");
}

static void test_multi_modifier(void) {
    printf("\n--- Multi-modifier combos ---\n");
    guint key; GdkModifierType mods;

    ASSERT(parse_ok("Ctrl+Shift+a", &key, &mods) &&
           (mods & GDK_CONTROL_MASK) && (mods & GDK_SHIFT_MASK),
           "Ctrl+Shift+a");
    ASSERT(parse_ok("Ctrl+Alt+Delete", &key, &mods) &&
           (mods & GDK_CONTROL_MASK) && (mods & GDK_MOD1_MASK) &&
           key == GDK_KEY_Delete,
           "Ctrl+Alt+Delete");
    ASSERT(parse_ok("Super+Shift+F5", &key, &mods) &&
           (mods & GDK_SUPER_MASK) && (mods & GDK_SHIFT_MASK) &&
           key == GDK_KEY_F5,
           "Super+Shift+F5");
}

static void test_error_messages(void) {
    printf("\n--- Error messages ---\n");
    char err[256];

    // Typo in modifier - should suggest
    ASSERT(parse_fail("Crtl+a", err, sizeof(err)) && strstr(err, "Did you mean") && strstr(err, "Ctrl"),
           "Typo 'Crtl' suggests 'Ctrl'");
    ASSERT(parse_fail("Atl+a", err, sizeof(err)) && strstr(err, "Did you mean") && strstr(err, "Alt"),
           "Typo 'Atl' suggests 'Alt'");
    ASSERT(parse_fail("Supr+a", err, sizeof(err)) && strstr(err, "Did you mean") && strstr(err, "Super"),
           "Typo 'Supr' suggests 'Super'");
    ASSERT(parse_fail("Shfit+a", err, sizeof(err)) && strstr(err, "Did you mean") && strstr(err, "Shift"),
           "Typo 'Shfit' suggests 'Shift'");

    // Unknown key suggests close match
    ASSERT(parse_fail("Alt+Tba", err, sizeof(err)) && strstr(err, "Did you mean") && strstr(err, "Tab"),
           "Typo 'Tba' suggests 'Tab'");
    ASSERT(parse_fail("Alt+Esacpe", err, sizeof(err)) && strstr(err, "Did you mean"),
           "Typo 'Esacpe' suggests something");
    ASSERT(parse_fail("Alt+Retrn", err, sizeof(err)) && strstr(err, "Did you mean") && strstr(err, "Return"),
           "Typo 'Retrn' suggests 'Return'");

    // Modifier used as key
    ASSERT(parse_fail("Ctrl", err, sizeof(err)) && strstr(err, "modifier"),
           "'Ctrl' alone detected as modifier-not-key");

    // Completely unknown
    ASSERT(parse_fail("Alt+xyzzy123", err, sizeof(err)) && strstr(err, "Unknown key"),
           "Totally unknown key gives generic message");

    // Empty string
    ASSERT(parse_fail("", err, sizeof(err)) && strstr(err, "Empty"),
           "Empty string gives error");

    // NULL error buffer still works (no crash)
    {
        guint key; GdkModifierType mods;
        gboolean ok = parse_shortcut_with_error("Crtl+a", &key, &mods, NULL, 0);
        tests_run++;
        if (!ok) { tests_passed++; printf("  PASS: NULL error buffer doesn't crash\n"); }
        else { printf("  FAIL: NULL error buffer doesn't crash (line %d)\n", __LINE__); }
    }
}

static void test_backward_compat(void) {
    printf("\n--- Backward compatibility (parse_shortcut) ---\n");
    guint key; GdkModifierType mods;

    ASSERT(parse_shortcut("Alt+Tab", &key, &mods) && key == GDK_KEY_Tab && (mods & GDK_MOD1_MASK),
           "parse_shortcut still works for Alt+Tab");
    ASSERT(parse_shortcut("Super+w", &key, &mods) && (mods & GDK_SUPER_MASK),
           "parse_shortcut still works for Super+w");
    ASSERT(!parse_shortcut("Crtl+a", &key, &mods),
           "parse_shortcut returns FALSE on typo");
    ASSERT(!parse_shortcut(NULL, &key, &mods),
           "parse_shortcut handles NULL");
}

static void test_whitespace_handling(void) {
    printf("\n--- Whitespace handling ---\n");
    guint key; GdkModifierType mods;

    ASSERT(parse_ok("Ctrl + a", &key, &mods) && (mods & GDK_CONTROL_MASK),
           "Ctrl + a (spaces around +)");
    ASSERT(parse_ok("  Alt  +  Tab  ", &key, &mods) && key == GDK_KEY_Tab,
           "Extra whitespace handled");
}

static void test_hotkey_canonicalization(void) {
    printf("\n--- Hotkey canonicalization ---\n");
    char out[128];

    ASSERT(canonicalize_ok("alt+tab", out, sizeof(out)) && strcmp(out, "Mod1+Tab") == 0,
           "alt+tab -> Mod1+Tab");
    ASSERT(canonicalize_ok("ctrl+enter", out, sizeof(out)) && strcmp(out, "Control+Return") == 0,
           "ctrl+enter -> Control+Return");
    ASSERT(canonicalize_ok("win+x", out, sizeof(out)) && strcmp(out, "Mod4+x") == 0,
           "win+x -> Mod4+x");
    ASSERT(canonicalize_ok("Shift+Backspace", out, sizeof(out)) && strcmp(out, "Shift+BackSpace") == 0,
           "Shift+Backspace -> Shift+BackSpace");
    ASSERT(canonicalize_ok("mod1+grave", out, sizeof(out)) && strcmp(out, "Mod1+grave") == 0,
           "mod1+grave -> Mod1+grave");
    ASSERT(canonicalize_ok("KP_1", out, sizeof(out)) && strcmp(out, "KP_1") == 0,
           "KP_1");
    ASSERT(canonicalize_ok("Ctrl+kp_enter", out, sizeof(out)) && strcmp(out, "Control+KP_Enter") == 0,
           "Ctrl+kp_enter -> Control+KP_Enter");
}

static void test_hotkey_canonicalization_errors(void) {
    printf("\n--- Hotkey canonicalization errors ---\n");
    char err[256];

    ASSERT(canonicalize_fail("Meta+x", err, sizeof(err)) && strstr(err, "not supported"),
           "Meta rejected for XGrabKey hotkeys");
    ASSERT(canonicalize_fail("Hyper+x", err, sizeof(err)) && strstr(err, "not supported"),
           "Hyper rejected for XGrabKey hotkeys");
    ASSERT(canonicalize_fail("Mod2+x", err, sizeof(err)) && strstr(err, "Unknown modifier"),
           "Mod2 still rejected by user parser");
    ASSERT(canonicalize_fail("Alt+NoSuchKey", err, sizeof(err)) && strstr(err, "Unknown key"),
           "Unknown key rejected");
}

static void test_hotkey_event_canonicalization(void) {
    printf("\n--- Hotkey event canonicalization ---\n");
    char out[128];

    GdkEventKey event = {0};
    event.state = GDK_CONTROL_MASK;
    event.keyval = GDK_KEY_Tab;
    ASSERT(canonicalize_event_ok(&event, out, sizeof(out)) && strcmp(out, "Control+Tab") == 0,
           "Event Ctrl+Tab -> Control+Tab");

    event = (GdkEventKey){0};
    event.state = GDK_MOD1_MASK;
    event.keyval = GDK_KEY_q;
    ASSERT(canonicalize_event_ok(&event, out, sizeof(out)) && strcmp(out, "Mod1+q") == 0,
           "Event Mod1+q -> Mod1+q");

    event = (GdkEventKey){0};
    event.state = GDK_SUPER_MASK;
    event.keyval = GDK_KEY_Escape;
    ASSERT(canonicalize_event_ok(&event, out, sizeof(out)) && strcmp(out, "Mod4+Escape") == 0,
           "Event Super+Escape -> Mod4+Escape");

    event = (GdkEventKey){0};
    event.state = 0;
    event.keyval = GDK_KEY_Control_L;
    char err[256];
    ASSERT(canonicalize_event_fail(&event, err, sizeof(err)) && strstr(err, "Modifier"),
           "Modifier-only event rejected");
}

int main(void) {
    printf("=== parse_shortcut tests ===\n");

    test_case_insensitive_modifiers();
    test_modifier_aliases();
    test_key_aliases();
    test_multi_modifier();
    test_numpad_keys();
    test_error_messages();
    test_backward_compat();
    test_whitespace_handling();
    test_hotkey_canonicalization();
    test_hotkey_canonicalization_errors();
    test_hotkey_event_canonicalization();

    printf("\n=== Summary: %d/%d passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
