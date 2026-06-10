#include <stdio.h>
#include <string.h>

#include "core/app/app_data.h"
#include "commands/command_api.h"
#include "commands/command_mode.h"
#include "providers/cofi_tab_provider.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

static int g_update_display_calls = 0;
static const char *g_help_text_stub = NULL;
static int g_hide_window_calls = 0;
static gboolean g_execute_command_result = TRUE;
static gboolean g_should_close_after_execute_result = FALSE;
static char g_last_execute_command[256] = {0};
static char g_last_should_close_command[256] = {0};

void hide_window(AppData *app) { (void)app; g_hide_window_calls++; }
void update_display(AppData *app) {
    (void)app;
    g_update_display_calls++;
}
void cofi_enter_modal(AppData *app, const CofiTabProvider *provider) { (void)app; (void)provider; }
void cofi_exit_modal(AppData *app) { (void)app; }
const CofiTabProvider *cofi_get_provider_for_prefix(char prefix) { (void)prefix; return NULL; }
int cofi_provider_count(void) { return 0; }
const CofiTabProvider *cofi_get_provider(int provider_id) { (void)provider_id; return NULL; }
int cofi_provider_is_enabled(int provider_id) { (void)provider_id; return 0; }
int command_primary_is_available(const char *primary) { (void)primary; return 1; }
void move_selection_up(AppData *app) { (void)app; }
void move_selection_down(AppData *app) { (void)app; }
gboolean execute_command(const char *cmd, AppData *app) {
    (void)app;
    g_strlcpy(g_last_execute_command, cmd ? cmd : "", sizeof(g_last_execute_command));
    return g_execute_command_result;
}
gboolean should_close_after_execute(const char *cmd) {
    g_strlcpy(g_last_should_close_command, cmd ? cmd : "", sizeof(g_last_should_close_command));
    return g_should_close_after_execute_result;
}
char *generate_command_help_text(HelpFormat fmt, int width) {
    (void)fmt;
    (void)width;
    return g_help_text_stub ? strdup(g_help_text_stub) : NULL;
}
int get_display_columns(AppData *app) { (void)app; return 80; }
int get_max_display_lines_dynamic(AppData *app) { (void)app; return 20; }
void overlay_scrollbar(GString *s, int t, int v, int o, int c)
    { (void)s; (void)t; (void)v; (void)o; (void)c; }

#include "commands/command_parser.c"
#include "commands/command_mode.c"

static GtkWidget *g_fake_entry = NULL;
static GtkWidget *g_fake_label = NULL;

static void read_textbuffer(GtkTextBuffer *buffer, char *out, size_t out_size) {
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    g_strlcpy(out, text ? text : "", out_size);
    g_free(text);
}

static AppData make_app_with_windows(int count) {
    AppData app = {0};
    app.current_tab = TAB_WINDOWS;
    app.filtered_count = count;
    for (int i = 0; i < count; i++) {
        app.filtered[i].id = (Window)(0x100 + i);
    }
    app.selection.window_index = 0;
    app.selection.selected_window_id = app.filtered[0].id;
    app.command_target_id = 0;
    app.entry = g_fake_entry;
    app.mode_indicator = g_fake_label;
    init_command_mode(&app.command_mode);
    return app;
}

static void test_command_target_id_selects_matching_window(void) {
    AppData app = make_app_with_windows(5);
    app.command_target_id = (Window)0x102; // index 2

    enter_command_mode(&app);

    ASSERT_TRUE("target window at index 2 is selected",
                app.selection.window_index == 2);
    ASSERT_TRUE("selected_window_id matches target",
                app.selection.selected_window_id == (Window)0x102);
}

static void test_zero_command_target_id_leaves_selection_unchanged(void) {
    AppData app = make_app_with_windows(5);
    app.selection.window_index = 3;
    app.selection.selected_window_id = app.filtered[3].id;
    app.command_target_id = 0;

    enter_command_mode(&app);

    ASSERT_TRUE("selection index unchanged when command_target_id is 0",
                app.selection.window_index == 3);
    ASSERT_TRUE("selected_window_id unchanged when command_target_id is 0",
                app.selection.selected_window_id == (Window)0x103);
}

static void test_unmatched_command_target_id_leaves_selection_unchanged(void) {
    AppData app = make_app_with_windows(5);
    app.selection.window_index = 1;
    app.selection.selected_window_id = app.filtered[1].id;
    app.command_target_id = (Window)0xdeadbeef; // not in filtered

    enter_command_mode(&app);

    ASSERT_TRUE("selection index unchanged when target not in filtered list",
                app.selection.window_index == 1);
    ASSERT_TRUE("selected_window_id unchanged when target not in filtered list",
                app.selection.selected_window_id == (Window)0x101);
}

static void test_exit_command_mode_resets_command_target_id(void) {
    AppData app = make_app_with_windows(3);
    app.command_target_id = (Window)0x101;
    enter_command_mode(&app);
    exit_command_mode(&app);

    ASSERT_TRUE("command_target_id reset to 0 on exit",
                app.command_target_id == 0);
}

