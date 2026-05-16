#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../src/app_data.h"
#include "../src/hotkey_config.h"
#include "../src/overlay_hotkey_add.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

/* ------------------------------------------------------------------ */
/* Stub counters                                                       */
/* ------------------------------------------------------------------ */

static int g_hide_overlay_calls;
static int g_save_hotkey_config_calls;
static int g_filter_hotkeys_calls;
static int g_validate_selection_calls;
static int g_update_scroll_calls;
static int g_update_display_calls;
static int g_regrab_hotkeys_calls;

static void reset_captures(void) {
    g_hide_overlay_calls       = 0;
    g_save_hotkey_config_calls = 0;
    g_filter_hotkeys_calls     = 0;
    g_validate_selection_calls = 0;
    g_update_scroll_calls      = 0;
    g_update_display_calls     = 0;
    g_regrab_hotkeys_calls     = 0;
}

/* ------------------------------------------------------------------ */
/* Stubs for collaborators of the rebind helpers.                      */
/* ------------------------------------------------------------------ */

void __wrap_log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

void __wrap_hide_overlay(AppData *app) {
    g_hide_overlay_calls++;
    memset(&app->hotkey_rebind, 0, sizeof(app->hotkey_rebind));
    app->hotkey_rebind.conflict_index = -1;
    app->overlay_active = FALSE;
    app->current_overlay = OVERLAY_NONE;
    if (app->hotkey_capture_active) {
        app->hotkey_capture_active = FALSE;
    }
}

int __wrap_save_hotkey_config(const HotkeyConfig *config) {
    (void)config;
    g_save_hotkey_config_calls++;
    return 1;
}

void __wrap_regrab_hotkeys(AppData *app) {
    (void)app;
    g_regrab_hotkeys_calls++;
}

void __wrap_filter_hotkeys(AppData *app, const char *query) {
    (void)query;
    g_filter_hotkeys_calls++;
    app->filtered_hotkeys_count = app->hotkey_config.count;
    for (int i = 0; i < app->hotkey_config.count; i++) {
        app->filtered_hotkeys[i] = app->hotkey_config.bindings[i];
        app->filtered_hotkeys_indices[i] = i;
    }
}

void __wrap_hotkeys_select_key(AppData *app, const char *key) {
    (void)key;
    app->selection.provider_index = 0;
}

void __wrap_validate_selection(AppData *app) {
    (void)app;
    g_validate_selection_calls++;
}

void __wrap_update_scroll_position(AppData *app) {
    (void)app;
    g_update_scroll_calls++;
}

void __wrap_update_display(AppData *app) {
    (void)app;
    g_update_display_calls++;
}

const gchar *__wrap_gtk_entry_get_text(GtkEntry *e) { (void)e; return ""; }

/* Unreached by rebind helpers, but referenced by sibling code in the same .o.
 * Linker requires definitions; tests never exercise these paths. */
gboolean canonicalize_hotkey_event(const GdkEventKey *event, char *out, size_t out_size,
                                   char *err, size_t err_size) {
    (void)event; (void)out; (void)out_size; (void)err; (void)err_size;
    return FALSE;
}

gboolean canonicalize_hotkey_shortcut(const char *input, char *out, size_t out_size,
                                      char *err, size_t err_size) {
    (void)input; (void)out; (void)out_size; (void)err; (void)err_size;
    return FALSE;
}

/* ------------------------------------------------------------------ */
/* Test fixtures                                                       */
/* ------------------------------------------------------------------ */

static void setup_config_2(HotkeyConfig *cfg) {
    init_hotkey_config(cfg);
    add_hotkey_binding(cfg, "Mod1+Tab", "show windows");
    add_hotkey_binding(cfg, "Mod4+1",   "jw 1");
}

static void setup_rebind(AppData *app, int target_index) {
    app->hotkey_rebind.active           = TRUE;
    app->hotkey_rebind.target_index     = target_index;
    app->hotkey_rebind.awaiting_confirm = FALSE;
    app->hotkey_rebind.conflict_index   = -1;
    g_strlcpy(app->hotkey_rebind.target_key,
              app->hotkey_config.bindings[target_index].key,
              sizeof(app->hotkey_rebind.target_key));
    g_strlcpy(app->hotkey_rebind.target_command,
              app->hotkey_config.bindings[target_index].command,
              sizeof(app->hotkey_rebind.target_command));
    app->hotkey_capture_active = TRUE;
    app->overlay_active        = TRUE;
    app->current_overlay       = OVERLAY_HOTKEY_REBIND;
}

