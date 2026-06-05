#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xatom.h>

#include "core/app/app_data.h"

#undef DefaultRootWindow
#define DefaultRootWindow(display) ((Window)0x1)

#define TEST_ROOT_WINDOW ((Window)0x1)
#define TEST_NET_CLIENT_LIST ((Atom)0x1000)
#define TEST_NET_WM_NAME ((Atom)0x1001)
#define TEST_NET_WM_DESKTOP ((Atom)0x1002)

static int pass_count = 0;
static int fail_count = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { \
        printf("PASS: %s\n", name); \
        pass_count++; \
    } else { \
        printf("FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        fail_count++; \
    } \
} while (0)

typedef struct {
    Window window;
    const char *title;
    int desktop;
} MockWindowSnapshot;

static const MockWindowSnapshot window_snapshots[] = {
    { .window = 0x100, .title = "Terminal", .desktop = 3 },
    { .window = 0x100, .title = "cofi", .desktop = 4 },
};

static int client_list_step = 0;
static int title_read_count = 0;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

CofiResult get_x11_property(Display *display, Window window, Atom property,
                            Atom req_type, unsigned long max_items,
                            Atom *actual_type_return, int *actual_format_return,
                            unsigned long *n_items_return,
                            unsigned char **prop_return) {
    (void)display;
    (void)req_type;
    (void)max_items;

    if (window == TEST_ROOT_WINDOW && property == TEST_NET_CLIENT_LIST) {
        Window *windows = g_malloc(sizeof(Window));
        windows[0] = window_snapshots[client_list_step].window;

        if (actual_type_return) *actual_type_return = XA_WINDOW;
        if (actual_format_return) *actual_format_return = 32;
        if (n_items_return) *n_items_return = 1;
        if (prop_return) *prop_return = (unsigned char*)windows;

        client_list_step++;
        return COFI_SUCCESS;
    }

    if (property == TEST_NET_WM_DESKTOP) {
        long *desktop = g_malloc(sizeof(long));
        *desktop = window_snapshots[client_list_step - 1].desktop;

        if (actual_type_return) *actual_type_return = XA_CARDINAL;
        if (actual_format_return) *actual_format_return = 32;
        if (n_items_return) *n_items_return = 1;
        if (prop_return) *prop_return = (unsigned char*)desktop;

        return COFI_SUCCESS;
    }

    return COFI_ERROR;
}

char *get_window_property(Display *display, Window window, Atom property) {
    (void)display;
    (void)window;
    (void)property;

    title_read_count++;
    return g_strdup(window_snapshots[client_list_step - 1].title);
}

void get_window_class(Display *display, Window window,
                      char *instance, char *class_name) {
    (void)display;
    (void)window;
    strcpy(instance, "terminal");
    strcpy(class_name, "terminal");
}

char *get_window_type_cached(Display *display, Window window, AtomCache *atoms) {
    (void)display;
    (void)window;
    (void)atoms;
    return g_strdup("Normal");
}

int get_window_pid_cached(Display *display, Window window, AtomCache *atoms) {
    (void)display;
    (void)window;
    (void)atoms;
    return 1234;
}

int XGetWindowAttributes(Display *display, Window window,
                         XWindowAttributes *window_attributes_return) {
    (void)display;
    (void)window;
    memset(window_attributes_return, 0, sizeof(*window_attributes_return));
    return 1;
}

int XFree(void *data) {
    g_free(data);
    return 1;
}

#include "../src/x11/window_list.c"

static void init_window_list_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    app->atoms.net_client_list = TEST_NET_CLIENT_LIST;
    app->atoms.net_wm_name = TEST_NET_WM_NAME;
    app->atoms.net_wm_desktop = TEST_NET_WM_DESKTOP;
}

static void reset_window_list_mocks(void) {
    client_list_step = 0;
    title_read_count = 0;
}

static void test_get_window_list_preserves_existing_title(void) {
    AppData app;
    init_window_list_app(&app);
    reset_window_list_mocks();

    get_window_list(&app);
    ASSERT_TRUE("initial client-list refresh captures new-window title",
                app.window_count == 1 && strcmp(app.windows[0].title, "Terminal") == 0);
    ASSERT_TRUE("initial client-list refresh reads title once",
                title_read_count == 1);

    get_window_list(&app);
    ASSERT_TRUE("existing client-list refresh preserves cached title",
                app.window_count == 1 && strcmp(app.windows[0].title, "Terminal") == 0);
    ASSERT_TRUE("existing client-list refresh still updates other metadata",
                app.windows[0].desktop == 4);
    ASSERT_TRUE("existing client-list refresh does not read replacement title",
                title_read_count == 1);
}

int main(void) {
    printf("Window list tests\n");
    printf("=================\n\n");

    test_get_window_list_preserves_existing_title();

    printf("\nResults: %d/%d tests passed\n", pass_count, pass_count + fail_count);
    return fail_count == 0 ? 0 : 1;
}
