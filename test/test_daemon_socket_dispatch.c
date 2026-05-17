#include <stdio.h>
#include <string.h>

#include "../src/app_data.h"
#include "../src/cofi_tab_provider.h"
#include "../src/daemon_socket.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

static int show_window_calls = 0;
static int switch_tab_calls = 0;
static int enter_command_mode_calls = 0;
static int call_sequence = 0;
static int show_window_sequence = 0;
static int get_active_window_sequence = 0;
static int enter_command_mode_sequence = 0;
static int exit_command_mode_calls = 0;
static int reset_selection_calls = 0;
static int filter_windows_calls = 0;
static int update_display_calls = 0;
static int gtk_entry_set_text_calls = 0;
static int gtk_widget_grab_focus_calls = 0;
static Window active_window_id_stub = (Window)0x1234;
static guint32 fresh_focus_timestamp_stub = 0;
static guint32 focus_timestamp_at_show = 0;
static int xinternatom_calls = 0;
static int xchangeproperty_calls = 0;
static int xflush_calls = 0;
static guint32 user_time_property_value_at_set = 0;
static CofiTabProvider run_provider_stub;
static int run_provider_available = 1;

#define TEST_WORKSPACES_TAB ((TabMode)(TAB_COUNT + 1))
#define TEST_HARPOON_TAB    ((TabMode)(TAB_COUNT + 2))
#define TEST_NAMES_TAB      ((TabMode)(TAB_COUNT + 3))
#define TEST_APPS_TAB       ((TabMode)(TAB_COUNT + 4))

TabMode workspaces_tab_mode(void) {
    return TEST_WORKSPACES_TAB;
}

TabMode harpoon_tab_mode(void) {
    return TEST_HARPOON_TAB;
}

TabMode apps_tab_mode(void) {
    return TEST_APPS_TAB;
}

TabMode names_tab_mode(void) {
    return TEST_NAMES_TAB;
}

void show_window(AppData *app) {
    show_window_calls++;
    focus_timestamp_at_show = app->focus_timestamp;
    show_window_sequence = ++call_sequence;
}

void switch_to_tab(AppData *app, TabMode tab) {
    switch_tab_calls++;
    app->current_tab = tab;
}

void surface_tab(AppData *app, TabMode tab) {
    switch_tab_calls++;
    app->current_tab = tab;
}

void enter_command_mode(AppData *app) {
    enter_command_mode_calls++;
    enter_command_mode_sequence = ++call_sequence;
    app->command_mode.state = CMD_MODE_COMMAND;
}

void exit_command_mode(AppData *app) {
    exit_command_mode_calls++;
    app->command_mode.state = CMD_MODE_NORMAL;
}

void reset_selection(AppData *app) {
    (void)app;
    reset_selection_calls++;
}

void filter_windows(AppData *app, const char *query) {
    (void)app;
    (void)query;
    filter_windows_calls++;
}

void update_display(AppData *app) {
    (void)app;
    update_display_calls++;
}

int get_active_window_id(Display *display) {
    (void)display;
    get_active_window_sequence = ++call_sequence;
    return (int)active_window_id_stub;
}

void gtk_entry_set_text(GtkEntry *entry, const gchar *text) {
    (void)entry;
    (void)text;
    gtk_entry_set_text_calls++;
}

void gtk_widget_grab_focus(GtkWidget *widget) {
    (void)widget;
    gtk_widget_grab_focus_calls++;
}

guint32 test_get_fresh_focus_timestamp(AppData *app) {
    (void)app;
    return fresh_focus_timestamp_stub;
}

Atom test_XInternAtom(Display *display, const char *atom_name, Bool only_if_exists) {
    (void)display;
    (void)atom_name;
    (void)only_if_exists;
    xinternatom_calls++;
    return (Atom)1;
}

int test_XChangeProperty(Display *display, Window w, Atom property, Atom type,
                         int format, int mode, const unsigned char *data,
                         int nelements) {
    (void)display;
    (void)w;
    (void)property;
    (void)type;
    (void)format;
    (void)mode;
    (void)nelements;
    xchangeproperty_calls++;
    user_time_property_value_at_set = *(const guint32 *)data;
    return Success;
}