static GdkEventKey make_key(guint keyval) {
    GdkEventKey ev;
    memset(&ev, 0, sizeof(ev));
    ev.keyval = keyval;
    return ev;
}

/* ------------------------------------------------------------------ */
/* Test 1: apply_rebind writes new key, preserves command, persists.   */
/* ------------------------------------------------------------------ */

static void test_apply_rebind_writes_new_key(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    setup_config_2(&app.hotkey_config);
    setup_rebind(&app, 0);
    reset_captures();

    gboolean handled = apply_rebind(&app, "Mod4+F1");

    ASSERT_TRUE("apply_rebind: returns TRUE", handled == TRUE);
    ASSERT_TRUE("apply_rebind: target key updated",
                strcmp(app.hotkey_config.bindings[0].key, "Mod4+F1") == 0);
    ASSERT_TRUE("apply_rebind: command preserved",
                strcmp(app.hotkey_config.bindings[0].command, "show windows") == 0);
    ASSERT_TRUE("apply_rebind: count unchanged", app.hotkey_config.count == 2);
    ASSERT_TRUE("apply_rebind: save called once", g_save_hotkey_config_calls == 1);
    ASSERT_TRUE("apply_rebind: hide called once", g_hide_overlay_calls == 1);
}

/* ------------------------------------------------------------------ */
/* Test 2: show_rebind_conflict sets awaiting_confirm + pending_combo. */
/* ------------------------------------------------------------------ */

static void test_show_rebind_conflict_sets_pending_state(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    setup_config_2(&app.hotkey_config);
    setup_rebind(&app, 0);
    reset_captures();

    gboolean handled = show_rebind_conflict(&app, NULL, "Mod4+1", 1);

    ASSERT_TRUE("conflict: returns TRUE", handled == TRUE);
    ASSERT_TRUE("conflict: awaiting_confirm set", app.hotkey_rebind.awaiting_confirm == TRUE);
    ASSERT_TRUE("conflict: conflict_index recorded", app.hotkey_rebind.conflict_index == 1);
    ASSERT_TRUE("conflict: pending_combo recorded",
                strcmp(app.hotkey_rebind.pending_combo, "Mod4+1") == 0);
    ASSERT_TRUE("conflict: save NOT called", g_save_hotkey_config_calls == 0);
    ASSERT_TRUE("conflict: hide NOT called", g_hide_overlay_calls == 0);
    ASSERT_TRUE("conflict: config unchanged",
                app.hotkey_config.count == 2 &&
                strcmp(app.hotkey_config.bindings[0].key, "Mod1+Tab") == 0);
}

/* ------------------------------------------------------------------ */
/* Test 3: handle_rebind_confirm_key 'y' removes conflict, applies.    */
/* ------------------------------------------------------------------ */

static void test_confirm_y_removes_conflict_and_applies(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    setup_config_2(&app.hotkey_config);
    setup_rebind(&app, 0);

    /* Reach awaiting_confirm via the real show_rebind_conflict() */
    show_rebind_conflict(&app, NULL, "Mod4+1", 1);
    reset_captures();

    GdkEventKey ev = make_key(GDK_KEY_y);
    gboolean handled = handle_rebind_confirm_key(&app, &ev);

    ASSERT_TRUE("confirm Y: returns TRUE", handled == TRUE);
    ASSERT_TRUE("confirm Y: count == 1", app.hotkey_config.count == 1);
    ASSERT_TRUE("confirm Y: key updated to Mod4+1",
                strcmp(app.hotkey_config.bindings[0].key, "Mod4+1") == 0);
    ASSERT_TRUE("confirm Y: command preserved",
                strcmp(app.hotkey_config.bindings[0].command, "show windows") == 0);
    ASSERT_TRUE("confirm Y: save called once", g_save_hotkey_config_calls == 1);
    ASSERT_TRUE("confirm Y: hide called once", g_hide_overlay_calls == 1);
}

/* ------------------------------------------------------------------ */
/* Test 4: handle_rebind_confirm_key 'n' cancels.                      */
/* ------------------------------------------------------------------ */

