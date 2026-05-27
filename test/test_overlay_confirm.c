#include <gdk/gdk.h>
#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "ui/overlay_confirm.h"

static int tests_run = 0;
static int tests_passed = 0;

static int g_show_overlay_calls = 0;
static int g_hide_overlay_calls = 0;
static int g_confirm_calls = 0;
static int g_confirm_calls_alt = 0;

#define ASSERT_TRUE(name, cond) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
        printf("PASS: %s\n", name); \
    } else { \
        printf("FAIL: %s (line %d)\n", name, __LINE__); \
    } \
} while (0)

void show_overlay(AppData *app, OverlayType type, gpointer data) {
    (void)data;
    g_show_overlay_calls++;
    app->overlay_active = TRUE;
    app->current_overlay = type;
}

void hide_overlay(AppData *app) {
    g_hide_overlay_calls++;
    clear_confirm_overlay_state(app);
    app->overlay_active = FALSE;
    app->current_overlay = OVERLAY_NONE;
}

static void on_confirm(AppData *app) {
    (void)app;
    g_confirm_calls++;
}

static void on_confirm_alt(AppData *app) {
    (void)app;
    g_confirm_calls_alt++;
}

static GdkEventKey make_key(guint keyval, GdkModifierType state) {
    GdkEventKey event;
    memset(&event, 0, sizeof(event));
    event.keyval = keyval;
    event.state = state;
    return event;
}

static void reset_state(AppData *app) {
    memset(app, 0, sizeof(*app));
    g_show_overlay_calls = 0;
    g_hide_overlay_calls = 0;
    g_confirm_calls = 0;
    g_confirm_calls_alt = 0;
}

static void test_show_confirm_overlay_marks_active_and_copies_strings(void) {
    AppData app;
    reset_state(&app);
    char title[] = "Delete?";
    char info[] = "Info";

    show_confirm_overlay(&app, title, info, on_confirm);

    ASSERT_TRUE("show sets active", app.confirm_overlay.active == TRUE);
    ASSERT_TRUE("show stores copies", app.confirm_overlay.title != title && app.confirm_overlay.info != info);
    ASSERT_TRUE("show stores exact values", strcmp(app.confirm_overlay.title, title) == 0 && strcmp(app.confirm_overlay.info, info) == 0);
    ASSERT_TRUE("show routes overlay", app.current_overlay == OVERLAY_CONFIRM && g_show_overlay_calls == 1);
}

static void test_y_key_invokes_on_confirm_and_hides(void) {
    AppData app;
    reset_state(&app);
    show_confirm_overlay(&app, "t", "i", on_confirm);
    GdkEventKey key = make_key(GDK_KEY_y, 0);
    ASSERT_TRUE("y handled", handle_confirm_overlay_key_press(&app, &key) == TRUE);
    ASSERT_TRUE("y invokes callback", g_confirm_calls == 1);
    ASSERT_TRUE("y hides overlay", g_hide_overlay_calls == 1 && app.confirm_overlay.active == FALSE);
}

static void test_Y_uppercase_invokes_on_confirm(void) {
    AppData app;
    reset_state(&app);
    show_confirm_overlay(&app, "t", "i", on_confirm);
    GdkEventKey key = make_key(GDK_KEY_Y, 0);
    ASSERT_TRUE("Y handled", handle_confirm_overlay_key_press(&app, &key) == TRUE);
    ASSERT_TRUE("Y invokes callback", g_confirm_calls == 1);
}

static void test_ctrl_d_invokes_on_confirm(void) {
    AppData app;
    reset_state(&app);
    show_confirm_overlay(&app, "t", "i", on_confirm);
    GdkEventKey key = make_key(GDK_KEY_d, GDK_CONTROL_MASK);
    ASSERT_TRUE("Ctrl+D handled", handle_confirm_overlay_key_press(&app, &key) == TRUE);
    ASSERT_TRUE("Ctrl+D invokes callback", g_confirm_calls == 1);
}

static void test_delete_key_invokes_on_confirm(void) {
    AppData app;
    reset_state(&app);
    show_confirm_overlay(&app, "t", "i", on_confirm);
    GdkEventKey key = make_key(GDK_KEY_Delete, 0);
    ASSERT_TRUE("Delete handled", handle_confirm_overlay_key_press(&app, &key) == TRUE);
    ASSERT_TRUE("Delete invokes callback", g_confirm_calls == 1);
}

static void test_kp_delete_invokes_on_confirm(void) {
    AppData app;
    reset_state(&app);
    show_confirm_overlay(&app, "t", "i", on_confirm);
    GdkEventKey key = make_key(GDK_KEY_KP_Delete, 0);
    ASSERT_TRUE("KP_Delete handled", handle_confirm_overlay_key_press(&app, &key) == TRUE);
    ASSERT_TRUE("KP_Delete invokes callback", g_confirm_calls == 1);
}

