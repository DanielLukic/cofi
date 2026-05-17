#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>

#include "../src/app_data.h"
#include "../src/cofi_tab_provider.h"
#include "../src/slot_store.h"

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

static int g_exit_command_mode_calls;
static int g_hide_window_calls;
static int g_surface_tab_calls;
static TabMode g_last_surface_tab;
static int g_switch_name_calls;
static char g_last_switch_name[MAX_SINK_NAME_LEN];
static gboolean g_switch_result = TRUE;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

void sinks_refresh_async(AppData *app) {
    (void)app;
}

void sinks_filter(AppData *app, const char *filter) {
    (void)app;
    (void)filter;
}

gboolean sinks_switch_name(AppData *app, const char *sink_name) {
    (void)app;
    g_switch_name_calls++;
    g_strlcpy(g_last_switch_name, sink_name ? sink_name : "",
              sizeof(g_last_switch_name));
    return g_switch_result;
}

void exit_command_mode(AppData *app) {
    (void)app;
    g_exit_command_mode_calls++;
}

void hide_window(AppData *app) {
    (void)app;
    g_hide_window_calls++;
}

void surface_tab(AppData *app, TabMode tab) {
    (void)app;
    g_surface_tab_calls++;
    g_last_surface_tab = tab;
}

#include "../src/cofi_tab_provider.c"
#include "../src/slot_store.c"
#include "../src/sinks_provider.c"

static const CofiTabProvider *registered_sinks_provider(void) {
    cofi_registry_reset();
    sinks_provider_register();
    return cofi_get_provider_for_tab(TAB_SINKS);
}

static void reset_capture(void) {
    g_exit_command_mode_calls = 0;
    g_hide_window_calls = 0;
    g_surface_tab_calls = 0;
    g_last_surface_tab = TAB_WINDOWS;
    g_switch_name_calls = 0;
    g_last_switch_name[0] = '\0';
    g_switch_result = TRUE;
}

static void setup_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    app->current_tab = TAB_WINDOWS;
    app->textbuffer = gtk_text_buffer_new(NULL);
    init_sinks_mode(&app->sinks_mode);
    slot_store_init(&app->harpoon.store);
}

static void seed_sink(AppData *app) {
    app->sinks_mode.sink_count = 1;
    app->sinks_mode.filtered_count = 1;
    app->sinks_mode.filtered_indices[0] = 0;
    g_strlcpy(app->sinks_mode.sinks[0].name,
              "alsa_output.usb-DAC.analog-stereo",
              sizeof(app->sinks_mode.sinks[0].name));
    g_strlcpy(app->sinks_mode.sinks[0].description,
              "USB DAC Headphones",
              sizeof(app->sinks_mode.sinks[0].description));
}

static void teardown_app(AppData *app) {
    if (app->textbuffer) {
        g_object_unref(app->textbuffer);
        app->textbuffer = NULL;
    }
    slot_store_free(&app->harpoon.store);
}

static void read_textbuffer(GtkTextBuffer *buffer, char *out, size_t out_size) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    g_strlcpy(out, text ? text : "", out_size);
    g_free(text);
}

static void test_registered_command_metadata(void) {
    const CofiTabProvider *p = registered_sinks_provider();

    ASSERT_TRUE("sinks command registered", p != NULL);
    ASSERT_TRUE("sinks command primary", p && strcmp(p->primary_cmd, "sinks") == 0);
    ASSERT_TRUE("sinks command alias", p && p->aliases && strcmp(p->aliases[0], "sink") == 0);
    ASSERT_TRUE("sinks command help",
                p && strcmp(p->command_help_format, "sinks, sink [@SLOT|SINK]") == 0);
    ASSERT_TRUE("sinks command description",
                p && strcmp(p->command_description, "Switch to audio sinks tab") == 0);
    ASSERT_TRUE("sinks command handler registered", p && p->command_handler != NULL);
    ASSERT_TRUE("sinks command keeps open", p && p->command_keeps_open_on_hotkey_auto == 1);
}

static void test_command_handler_without_args_surfaces_tab(void) {
    AppData app;
    const CofiTabProvider *p = registered_sinks_provider();
    setup_app(&app);
    reset_capture();

    gboolean result = p->command_handler(&app, NULL, "");

    ASSERT_TRUE("sinks command without args returns false", result == FALSE);
    ASSERT_TRUE("sinks command without args exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("sinks command without args surfaces once", g_surface_tab_calls == 1);
    ASSERT_TRUE("sinks command without args surfaces sinks tab", g_last_surface_tab == TAB_SINKS);
    ASSERT_TRUE("sinks command without args keeps origin windows", app.prefix_origin_tab == TAB_WINDOWS);
    ASSERT_TRUE("sinks command without args does not hide", g_hide_window_calls == 0);
    teardown_app(&app);
}

static void test_command_handler_matches_sink_and_hides(void) {
    AppData app;
    const CofiTabProvider *p = registered_sinks_provider();
    setup_app(&app);
    seed_sink(&app);
    reset_capture();

    gboolean result = p->command_handler(&app, NULL, "Headphones");

    ASSERT_TRUE("sinks command with match returns false", result == FALSE);
    ASSERT_TRUE("sinks command with match exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("sinks command with match switches once", g_switch_name_calls == 1);
    ASSERT_TRUE("sinks command with match switches sink name",
                strcmp(g_last_switch_name, "alsa_output.usb-DAC.analog-stereo") == 0);
    ASSERT_TRUE("sinks command with match hides", g_hide_window_calls == 1);
    teardown_app(&app);
}

static void test_command_handler_recalls_slot_and_hides(void) {
    AppData app;
    const CofiTabProvider *p = registered_sinks_provider();
    setup_app(&app);
    slot_assign(&app.harpoon.store, 'a', "sinks", "alsa_output.slot");
    reset_capture();

    gboolean result = p->command_handler(&app, NULL, "@a");

    ASSERT_TRUE("sinks command slot returns false", result == FALSE);
    ASSERT_TRUE("sinks command slot switches once", g_switch_name_calls == 1);
    ASSERT_TRUE("sinks command slot switches payload",
                strcmp(g_last_switch_name, "alsa_output.slot") == 0);
    ASSERT_TRUE("sinks command slot hides", g_hide_window_calls == 1);
    teardown_app(&app);
}

static void test_command_handler_invalid_arg_shows_error(void) {
    AppData app;
    const CofiTabProvider *p = registered_sinks_provider();
    setup_app(&app);
    reset_capture();

    gboolean result = p->command_handler(&app, NULL, "missing");

    char text[128];
    read_textbuffer(app.textbuffer, text, sizeof(text));
    ASSERT_TRUE("sinks command invalid returns false", result == FALSE);
    ASSERT_TRUE("sinks command invalid does not hide", g_hide_window_calls == 0);
    ASSERT_TRUE("sinks command invalid sets help state", app.command_mode.showing_help);
    ASSERT_TRUE("sinks command invalid shows error",
                strcmp(text, "No matching sink or sink slot.") == 0);
    teardown_app(&app);
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) {
        printf("SKIP: GTK unavailable\n");
        return 0;
    }

    printf("sinks_provider behavioral tests\n");
    printf("===============================\n\n");

    test_registered_command_metadata();
    test_command_handler_without_args_surfaces_tab();
    test_command_handler_matches_sink_and_hides();
    test_command_handler_recalls_slot_and_hides();
    test_command_handler_invalid_arg_shows_error();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
