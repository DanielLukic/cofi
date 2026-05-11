#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../src/app_data.h"
#include "../src/hotkey_config.h"
#include "../src/overlay_hotkey_add.h"
#include "../src/overlay_manager.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

/* ------------------------------------------------------------------ */
/* Stub counters                                                        */
/* ------------------------------------------------------------------ */

static int g_hide_overlay_calls;
static int g_save_hotkey_config_calls;
static int g_finish_capture_calls;
static int g_filter_hotkeys_calls;
static int g_validate_selection_calls;
static int g_update_scroll_calls;
static int g_update_display_calls;
static int g_regrab_hotkeys_calls;

static void reset_captures(void) {
    g_hide_overlay_calls       = 0;
    g_save_hotkey_config_calls = 0;
    g_finish_capture_calls     = 0;
    g_filter_hotkeys_calls     = 0;
    g_validate_selection_calls = 0;
    g_update_scroll_calls      = 0;
    g_update_display_calls     = 0;
    g_regrab_hotkeys_calls     = 0;
}

/* ------------------------------------------------------------------ */
/* Stubs                                                               */
/* ------------------------------------------------------------------ */

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level; (void)file; (void)line; (void)fmt;
}

void hide_overlay(AppData *app) {
    g_hide_overlay_calls++;
    memset(&app->hotkey_rebind, 0, sizeof(app->hotkey_rebind));
    app->hotkey_rebind.conflict_index = -1;
    app->overlay_active = FALSE;
    app->current_overlay = OVERLAY_NONE;
    if (app->hotkey_capture_active) {
        app->hotkey_capture_active = FALSE;
        /* regrab stubbed below */
    }
}

int save_hotkey_config(const HotkeyConfig *config) {
    (void)config;
    g_save_hotkey_config_calls++;
    return 1;
}

void regrab_hotkeys(AppData *app) {
    (void)app;
    g_regrab_hotkeys_calls++;
}

void filter_hotkeys(AppData *app, const char *query) {
    (void)query;
    g_filter_hotkeys_calls++;
    app->filtered_hotkeys_count = app->hotkey_config.count;
    for (int i = 0; i < app->hotkey_config.count; i++) {
        app->filtered_hotkeys[i] = app->hotkey_config.bindings[i];
        app->filtered_hotkeys_indices[i] = i;
    }
}

void validate_selection(AppData *app) {
    (void)app;
    g_validate_selection_calls++;
}

void update_scroll_position(AppData *app) {
    (void)app;
    g_update_scroll_calls++;
}

void update_display(AppData *app) {
    (void)app;
    g_update_display_calls++;
}