static void test_n_key_hides_without_invoking(void) {
    AppData app;
    reset_state(&app);
    show_confirm_overlay(&app, "t", "i", on_confirm);
    GdkEventKey key = make_key(GDK_KEY_n, 0);
    ASSERT_TRUE("n handled", handle_confirm_overlay_key_press(&app, &key) == TRUE);
    ASSERT_TRUE("n does not invoke callback", g_confirm_calls == 0);
    ASSERT_TRUE("n hides", g_hide_overlay_calls == 1);
}

static void test_N_uppercase_hides_without_invoking(void) {
    AppData app;
    reset_state(&app);
    show_confirm_overlay(&app, "t", "i", on_confirm);
    GdkEventKey key = make_key(GDK_KEY_N, 0);
    ASSERT_TRUE("N handled", handle_confirm_overlay_key_press(&app, &key) == TRUE);
    ASSERT_TRUE("N does not invoke callback", g_confirm_calls == 0);
}

static void test_esc_hides_without_invoking(void) {
    AppData app;
    reset_state(&app);
    show_confirm_overlay(&app, "t", "i", on_confirm);
    GdkEventKey key = make_key(GDK_KEY_Escape, 0);
    ASSERT_TRUE("Esc handled", handle_confirm_overlay_key_press(&app, &key) == TRUE);
    ASSERT_TRUE("Esc does not invoke callback", g_confirm_calls == 0);
}

static void test_unrelated_key_does_not_dismiss(void) {
    AppData app;
    reset_state(&app);
    show_confirm_overlay(&app, "t", "i", on_confirm);
    GdkEventKey key = make_key(GDK_KEY_a, 0);
    ASSERT_TRUE("unrelated key not handled", handle_confirm_overlay_key_press(&app, &key) == FALSE);
    ASSERT_TRUE("unrelated key keeps overlay", app.confirm_overlay.active == TRUE && g_hide_overlay_calls == 0);
}

static void test_hide_clears_active_flag_and_callback(void) {
    AppData app;
    reset_state(&app);
    show_confirm_overlay(&app, "t", "i", on_confirm);
    hide_overlay(&app);
    ASSERT_TRUE("hide clears active", app.confirm_overlay.active == FALSE);
    ASSERT_TRUE("hide clears callback", app.confirm_overlay.on_confirm == NULL);
}

static void test_show_replaces_pending_overlay_drops_prior_callback(void) {
    AppData app;
    reset_state(&app);
    show_confirm_overlay(&app, "first", "one", on_confirm);
    show_confirm_overlay(&app, "second", "two", on_confirm_alt);
    ASSERT_TRUE("second show hides previous", g_hide_overlay_calls == 1);
    ASSERT_TRUE("second show updates callback", app.confirm_overlay.on_confirm == on_confirm_alt);
    ASSERT_TRUE("second show keeps first callback uncalled", g_confirm_calls == 0);
}

static void test_hide_frees_title_and_info_strings(void) {
    AppData app;
    reset_state(&app);
    show_confirm_overlay(&app, "title", "info", on_confirm);
    hide_overlay(&app);
    ASSERT_TRUE("hide clears title pointer", app.confirm_overlay.title == NULL);
    ASSERT_TRUE("hide clears info pointer", app.confirm_overlay.info == NULL);
}

static void test_long_info_string_not_truncated(void) {
    AppData app;
    reset_state(&app);
    char info[4097];
    memset(info, 'x', sizeof(info) - 1);
    info[sizeof(info) - 1] = '\0';
    show_confirm_overlay(&app, "title", info, on_confirm);
    ASSERT_TRUE("long info preserved", strlen(app.confirm_overlay.info) == strlen(info) &&
                strcmp(app.confirm_overlay.info, info) == 0);
}

int main(void) {
    printf("Overlay confirm tests\n");
    printf("=====================\n\n");

    test_show_confirm_overlay_marks_active_and_copies_strings();
    test_y_key_invokes_on_confirm_and_hides();
    test_Y_uppercase_invokes_on_confirm();
    test_ctrl_d_invokes_on_confirm();
    test_delete_key_invokes_on_confirm();
    test_kp_delete_invokes_on_confirm();
    test_n_key_hides_without_invoking();
    test_N_uppercase_hides_without_invoking();
    test_esc_hides_without_invoking();
    test_unrelated_key_does_not_dismiss();
    test_hide_clears_active_flag_and_callback();
    test_show_replaces_pending_overlay_drops_prior_callback();
    test_hide_frees_title_and_info_strings();
    test_long_info_string_not_truncated();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
