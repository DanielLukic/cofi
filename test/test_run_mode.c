#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/run_mode.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

static int g_update_display_calls = 0;

void hide_window(AppData *app) {
    (void)app;
}

void update_display(AppData *app) {
    (void)app;
    g_update_display_calls++;
}

#include "../src/run_mode.c"

static void test_extract_run_command_strips_prefix_and_whitespace(void) {
    char command[256];

    ASSERT_TRUE("extract strips ! and whitespace",
                extract_run_command("!   echo hi  ", command, sizeof(command)) &&
                strcmp(command, "echo hi") == 0);
}

static void test_extract_run_command_accepts_raw_entry_text(void) {
    char command[256];

    ASSERT_TRUE("extract accepts raw run entry without !",
                extract_run_command("echo hi", command, sizeof(command)) &&
                strcmp(command, "echo hi") == 0);
}

static void test_extract_run_command_rejects_empty_and_whitespace_only(void) {
    char command[256];

    ASSERT_TRUE("extract rejects bare prompt",
                !extract_run_command("!", command, sizeof(command)));
    ASSERT_TRUE("extract rejects whitespace-only command",
                !extract_run_command("!   \t  ", command, sizeof(command)));
}

static void test_run_history_is_session_only_ring_with_dedup_of_latest(void) {
    RunMode run_mode;
    char entry_text[256];

    init_run_mode(&run_mode);
    add_run_history_entry(&run_mode, "echo one");
    add_run_history_entry(&run_mode, "echo one");
    add_run_history_entry(&run_mode, "echo two");

    ASSERT_TRUE("history keeps only distinct latest command",
                run_mode.history_count == 2);
    ASSERT_TRUE("history newest first",
                strcmp(run_mode.history[0], "echo two") == 0);
    ASSERT_TRUE("history retains prior command",
                strcmp(run_mode.history[1], "echo one") == 0);

    ASSERT_TRUE("history up shows newest raw command",
                browse_run_history(&run_mode, -1, entry_text, sizeof(entry_text)) &&
                strcmp(entry_text, "echo two") == 0);
    ASSERT_TRUE("history second up shows next entry",
                browse_run_history(&run_mode, -1, entry_text, sizeof(entry_text)) &&
                strcmp(entry_text, "echo one") == 0);
    ASSERT_TRUE("history down returns toward newer entry",
                browse_run_history(&run_mode, 1, entry_text, sizeof(entry_text)) &&
                strcmp(entry_text, "echo two") == 0);
    ASSERT_TRUE("history down from newest returns empty entry",
                browse_run_history(&run_mode, 1, entry_text, sizeof(entry_text)) &&
                strcmp(entry_text, "") == 0);
}

static void test_enter_run_mode_keeps_entry_without_prefix(void) {
    AppData app = {0};

    app.entry = gtk_entry_new();
    app.mode_indicator = gtk_label_new("> ");

    enter_run_mode(&app, NULL);
    ASSERT_TRUE("enter run mode sets empty entry",
                strcmp(gtk_entry_get_text(GTK_ENTRY(app.entry)), "") == 0);

    enter_run_mode(&app, "echo hi");
    ASSERT_TRUE("enter run mode prefill has no ! prefix",
                strcmp(gtk_entry_get_text(GTK_ENTRY(app.entry)), "echo hi") == 0);
}

static void test_exit_run_mode_is_noop_when_already_normal(void) {
    AppData app = {0};
    int before_filtered_count;
    int before_window_index;
    Window before_selected_window_id;

    app.entry = gtk_entry_new();
    app.mode_indicator = gtk_label_new("> ");
    app.filtered_count = 4;
    for (int i = 0; i < app.filtered_count; i++) {
        app.filtered[i].id = (Window)(0x200 + i);
    }
    app.selection.window_index = 2;
    app.selection.selected_window_id = app.filtered[2].id;
    app.command_mode.state = CMD_MODE_NORMAL;
    gtk_entry_set_text(GTK_ENTRY(app.entry), "obs");

    before_filtered_count = app.filtered_count;
    before_window_index = app.selection.window_index;
    before_selected_window_id = app.selection.selected_window_id;
    g_update_display_calls = 0;

    exit_run_mode(&app);

    ASSERT_TRUE("exit run mode keeps entry text in normal mode",
                strcmp(gtk_entry_get_text(GTK_ENTRY(app.entry)), "obs") == 0);
    ASSERT_TRUE("exit run mode keeps filtered_count in normal mode",
                app.filtered_count == before_filtered_count);
    ASSERT_TRUE("exit run mode keeps selection index in normal mode",
                app.selection.window_index == before_window_index);
    ASSERT_TRUE("exit run mode keeps selected window id in normal mode",
                app.selection.selected_window_id == before_selected_window_id);
    ASSERT_TRUE("exit run mode keeps mode NORMAL",
                app.command_mode.state == CMD_MODE_NORMAL);
    ASSERT_TRUE("exit run mode does not update display in normal mode",
                g_update_display_calls == 0);
}

int main(void) {
    int argc = 0;
    char **argv = NULL;

    if (!gtk_init_check(&argc, &argv)) {
        printf("Run mode tests\n");
        printf("==============\n\n");
        printf("SKIP: GTK display unavailable\n");
        return 0;
    }

    test_extract_run_command_strips_prefix_and_whitespace();
    test_extract_run_command_accepts_raw_entry_text();
    test_extract_run_command_rejects_empty_and_whitespace_only();
    test_run_history_is_session_only_ring_with_dedup_of_latest();
    test_enter_run_mode_keeps_entry_without_prefix();
    test_exit_run_mode_is_noop_when_already_normal();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