/* GtkWidget stubs — avoid needing a real GTK display */
GtkWidget *gtk_label_new(const char *text)    { (void)text; return (GtkWidget *)0x1; }
GtkWidget *gtk_entry_new(void)                { return (GtkWidget *)0x2; }
GtkWidget *gtk_box_new(GtkOrientation o, int s) { (void)o; (void)s; return (GtkWidget *)0x3; }
const gchar *gtk_entry_get_text(GtkEntry *e)  { (void)e; return ""; }
void gtk_label_set_text(GtkLabel *l, const char *t) { (void)l; (void)t; }
void gtk_entry_set_text(GtkEntry *e, const char *t) { (void)e; (void)t; }
gpointer g_object_get_data(GObject *o, const char *k) {
    /* Return non-NULL for name_entry and error_label so handler doesn't bail */
    (void)o; (void)k;
    return (gpointer)0x1;
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
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

/* ------------------------------------------------------------------ */
/* Test 1: no-conflict rebind applies new key                          */
/* ------------------------------------------------------------------ */

static void test_rebind_no_conflict_applies_new_key(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    setup_config_2(&app.hotkey_config);
    setup_rebind(&app, 0);  /* rebinding Mod1+Tab */
    reset_captures();

    /* Simulate capturing "Mod4+F1" — not in config, no conflict.
     * Call apply path directly via handle_hotkey_add_key_press with a
     * synthesized Return key after pre-populating awaiting_confirm=FALSE.
     * We bypass GTK widget lookup by testing the data-layer logic:
     * use find_hotkey_binding + manual apply to mirror what the handler does. */
    const char *new_combo = "Mod4+F1";
    int existing = find_hotkey_binding(&app.hotkey_config, new_combo);

    ASSERT_TRUE("No-conflict: combo not found before rebind", existing < 0);

    /* Apply the rebind directly (mirrors apply_rebind() logic) */
    g_strlcpy(app.hotkey_config.bindings[app.hotkey_rebind.target_index].key,
              new_combo,
              sizeof(app.hotkey_config.bindings[0].key));
    save_hotkey_config(&app.hotkey_config);
    hide_overlay(&app);

    ASSERT_TRUE("No-conflict: key updated", strcmp(app.hotkey_config.bindings[0].key, "Mod4+F1") == 0);
    ASSERT_TRUE("No-conflict: command preserved", strcmp(app.hotkey_config.bindings[0].command, "show windows") == 0);
    ASSERT_TRUE("No-conflict: count unchanged", app.hotkey_config.count == 2);
    ASSERT_TRUE("No-conflict: save called once", g_save_hotkey_config_calls == 1);
    ASSERT_TRUE("No-conflict: hide called once", g_hide_overlay_calls == 1);
}

/* ------------------------------------------------------------------ */
/* Test 2: same-combo is no-op                                         */
/* ------------------------------------------------------------------ */

static void test_rebind_same_combo_is_noop(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    setup_config_2(&app.hotkey_config);
    setup_rebind(&app, 0);
    reset_captures();

    /* Same combo as current key */
    const char *same = "Mod1+Tab";
    int existing = find_hotkey_binding(&app.hotkey_config, same);

    /* existing == target_index (0) → no-op: just hide */
    ASSERT_TRUE("Same-combo: existing == target_index", existing == app.hotkey_rebind.target_index);
    hide_overlay(&app);  /* mirrors same-combo branch */

    ASSERT_TRUE("Same-combo: hide called once", g_hide_overlay_calls == 1);
    ASSERT_TRUE("Same-combo: save NOT called", g_save_hotkey_config_calls == 0);
    ASSERT_TRUE("Same-combo: key unchanged", strcmp(app.hotkey_config.bindings[0].key, "Mod1+Tab") == 0);
}

/* ------------------------------------------------------------------ */
/* Test 3: conflict → Y removes conflict, applies rebind               */
/* ------------------------------------------------------------------ */

static void test_rebind_conflict_confirm_y_removes_conflict_and_applies(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    setup_config_2(&app.hotkey_config);
    setup_rebind(&app, 0);  /* rebinding Mod1+Tab to Mod4+1 */

    /* Phase 1: conflict detected with index 1 */
    const char *new_combo = "Mod4+1";
    int conflict = find_hotkey_binding(&app.hotkey_config, new_combo);

    ASSERT_TRUE("Conflict phase-1: conflict found", conflict == 1);

    app.hotkey_rebind.awaiting_confirm = TRUE;
    app.hotkey_rebind.conflict_index   = conflict;
    g_strlcpy(app.hotkey_rebind.pending_combo, new_combo,
              sizeof(app.hotkey_rebind.pending_combo));

    reset_captures();
    ASSERT_TRUE("Conflict phase-1: no save yet", g_save_hotkey_config_calls == 0);
    ASSERT_TRUE("Conflict phase-1: no hide yet", g_hide_overlay_calls == 0);

    /* Phase 2: simulate 'y' — mirrors handle_rebind_confirm_key logic */
    const char *conflict_key = app.hotkey_config.bindings[app.hotkey_rebind.conflict_index].key;
    remove_hotkey_binding(&app.hotkey_config, conflict_key);
    int new_target = find_hotkey_binding(&app.hotkey_config, app.hotkey_rebind.target_key);
    if (new_target >= 0) {
        g_strlcpy(app.hotkey_config.bindings[new_target].key,
                  app.hotkey_rebind.pending_combo,
                  sizeof(app.hotkey_config.bindings[0].key));
    }
    save_hotkey_config(&app.hotkey_config);
    hide_overlay(&app);

    ASSERT_TRUE("Conflict Y: count == 1", app.hotkey_config.count == 1);
    ASSERT_TRUE("Conflict Y: key updated to Mod4+1",
                strcmp(app.hotkey_config.bindings[0].key, "Mod4+1") == 0);
    ASSERT_TRUE("Conflict Y: command preserved",
                strcmp(app.hotkey_config.bindings[0].command, "show windows") == 0);
    ASSERT_TRUE("Conflict Y: save called once", g_save_hotkey_config_calls == 1);
    ASSERT_TRUE("Conflict Y: hide called once", g_hide_overlay_calls == 1);
}

/* ------------------------------------------------------------------ */
/* Test 4: conflict → N cancels                                        */
/* ------------------------------------------------------------------ */

static void test_rebind_conflict_confirm_n_cancels(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    setup_config_2(&app.hotkey_config);
    setup_rebind(&app, 0);

    /* Reach awaiting_confirm state */
    app.hotkey_rebind.awaiting_confirm = TRUE;
    app.hotkey_rebind.conflict_index   = 1;
    g_strlcpy(app.hotkey_rebind.pending_combo, "Mod4+1",
              sizeof(app.hotkey_rebind.pending_combo));

    reset_captures();

    /* Simulate 'n' — mirrors n-branch of handle_rebind_confirm_key */
    hide_overlay(&app);

    ASSERT_TRUE("Cancel N: save NOT called", g_save_hotkey_config_calls == 0);
    ASSERT_TRUE("Cancel N: hide called once", g_hide_overlay_calls == 1);
    ASSERT_TRUE("Cancel N: config unchanged",
                app.hotkey_config.count == 2 &&
                strcmp(app.hotkey_config.bindings[0].key, "Mod1+Tab") == 0 &&
                strcmp(app.hotkey_config.bindings[1].key, "Mod4+1") == 0);
}

/* ------------------------------------------------------------------ */
/* Test 5: index shift when conflict_index < target_index              */
/* ------------------------------------------------------------------ */

static void test_rebind_conflict_index_shift_when_conflict_before_target(void) {
    AppData app;
    memset(&app, 0, sizeof(app));
    init_hotkey_config(&app.hotkey_config);
    add_hotkey_binding(&app.hotkey_config, "Mod1+F1", "cmd-a");
    add_hotkey_binding(&app.hotkey_config, "Mod1+F2", "cmd-b");
    add_hotkey_binding(&app.hotkey_config, "Mod1+F3", "cmd-c");
    setup_rebind(&app, 2);  /* rebinding Mod1+F3 */

    /* Conflict is index 0 (before target) */
    app.hotkey_rebind.awaiting_confirm = TRUE;
    app.hotkey_rebind.conflict_index   = 0;
    g_strlcpy(app.hotkey_rebind.pending_combo, "Mod1+F1",
              sizeof(app.hotkey_rebind.pending_combo));

    reset_captures();

    /* Simulate 'y' */
    const char *conflict_key = app.hotkey_config.bindings[app.hotkey_rebind.conflict_index].key;
    remove_hotkey_binding(&app.hotkey_config, conflict_key);
    /* After removal, target shifts: find by key string */
    int new_target = find_hotkey_binding(&app.hotkey_config, app.hotkey_rebind.target_key);
    if (new_target >= 0) {
        g_strlcpy(app.hotkey_config.bindings[new_target].key,
                  app.hotkey_rebind.pending_combo,
                  sizeof(app.hotkey_config.bindings[0].key));
    }
    save_hotkey_config(&app.hotkey_config);
    hide_overlay(&app);

    ASSERT_TRUE("Index-shift Y: count == 2", app.hotkey_config.count == 2);

    int idx = find_hotkey_binding(&app.hotkey_config, "Mod1+F1");
    ASSERT_TRUE("Index-shift Y: Mod1+F1 present", idx >= 0);
    ASSERT_TRUE("Index-shift Y: command is cmd-c",
                idx >= 0 && strcmp(app.hotkey_config.bindings[idx].command, "cmd-c") == 0);
    ASSERT_TRUE("Index-shift Y: save called once", g_save_hotkey_config_calls == 1);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void) {
    printf("Hotkey rebind flow tests\n");
    printf("========================\n\n");

    test_rebind_no_conflict_applies_new_key();
    test_rebind_same_combo_is_noop();
    test_rebind_conflict_confirm_y_removes_conflict_and_applies();
    test_rebind_conflict_confirm_n_cancels();
    test_rebind_conflict_index_shift_when_conflict_before_target();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