static void test_exit_command_mode_is_noop_when_already_normal(void) {
    AppData app = make_app_with_windows(4);
    int before_filtered_count;
    int before_window_index;
    Window before_selected_window_id;

    app.command_mode.state = CMD_MODE_NORMAL;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "obs");
    app.selection.window_index = 2;
    app.selection.selected_window_id = app.filtered[2].id;

    before_filtered_count = app.filtered_count;
    before_window_index = app.selection.window_index;
    before_selected_window_id = app.selection.selected_window_id;
    g_update_display_calls = 0;

    exit_command_mode(&app);

    ASSERT_TRUE("exit command mode keeps entry text in normal mode",
                strcmp(gtk_entry_get_text(GTK_ENTRY(app.entry)), "obs") == 0);
    ASSERT_TRUE("exit command mode keeps filtered_count in normal mode",
                app.filtered_count == before_filtered_count);
    ASSERT_TRUE("exit command mode keeps selection index in normal mode",
                app.selection.window_index == before_window_index);
    ASSERT_TRUE("exit command mode keeps selected window id in normal mode",
                app.selection.selected_window_id == before_selected_window_id);
    ASSERT_TRUE("exit command mode keeps mode NORMAL",
                app.command_mode.state == CMD_MODE_NORMAL);
    ASSERT_TRUE("exit command mode does not update display in normal mode",
                g_update_display_calls == 0);
}

static void test_help_paging_clamps_to_real_last_line(void) {
    AppData app = make_app_with_windows(1);
    GtkTextBuffer *buffer = gtk_text_buffer_new(NULL);
    app.textbuffer = buffer;
    g_help_text_stub = "line1\nline2\nline3\n";

    render_help_page(&app, INT_MAX);

    char rendered[256];
    read_textbuffer(buffer, rendered, sizeof(rendered));
    ASSERT_TRUE("help end paging includes last line", strstr(rendered, "line3") != NULL);
    ASSERT_TRUE("help end paging does not render trailing blank page",
                strstr(rendered, "line2\nline3\n\n") == NULL &&
                strstr(rendered, "line3\n\n") == NULL);
    g_object_unref(buffer);
    g_help_text_stub = NULL;
}

static void test_return_dismisses_after_close_command(void) {
    AppData app = make_app_with_windows(1);
    GdkEventKey event = {0};

    enter_command_mode(&app);
    gtk_entry_set_text(GTK_ENTRY(app.entry), "close");
    g_execute_command_result = TRUE;
    g_should_close_after_execute_result = TRUE;
    g_hide_window_calls = 0;
    g_last_execute_command[0] = '\0';
    g_last_should_close_command[0] = '\0';

    event.keyval = GDK_KEY_Return;
    handle_command_key(&event, &app);

    ASSERT_TRUE("typed close executes command", strcmp(g_last_execute_command, "close") == 0);
    ASSERT_TRUE("typed close checks dismiss policy", strcmp(g_last_should_close_command, "close") == 0);
    ASSERT_TRUE("typed close dismisses cofi", g_hide_window_calls == 1);
}

static void test_return_dismisses_after_minimize_command(void) {
    AppData app = make_app_with_windows(1);
    GdkEventKey event = {0};

    enter_command_mode(&app);
    gtk_entry_set_text(GTK_ENTRY(app.entry), "minimize-window");
    g_execute_command_result = TRUE;
    g_should_close_after_execute_result = TRUE;
    g_hide_window_calls = 0;
    g_last_execute_command[0] = '\0';
    g_last_should_close_command[0] = '\0';

    event.keyval = GDK_KEY_Return;
    handle_command_key(&event, &app);

    ASSERT_TRUE("typed minimize executes command", strcmp(g_last_execute_command, "minimize-window") == 0);
    ASSERT_TRUE("typed minimize checks dismiss policy", strcmp(g_last_should_close_command, "minimize-window") == 0);
    ASSERT_TRUE("typed minimize dismisses cofi", g_hide_window_calls == 1);
}

static void test_return_keeps_cofi_visible_when_command_does_not_close(void) {
    AppData app = make_app_with_windows(1);
    GdkEventKey event = {0};

    enter_command_mode(&app);
    gtk_entry_set_text(GTK_ENTRY(app.entry), "maximize-window");
    g_execute_command_result = TRUE;
    g_should_close_after_execute_result = FALSE;
    g_hide_window_calls = 0;
    g_last_execute_command[0] = '\0';
    g_last_should_close_command[0] = '\0';

    event.keyval = GDK_KEY_Return;
    handle_command_key(&event, &app);

    ASSERT_TRUE("typed maximize executes command", strcmp(g_last_execute_command, "maximize-window") == 0);
    ASSERT_TRUE("typed maximize checks dismiss policy", strcmp(g_last_should_close_command, "maximize-window") == 0);
    ASSERT_TRUE("typed maximize keeps cofi visible", g_hide_window_calls == 0);
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) {
        printf("Command mode targeting tests\n");
        printf("============================\n\n");
        printf("SKIP: GTK display unavailable\n");
        return 0;
    }
    g_fake_entry = gtk_entry_new();
    g_fake_label = gtk_label_new(">");

    test_command_target_id_selects_matching_window();
    test_zero_command_target_id_leaves_selection_unchanged();
    test_unmatched_command_target_id_leaves_selection_unchanged();
    test_exit_command_mode_resets_command_target_id();
    test_exit_command_mode_is_noop_when_already_normal();
    test_help_paging_clamps_to_real_last_line();
    test_return_dismisses_after_close_command();
    test_return_dismisses_after_minimize_command();
    test_return_keeps_cofi_visible_when_command_does_not_close();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