int test_XFlush(Display *display) {
    (void)display;
    xflush_calls++;
    return 0;
}

#define cofi_get_fresh_focus_timestamp test_get_fresh_focus_timestamp
#define XInternAtom test_XInternAtom
#define XChangeProperty test_XChangeProperty
#define XFlush test_XFlush

#ifdef GTK_ENTRY
#undef GTK_ENTRY
#endif
#define GTK_ENTRY(widget) ((GtkEntry *)(widget))

static int cofi_enter_modal_calls = 0;
static int cofi_exit_modal_calls = 0;

void cofi_enter_modal(AppData *app, const CofiTabProvider *provider) {
    (void)provider;
    cofi_enter_modal_calls++;
    app->command_mode.state = CMD_MODE_MODAL;
}
void cofi_exit_modal(AppData *app) {
    cofi_exit_modal_calls++;
    app->command_mode.state = CMD_MODE_NORMAL;
}
const CofiTabProvider *cofi_get_provider_for_prefix(char prefix) {
    if (prefix == '!' && run_provider_available) return &run_provider_stub;
    return NULL;
}
int cofi_get_provider_id_for_tab(int tab_mode) { (void)tab_mode; return -1; }
CofiActionStatus cofi_call_on_command_args(int id, AppData *app, const char *args) {
    (void)id; (void)app; (void)args; return COFI_NO_OP;
}

#include "../src/daemon_socket_runtime.c"

static void reset_mocks(void) {
    show_window_calls = 0;
    switch_tab_calls = 0;
    enter_command_mode_calls = 0;
    call_sequence = 0;
    show_window_sequence = 0;
    get_active_window_sequence = 0;
    enter_command_mode_sequence = 0;
    exit_command_mode_calls = 0;
    cofi_enter_modal_calls = 0;
    cofi_exit_modal_calls = 0;
    reset_selection_calls = 0;
    filter_windows_calls = 0;
    update_display_calls = 0;
    gtk_entry_set_text_calls = 0;
    gtk_widget_grab_focus_calls = 0;
    fresh_focus_timestamp_stub = 0;
    focus_timestamp_at_show = 0;
    xinternatom_calls = 0;
    xchangeproperty_calls = 0;
    xflush_calls = 0;
    user_time_property_value_at_set = 0;
    memset(&run_provider_stub, 0, sizeof(run_provider_stub));
    run_provider_stub.tab_mode = TAB_COUNT + 1;
    run_provider_stub.prefix_char = '!';
    run_provider_available = 1;
}

static AppData make_app(void) {
    AppData app = {0};
    app.current_tab = TAB_WINDOWS;
    app.command_mode.state = CMD_MODE_NORMAL;
    app.entry = (GtkWidget *)0x1;
    app.display = (Display *)0x1;
    app.own_window_id = (Window)0x77;
    return app;
}

static void test_tab_opcode_dispatch(void) {
    struct {
        uint8_t opcode;
        TabMode expected_tab;
    } cases[] = {
        {COFI_OPCODE_WINDOWS, TAB_WINDOWS},
        {COFI_OPCODE_WORKSPACES, TEST_WORKSPACES_TAB},
        {COFI_OPCODE_HARPOON, TEST_HARPOON_TAB},
        {COFI_OPCODE_NAMES, TEST_NAMES_TAB},
        {COFI_OPCODE_APPLICATIONS, TEST_APPS_TAB}
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        AppData app = make_app();
        reset_mocks();
        fresh_focus_timestamp_stub = 1111 + (guint32)i;

        daemon_socket_dispatch_opcode(&app, cases[i].opcode);

        char label[128];
        snprintf(label, sizeof(label), "opcode %u sets expected tab", cases[i].opcode);
        ASSERT_TRUE(label, app.current_tab == cases[i].expected_tab);

        snprintf(label, sizeof(label), "opcode %u shows window", cases[i].opcode);
        ASSERT_TRUE(label, show_window_calls == 1);

        snprintf(label, sizeof(label), "opcode %u sets focus timestamp before show", cases[i].opcode);
        ASSERT_TRUE(label, focus_timestamp_at_show == fresh_focus_timestamp_stub);

        snprintf(label, sizeof(label), "opcode %u marks _NET_WM_USER_TIME", cases[i].opcode);
        ASSERT_TRUE(label, xchangeproperty_calls == 1);
        snprintf(label, sizeof(label), "opcode %u sets property to fresh timestamp", cases[i].opcode);
        ASSERT_TRUE(label, user_time_property_value_at_set == fresh_focus_timestamp_stub);

        if (cases[i].opcode == COFI_OPCODE_WINDOWS) {
            ASSERT_TRUE("windows opcode clears entry text", gtk_entry_set_text_calls == 1);
            ASSERT_TRUE("windows opcode resets selection", reset_selection_calls == 1);
            ASSERT_TRUE("windows opcode refilters", filter_windows_calls == 1);
            ASSERT_TRUE("windows opcode refreshes display", update_display_calls == 1);
            ASSERT_TRUE("windows opcode does not switch tab helper", switch_tab_calls == 0);
        } else {
            ASSERT_TRUE("non-windows opcode uses tab switch helper", switch_tab_calls == 1);
        }

        ASSERT_TRUE("tab opcodes focus entry", gtk_widget_grab_focus_calls == 1);
    }
}