static void test_confirm_n_cancels(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    setup_config_2(&app.hotkey_config);
    setup_rebind(&app, 0);
    show_rebind_conflict(&app, NULL, "Mod4+1", 1);
    reset_captures();

    GdkEventKey ev = make_key(GDK_KEY_n);
    gboolean handled = handle_rebind_confirm_key(&app, &ev);

    ASSERT_TRUE("confirm N: returns TRUE", handled == TRUE);
    ASSERT_TRUE("confirm N: save NOT called", g_save_hotkey_config_calls == 0);
    ASSERT_TRUE("confirm N: hide called once", g_hide_overlay_calls == 1);
    ASSERT_TRUE("confirm N: config unchanged",
                app.hotkey_config.count == 2 &&
                strcmp(app.hotkey_config.bindings[0].key, "Mod1+Tab") == 0 &&
                strcmp(app.hotkey_config.bindings[1].key, "Mod4+1") == 0);
}

/* ------------------------------------------------------------------ */
/* Test 5: Escape also cancels in confirm state.                       */
/* ------------------------------------------------------------------ */

static void test_confirm_escape_cancels(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    setup_config_2(&app.hotkey_config);
    setup_rebind(&app, 0);
    show_rebind_conflict(&app, NULL, "Mod4+1", 1);
    reset_captures();

    GdkEventKey ev = make_key(GDK_KEY_Escape);
    gboolean handled = handle_rebind_confirm_key(&app, &ev);

    ASSERT_TRUE("confirm Esc: returns TRUE", handled == TRUE);
    ASSERT_TRUE("confirm Esc: save NOT called", g_save_hotkey_config_calls == 0);
    ASSERT_TRUE("confirm Esc: hide called once", g_hide_overlay_calls == 1);
    ASSERT_TRUE("confirm Esc: config unchanged",
                app.hotkey_config.count == 2);
}

/* ------------------------------------------------------------------ */
/* Test 6: confirm_y handles index shift when conflict_index < target. */
/* ------------------------------------------------------------------ */

static void test_confirm_y_index_shift_conflict_before_target(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    init_hotkey_config(&app.hotkey_config);
    add_hotkey_binding(&app.hotkey_config, "Mod1+F1", "cmd-a");
    add_hotkey_binding(&app.hotkey_config, "Mod1+F2", "cmd-b");
    add_hotkey_binding(&app.hotkey_config, "Mod1+F3", "cmd-c");
    setup_rebind(&app, 2);  /* rebinding Mod1+F3 -> cmd-c */

    /* Conflict captured: Mod1+F1 at index 0 (BEFORE target_index 2) */
    show_rebind_conflict(&app, NULL, "Mod1+F1", 0);
    reset_captures();

    GdkEventKey ev = make_key(GDK_KEY_y);
    handle_rebind_confirm_key(&app, &ev);

    ASSERT_TRUE("index shift: count == 2", app.hotkey_config.count == 2);

    int idx = find_hotkey_binding(&app.hotkey_config, "Mod1+F1");
    ASSERT_TRUE("index shift: Mod1+F1 present", idx >= 0);
    ASSERT_TRUE("index shift: Mod1+F1 carries cmd-c (original target's command)",
                idx >= 0 && strcmp(app.hotkey_config.bindings[idx].command, "cmd-c") == 0);
    ASSERT_TRUE("index shift: save called once", g_save_hotkey_config_calls == 1);
    ASSERT_TRUE("index shift: hide called once", g_hide_overlay_calls == 1);
}

/* ------------------------------------------------------------------ */
/* Test 7: other key in confirm state is swallowed, no-op.             */
/* ------------------------------------------------------------------ */

static void test_confirm_other_key_swallowed(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    setup_config_2(&app.hotkey_config);
    setup_rebind(&app, 0);
    show_rebind_conflict(&app, NULL, "Mod4+1", 1);
    reset_captures();

    GdkEventKey ev = make_key(GDK_KEY_x);
    gboolean handled = handle_rebind_confirm_key(&app, &ev);

    ASSERT_TRUE("confirm other-key: returns TRUE (swallowed)", handled == TRUE);
    ASSERT_TRUE("confirm other-key: save NOT called", g_save_hotkey_config_calls == 0);
    ASSERT_TRUE("confirm other-key: hide NOT called", g_hide_overlay_calls == 0);
    ASSERT_TRUE("confirm other-key: still awaiting_confirm",
                app.hotkey_rebind.awaiting_confirm == TRUE);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void) {
    printf("Hotkey rebind flow tests\n");
    printf("========================\n\n");

    test_apply_rebind_writes_new_key();
    test_show_rebind_conflict_sets_pending_state();
    test_confirm_y_removes_conflict_and_applies();
    test_confirm_n_cancels();
    test_confirm_escape_cancels();
    test_confirm_y_index_shift_conflict_before_target();
    test_confirm_other_key_swallowed();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