static void test_command_opcode_dispatch(void) {
    AppData app = make_app();
    reset_mocks();

    active_window_id_stub = (Window)0xCAFE;
    fresh_focus_timestamp_stub = 0xBEEF;
    daemon_socket_dispatch_opcode(&app, COFI_OPCODE_COMMAND);

    ASSERT_TRUE("command opcode sets windows tab", app.current_tab == TAB_WINDOWS);
    ASSERT_TRUE("command opcode captures active window id", app.command_target_id == (Window)0xCAFE);
    ASSERT_TRUE("command opcode shows window", show_window_calls == 1);
    ASSERT_TRUE("command opcode sets focus timestamp before show", focus_timestamp_at_show == fresh_focus_timestamp_stub);
    ASSERT_TRUE("command opcode marks _NET_WM_USER_TIME", xchangeproperty_calls == 1);
    ASSERT_TRUE("command opcode sets property timestamp", user_time_property_value_at_set == fresh_focus_timestamp_stub);
    ASSERT_TRUE("command opcode enters command mode", enter_command_mode_calls == 1);
    ASSERT_TRUE("command opcode captures target before show",
                get_active_window_sequence > 0 &&
                show_window_sequence > 0 &&
                get_active_window_sequence < show_window_sequence);
    ASSERT_TRUE("command opcode enters command mode after show",
                enter_command_mode_sequence > 0 &&
                show_window_sequence > 0 &&
                show_window_sequence < enter_command_mode_sequence);
}

static void test_run_opcode_dispatch(void) {
    AppData app = make_app();
    reset_mocks();

    app.command_mode.state = CMD_MODE_COMMAND;
    fresh_focus_timestamp_stub = 0xCA11;
    daemon_socket_dispatch_opcode(&app, COFI_OPCODE_RUN);

    ASSERT_TRUE("run opcode sets windows tab", app.current_tab == TAB_WINDOWS);
    ASSERT_TRUE("run opcode shows window", show_window_calls == 1);
    ASSERT_TRUE("run opcode sets focus timestamp before show", focus_timestamp_at_show == fresh_focus_timestamp_stub);
    ASSERT_TRUE("run opcode marks _NET_WM_USER_TIME", xchangeproperty_calls == 1);
    ASSERT_TRUE("run opcode sets property timestamp", user_time_property_value_at_set == fresh_focus_timestamp_stub);
    ASSERT_TRUE("run opcode exits command mode first", exit_command_mode_calls == 1);
    ASSERT_TRUE("run opcode enters run modal", cofi_enter_modal_calls == 1);
}

static void test_run_opcode_ignores_disabled_provider(void) {
    AppData app = make_app();
    reset_mocks();

    run_provider_available = 0;
    daemon_socket_dispatch_opcode(&app, COFI_OPCODE_RUN);

    ASSERT_TRUE("disabled run provider does not show window", show_window_calls == 0);
    ASSERT_TRUE("disabled run provider does not enter modal", cofi_enter_modal_calls == 0);
}

int main(void) {
    test_tab_opcode_dispatch();
    test_command_opcode_dispatch();
    test_run_opcode_dispatch();
    test_run_opcode_ignores_disabled_provider();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
