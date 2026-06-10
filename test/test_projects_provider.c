#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <gtk/gtk.h>

#include "core/app/app_data.h"
#include "providers/cofi_tab_provider.h"
#include "commands/command_registry.h"
#include "core/slot_store/slot_store.h"

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
static int g_refresh_calls;
static int g_attach_named_calls;
static char g_last_attach_name[MAX_PROJECT_SESSION_NAME_LEN];
static int g_slot_recall_calls;
static char g_last_slot_payload[SLOT_STORE_PAYLOAD_LEN];
static int g_show_project_new_calls;
static ProjectBackend g_last_new_backend;
static int g_show_project_rename_calls;
static char g_last_rename_name[MAX_PROJECT_SESSION_NAME_LEN];
static int g_show_project_remote_host_calls;
static int g_show_project_kill_calls;
static ProjectBackend g_last_kill_backend;
static char g_last_kill_name[MAX_PROJECT_SESSION_NAME_LEN];
static int g_forget_remote_calls;
static gboolean g_forget_remote_result;
static gboolean g_remote_scope_active;
static int g_remote_scope_clear_calls;
static int g_projects_refresh_calls;
static int g_update_scroll_calls;
static int g_update_display_calls;
static int g_remote_status_clear_calls;
static int g_open_folder_terminal_calls;
static CofiActionStatus g_open_folder_terminal_result;
static char g_last_terminal_folder_path[1024];
static gboolean g_last_terminal_folder_remote;
static gboolean g_has_selected_session;
static ProjectSessionEntry g_selected_session;
static gboolean g_has_selected_folder;
static ProjectFolder g_selected_folder;
static int g_show_overlay_calls;
static OverlayType g_last_overlay_type;
static int g_forget_entry_calls;
static char g_last_forget_host[128];
static char g_last_forget_name[MAX_PROJECT_SESSION_NAME_LEN];
static int g_remove_folder_calls;
static gboolean g_remove_folder_is_remote;
static char g_last_remove_folder_path[1024];
static char g_last_remove_folder_host[128];
static gboolean g_has_named_result;
static CofiActionStatus g_attach_named_result = COFI_HANDLED_HIDE;
static CofiActionStatus g_slot_recall_result = COFI_HANDLED_HIDE;

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    (void)level;
    (void)file;
    (void)line;
    (void)fmt;
}

void log_set_level(int level) {
    (void)level;
}

void projects_remote_store_init(void) {}
gboolean projects_remote_store_reload(void) { return TRUE; }
void projects_remote_scope_init(void) {}

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

int cofi_register_command(const CommandSpec *spec) {
    return spec ? 0 : -1;
}

void projects_refresh(AppData *app) {
    (void)app;
    g_refresh_calls++;
    g_projects_refresh_calls++;
}

gboolean projects_has_named(AppData *app, const char *name) {
    (void)app;
    return g_has_named_result && name && strcmp(name, "work") == 0;
}

CofiActionStatus projects_attach_named(AppData *app, const char *name) {
    (void)app;
    g_attach_named_calls++;
    g_strlcpy(g_last_attach_name, name ? name : "", sizeof(g_last_attach_name));
    return g_attach_named_result;
}

CofiActionStatus projects_slot_recall(AppData *app, const char *payload) {
    (void)app;
    g_slot_recall_calls++;
    g_strlcpy(g_last_slot_payload, payload ? payload : "", sizeof(g_last_slot_payload));
    return g_slot_recall_result;
}

ProjectSessionEntry *projects_selected_session(AppData *app) {
    (void)app;
    return g_has_selected_session ? &g_selected_session : NULL;
}

ProjectFolder *projects_selected_folder(AppData *app) {
    (void)app;
    return g_has_selected_folder ? &g_selected_folder : NULL;
}

gchar *projects_build_folder_session_name(const char *path) {
    (void)path;
    return g_strdup("folder");
}

void show_project_new_overlay(AppData *app,
                              ProjectBackend backend,
                              const char *start_dir,
                              const char *initial_name) {
    (void)app;
    (void)start_dir;
    (void)initial_name;
    g_show_project_new_calls++;
    g_last_new_backend = backend;
}

void show_project_kill_overlay(AppData *app, const char *session_name, ProjectBackend backend) {
    (void)app;
    g_show_project_kill_calls++;
    g_last_kill_backend = backend;
    g_strlcpy(g_last_kill_name, session_name ? session_name : "",
              sizeof(g_last_kill_name));
}

void show_project_rename_overlay(AppData *app, const char *session_name) {
    (void)app;
    g_show_project_rename_calls++;
    g_strlcpy(g_last_rename_name, session_name ? session_name : "",
              sizeof(g_last_rename_name));
}

void show_project_remote_host_overlay(AppData *app) {
    (void)app;
    g_show_project_remote_host_calls++;
}

gboolean projects_remote_scope_is_active(void) {
    return g_remote_scope_active;
}

void projects_remote_scope_clear(void) {
    g_remote_scope_active = FALSE;
    g_remote_scope_clear_calls++;
}
void projects_remote_scope_clear_status_message(void) { g_remote_status_clear_calls++; }
const char *projects_remote_scope_status_message(void) { return ""; }

void reset_selection(AppData *app) { (void)app; }
void update_scroll_position(AppData *app) { (void)app; g_update_scroll_calls++; }
void update_display(AppData *app) { (void)app; g_update_display_calls++; }

ProjectFolder *projects_folder_at_visible(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return NULL;
}

CofiActionStatus projects_open_folder(AppData *app, const char *path) {
    (void)app;
    (void)path;
    return COFI_HANDLED_HIDE;
}

CofiActionStatus projects_open_folder_terminal(AppData *app, const ProjectFolder *folder) {
    (void)app;
    g_open_folder_terminal_calls++;
    g_strlcpy(g_last_terminal_folder_path, (folder && folder->path) ? folder->path : "",
              sizeof(g_last_terminal_folder_path));
    g_last_terminal_folder_remote = folder ? folder->is_remote : FALSE;
    return g_open_folder_terminal_result;
}

CofiActionStatus projects_attach_visible(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return COFI_HANDLED_HIDE;
}

gboolean projects_forget_selected_remote(AppData *app) {
    (void)app;
    g_forget_remote_calls++;
    return g_forget_remote_result;
}

gboolean projects_forget_remote_entry(const char *host,
                                      ProjectBackend backend,
                                      const char *name,
                                      const char *cwd) {
    (void)backend;
    (void)cwd;
    g_forget_entry_calls++;
    g_strlcpy(g_last_forget_host, host ? host : "", sizeof(g_last_forget_host));
    g_strlcpy(g_last_forget_name, name ? name : "", sizeof(g_last_forget_name));
    return TRUE;
}

CofiActionStatus projects_remove_folder_entry(AppData *app,
                                              const char *path,
                                              gboolean is_remote,
                                              const char *remote_host) {
    (void)app;
    g_remove_folder_calls++;
    g_remove_folder_is_remote = is_remote;
    g_strlcpy(g_last_remove_folder_path, path ? path : "", sizeof(g_last_remove_folder_path));
    g_strlcpy(g_last_remove_folder_host, remote_host ? remote_host : "", sizeof(g_last_remove_folder_host));
    return COFI_HANDLED_REFRESH;
}

void show_overlay(AppData *app, OverlayType type, gpointer data) {
    (void)app;
    (void)data;
    g_show_overlay_calls++;
    g_last_overlay_type = type;
}

int projects_row_count(AppData *app) {
    (void)app;
    return 0;
}

void projects_format_row(AppData *app, int visible_idx, CofiRowCells *out) {
    (void)app;
    (void)visible_idx;
    if (out) out->cell_count = 0;
}

const char *projects_match_string(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return "";
}

const char *projects_row_identity(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return "";
}

void projects_on_enter(AppData *app) {
    (void)app;
}

void projects_on_query_changed(AppData *app, const char *query) {
    (void)app;
    (void)query;
}

void projects_on_tick(AppData *app, int generation) {
    (void)app;
    (void)generation;
}

void projects_on_leave(AppData *app) {
    (void)app;
    projects_remote_scope_clear_status_message();
    projects_remote_scope_clear();
}

const char *projects_get_shortcut_hint(AppData *app) {
    (void)app;
    return "";
}

const char *projects_slot_payload_for(AppData *app, int visible_idx) {
    (void)app;
    (void)visible_idx;
    return NULL;
}

#include "providers/cofi_tab_provider.c"
#include "core/slot_store/slot_store.c"
#include "projects/projects_provider.c"

static const CofiTabProvider *registered_projects_provider(void) {
    cofi_registry_reset();
    cofi_config_registry_reset();
    projects_provider_register();
    return cofi_get_provider(s_projects_provider_id);
}

static void reset_capture(void) {
    g_exit_command_mode_calls = 0;
    g_hide_window_calls = 0;
    g_surface_tab_calls = 0;
    g_last_surface_tab = TAB_WINDOWS;
    g_refresh_calls = 0;
    g_attach_named_calls = 0;
    g_last_attach_name[0] = '\0';
    g_slot_recall_calls = 0;
    g_last_slot_payload[0] = '\0';
    g_show_project_new_calls = 0;
    g_last_new_backend = PROJECT_BACKEND_TMUX;
    g_show_project_rename_calls = 0;
    g_last_rename_name[0] = '\0';
    g_show_project_remote_host_calls = 0;
    g_show_project_kill_calls = 0;
    g_last_kill_backend = PROJECT_BACKEND_TMUX;
    g_last_kill_name[0] = '\0';
    g_forget_remote_calls = 0;
    g_forget_remote_result = FALSE;
    g_remote_scope_active = FALSE;
    g_remote_scope_clear_calls = 0;
    g_projects_refresh_calls = 0;
    g_update_scroll_calls = 0;
    g_update_display_calls = 0;
    g_remote_status_clear_calls = 0;
    g_open_folder_terminal_calls = 0;
    g_open_folder_terminal_result = COFI_HANDLED_HIDE;
    g_last_terminal_folder_path[0] = '\0';
    g_last_terminal_folder_remote = FALSE;
    g_has_selected_session = FALSE;
    memset(&g_selected_session, 0, sizeof(g_selected_session));
    g_has_selected_folder = FALSE;
    memset(&g_selected_folder, 0, sizeof(g_selected_folder));
    g_show_overlay_calls = 0;
    g_last_overlay_type = OVERLAY_NONE;
    g_forget_entry_calls = 0;
    g_last_forget_host[0] = '\0';
    g_last_forget_name[0] = '\0';
    g_remove_folder_calls = 0;
    g_remove_folder_is_remote = FALSE;
    g_last_remove_folder_path[0] = '\0';
    g_last_remove_folder_host[0] = '\0';
    g_has_named_result = FALSE;
    g_attach_named_result = COFI_HANDLED_HIDE;
    g_slot_recall_result = COFI_HANDLED_HIDE;
}

static void setup_app(AppData *app) {
    memset(app, 0, sizeof(*app));
    app->current_tab = TAB_WINDOWS;
    app->textbuffer = gtk_text_buffer_new(NULL);
    slot_store_init(&app->harpoon.store);
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
    const CofiTabProvider *p = registered_projects_provider();

    ASSERT_TRUE("projects provider registered", p != NULL);
    ASSERT_TRUE("projects provider uses dynamic tab", p->tab_mode >= TAB_COUNT);
    ASSERT_TRUE("projects command primary", strcmp(s_projects_command.primary, "projects") == 0);
    ASSERT_TRUE("projects command alias project",
                strcmp(s_projects_command.aliases[0], "project") == 0);
    ASSERT_TRUE("projects command alias tmux",
                strcmp(s_projects_command.aliases[1], "tmux") == 0);
    ASSERT_TRUE("projects command alias zellij",
                strcmp(s_projects_command.aliases[4], "zellij") == 0);
    ASSERT_TRUE("projects command help",
                strcmp(s_projects_command.help_format,
                       "projects, project, tmux, tx, zj, zellij [@SLOT|SESSION]") == 0);
    ASSERT_TRUE("projects command description",
                strcmp(s_projects_command.description, "Switch to projects tab") == 0);
    ASSERT_TRUE("projects command handler registered", s_projects_command.handler != NULL);
    ASSERT_TRUE("projects command keeps open", s_projects_command.keeps_open_on_hotkey_auto == 1);
}

static void test_registered_config_entries(void) {
    registered_projects_provider();

    ASSERT_TRUE("projects tmux path config registered",
                cofi_config_entry_for_key("projects.tmux_path") != NULL);
    ASSERT_TRUE("projects zellij path config registered",
                cofi_config_entry_for_key("projects.zellij_path") != NULL);
    ASSERT_TRUE("projects zoxide path config registered",
                cofi_config_entry_for_key("projects.zoxide_path") != NULL);
    ASSERT_TRUE("projects file explorer path config registered",
                cofi_config_entry_for_key("projects.file_explorer_path") != NULL);

    CofiConfig config;
    init_config_defaults(&config);
    ConfigEntry entries[MAX_CONFIG_ENTRIES];
    int count = 0;
    build_config_entries(&config, entries, &count);
    int found_zellij = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].key, "projects.zellij_path") == 0 &&
            entries[i].type == CONFIG_TYPE_STRING) {
            found_zellij = 1;
        }
    }
    ASSERT_TRUE("projects config entry appears in config list", found_zellij);
}

static void test_path_config_display_resolves_path_state(void) {
    registered_projects_provider();

    char dir_template[] = "/tmp/cofi-projects-provider-XXXXXX";
    char *dir = mkdtemp(dir_template);
    ASSERT_TRUE("create path display temp dir", dir != NULL);
    if (!dir) return;

    char zellij_path[512];
    snprintf(zellij_path, sizeof(zellij_path), "%s/zellij", dir);
    FILE *file = fopen(zellij_path, "w");
    ASSERT_TRUE("create fake zellij", file != NULL);
    if (file) {
        fputs("#!/bin/sh\nexit 0\n", file);
        fclose(file);
        chmod(zellij_path, 0755);
    }

    const char *old_path = g_getenv("PATH");
    char *saved_path = old_path ? g_strdup(old_path) : NULL;
    g_setenv("PATH", dir, TRUE);

    CofiConfig config;
    init_config_defaults(&config);
    ConfigEntry entries[MAX_CONFIG_ENTRIES];
    int count = 0;
    build_config_entries(&config, entries, &count);

    int saw_zellij = 0;
    int saw_zoxide_missing = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].key, "projects.zellij_path") == 0) {
            saw_zellij = 1;
            ASSERT_TRUE("zellij raw value remains empty", entries[i].value[0] == '\0');
            ASSERT_TRUE("zellij display shows PATH resolution",
                        strstr(entries[i].display_value, "(PATH:") == entries[i].display_value);
            ASSERT_TRUE("zellij display includes resolved executable",
                        strstr(entries[i].display_value, "/zellij)") != NULL);
        } else if (strcmp(entries[i].key, "projects.zoxide_path") == 0) {
            saw_zoxide_missing = 1;
            ASSERT_TRUE("missing zoxide display is explicit",
                        strcmp(entries[i].display_value, "(NOT FOUND)") == 0);
        }
    }

    ASSERT_TRUE("zellij display entry checked", saw_zellij);
    ASSERT_TRUE("zoxide missing display entry checked", saw_zoxide_missing);

    if (saved_path) {
        g_setenv("PATH", saved_path, TRUE);
        g_free(saved_path);
    } else {
        g_unsetenv("PATH");
    }
    unlink(zellij_path);
    rmdir(dir);
}

static void test_path_config_validation(void) {
    registered_projects_provider();
    CofiConfig config;
    init_config_defaults(&config);
    char err[256] = {0};

    ASSERT_TRUE("empty tmux path accepted",
                apply_config_setting(&config, "projects.tmux_path", "", err, sizeof(err)));
    ASSERT_TRUE("empty tmux path stored",
                strcmp(config.projects_tmux_path, "") == 0);

    ASSERT_TRUE("relative tmux path rejected",
                !apply_config_setting(&config, "projects.tmux_path", "tmux", err, sizeof(err)));
    ASSERT_TRUE("relative tmux path error mentions absolute",
                strstr(err, "absolute") != NULL);

    ASSERT_TRUE("directory tmux path rejected",
                !apply_config_setting(&config, "projects.tmux_path", "/tmp", err, sizeof(err)));

    ASSERT_TRUE("absolute executable tmux path accepted",
                apply_config_setting(&config, "projects.tmux_path", "/bin/sh", err, sizeof(err)));
    ASSERT_TRUE("absolute executable tmux path stored",
                strcmp(config.projects_tmux_path, "/bin/sh") == 0);
}

static void test_command_handler_without_args_surfaces_tab(void) {
    AppData app;
    const CofiTabProvider *p = registered_projects_provider();
    setup_app(&app);
    reset_capture();

    gboolean result = s_projects_command.handler(&app, NULL, "");

    ASSERT_TRUE("projects command without args returns false", result == FALSE);
    ASSERT_TRUE("projects command without args exits command mode", g_exit_command_mode_calls == 1);
    ASSERT_TRUE("projects command without args surfaces once", g_surface_tab_calls == 1);
    ASSERT_TRUE("projects command without args surfaces projects tab",
                p && g_last_surface_tab == (TabMode)p->tab_mode);
    ASSERT_TRUE("projects command without args keeps origin windows",
                app.prefix_origin_tab == TAB_WINDOWS);
    ASSERT_TRUE("projects command without args does not hide", g_hide_window_calls == 0);
    teardown_app(&app);
}

static void test_projects_command_metadata(void) {
    ASSERT_TRUE("projects command dismisses after typed execution",
                s_projects_command.closes_cofi_after_execute == 1);
}

static void test_command_handler_named_session_does_not_hide_directly(void) {
    AppData app;
    registered_projects_provider();
    setup_app(&app);
    reset_capture();
    g_has_named_result = TRUE;

    gboolean result = s_projects_command.handler(&app, NULL, "work");

    ASSERT_TRUE("projects named command returns true", result == TRUE);
    ASSERT_TRUE("projects named command refreshes", g_refresh_calls == 1);
    ASSERT_TRUE("projects named command attaches", g_attach_named_calls == 1);
    ASSERT_TRUE("projects named command passes name", strcmp(g_last_attach_name, "work") == 0);
    ASSERT_TRUE("projects named command does not hide directly", g_hide_window_calls == 0);
    teardown_app(&app);
}

static void test_command_handler_recalls_slot_without_direct_hide(void) {
    AppData app;
    registered_projects_provider();
    setup_app(&app);
    slot_assign(&app.harpoon.store, 'a', "projects", "session:tmux:work");
    reset_capture();

    gboolean result = s_projects_command.handler(&app, NULL, "@a");

    ASSERT_TRUE("projects slot command returns true", result == TRUE);
    ASSERT_TRUE("projects slot command recalls", g_slot_recall_calls == 1);
    ASSERT_TRUE("projects slot command passes payload",
                strcmp(g_last_slot_payload, "session:tmux:work") == 0);
    ASSERT_TRUE("projects slot command does not hide directly", g_hide_window_calls == 0);
    teardown_app(&app);
}

static void test_command_handler_invalid_arg_shows_error(void) {
    AppData app;
    registered_projects_provider();
    setup_app(&app);
    reset_capture();

    gboolean result = s_projects_command.handler(&app, NULL, "missing");

    char text[128];
    read_textbuffer(app.textbuffer, text, sizeof(text));
    ASSERT_TRUE("projects invalid command returns false", result == FALSE);
    ASSERT_TRUE("projects invalid command does not hide", g_hide_window_calls == 0);
    ASSERT_TRUE("projects invalid command sets help state", app.command_mode.showing_help);
    ASSERT_TRUE("projects invalid command shows error",
                strcmp(text, "No matching tmux/zellij session.") == 0);
    teardown_app(&app);
}

static void test_ctrl_n_opens_new_session_but_ctrl_shift_n_falls_through(void) {
    AppData app;
    const CofiTabProvider *p = registered_projects_provider();
    setup_app(&app);
    reset_capture();
    app.current_tab = (TabMode)p->tab_mode;

    GdkEventKey ctrl_n;
    memset(&ctrl_n, 0, sizeof(ctrl_n));
    ctrl_n.keyval = GDK_KEY_n;
    ctrl_n.state = GDK_CONTROL_MASK;
    ASSERT_TRUE("Ctrl+n is handled by Projects",
                handle_projects_tab_keys(&ctrl_n, &app) == TRUE);
    ASSERT_TRUE("Ctrl+n opens new-session overlay",
                g_show_project_new_calls == 1 &&
                g_last_new_backend == PROJECT_BACKEND_TMUX);

    GdkEventKey ctrl_shift_n;
    memset(&ctrl_shift_n, 0, sizeof(ctrl_shift_n));
    ctrl_shift_n.keyval = GDK_KEY_n;
    ctrl_shift_n.state = GDK_CONTROL_MASK | GDK_SHIFT_MASK;
    ASSERT_TRUE("Ctrl+Shift+n is not a Projects shortcut",
                handle_projects_tab_keys(&ctrl_shift_n, &app) == FALSE);
    ASSERT_TRUE("Ctrl+Shift+n does not open new-session overlay",
                g_show_project_new_calls == 1);
    teardown_app(&app);
}

static void test_ctrl_r_opens_rename_but_ctrl_shift_r_falls_through(void) {
    AppData app;
    const CofiTabProvider *p = registered_projects_provider();
    setup_app(&app);
    reset_capture();
    app.current_tab = (TabMode)p->tab_mode;
    g_has_selected_session = TRUE;
    g_selected_session.backend = PROJECT_BACKEND_TMUX;
    g_strlcpy(g_selected_session.name, "work", sizeof(g_selected_session.name));

    GdkEventKey ctrl_r;
    memset(&ctrl_r, 0, sizeof(ctrl_r));
    ctrl_r.keyval = GDK_KEY_r;
    ctrl_r.state = GDK_CONTROL_MASK;
    ASSERT_TRUE("Ctrl+r is handled by Projects",
                handle_projects_tab_keys(&ctrl_r, &app) == TRUE);
    ASSERT_TRUE("Ctrl+r opens rename overlay",
                g_show_project_rename_calls == 1 &&
                strcmp(g_last_rename_name, "work") == 0);

    GdkEventKey ctrl_shift_r;
    memset(&ctrl_shift_r, 0, sizeof(ctrl_shift_r));
    ctrl_shift_r.keyval = GDK_KEY_r;
    ctrl_shift_r.state = GDK_CONTROL_MASK | GDK_SHIFT_MASK;
    ASSERT_TRUE("Ctrl+Shift+r is not a Projects shortcut",
                handle_projects_tab_keys(&ctrl_shift_r, &app) == FALSE);
    ASSERT_TRUE("Ctrl+Shift+r does not open rename overlay",
                g_show_project_rename_calls == 1);
    teardown_app(&app);
}

static void test_delete_shortcuts_unified_forget_vs_kill(void) {
    AppData app;
    const CofiTabProvider *p = registered_projects_provider();
    setup_app(&app);
    reset_capture();
    app.current_tab = (TabMode)p->tab_mode;
    g_has_selected_session = TRUE;

    GdkEventKey ctrl_d;
    memset(&ctrl_d, 0, sizeof(ctrl_d));
    ctrl_d.keyval = GDK_KEY_d;
    ctrl_d.state = GDK_CONTROL_MASK;

    GdkEventKey del;
    memset(&del, 0, sizeof(del));
    del.keyval = GDK_KEY_Delete;

    g_selected_session.is_saved_remote = TRUE;
    g_strlcpy(g_selected_session.remote_host, "edge", sizeof(g_selected_session.remote_host));
    g_strlcpy(g_selected_session.name, "saved", sizeof(g_selected_session.name));
    ASSERT_TRUE("Ctrl+d handles saved remote via unified delete",
                handle_projects_tab_keys(&ctrl_d, &app) == TRUE);
    ASSERT_TRUE("Ctrl+d on saved remote opens confirm overlay",
                g_show_overlay_calls == 1 && g_last_overlay_type == OVERLAY_PROJECT_KILL);
    ASSERT_TRUE("Ctrl+d on saved remote sets forget action",
                app.project_kill.action == PROJECT_DELETE_FORGET_REMOTE &&
                strcmp(app.project_kill.remote_host, "edge") == 0 &&
                strcmp(app.project_kill.session_name, "saved") == 0);
    ASSERT_TRUE("Ctrl+d on saved remote does not forget before confirm",
                g_forget_entry_calls == 0);
    ASSERT_TRUE("Ctrl+d clears remote status line",
                g_remote_status_clear_calls == 1);

    g_selected_session.is_saved_remote = FALSE;
    g_selected_session.backend = PROJECT_BACKEND_ZELLIJ;
    g_strlcpy(g_selected_session.name, "live-zj", sizeof(g_selected_session.name));
    ASSERT_TRUE("Delete handles live session via unified delete",
                handle_projects_tab_keys(&del, &app) == TRUE);
    ASSERT_TRUE("Delete on live session opens kill overlay",
                g_show_project_kill_calls == 1 &&
                g_last_kill_backend == PROJECT_BACKEND_ZELLIJ &&
                strcmp(g_last_kill_name, "live-zj") == 0);
    ASSERT_TRUE("Delete also clears remote status line",
                g_remote_status_clear_calls == 2);

    g_has_selected_session = FALSE;
    g_has_selected_folder = TRUE;
    g_selected_folder.path = "/home/dl/work";
    g_selected_folder.is_remote = FALSE;
    ASSERT_TRUE("Delete on local folder is consumed",
                handle_projects_tab_keys(&del, &app) == TRUE);
    ASSERT_TRUE("Delete on local folder opens confirm overlay",
                g_show_overlay_calls == 2 && g_last_overlay_type == OVERLAY_PROJECT_KILL);
    ASSERT_TRUE("Delete on local folder sets remove-folder action",
                app.project_kill.action == PROJECT_DELETE_REMOVE_FOLDER &&
                strcmp(app.project_kill.folder_path, "/home/dl/work") == 0 &&
                app.project_kill.folder_is_remote == FALSE);

    g_selected_folder.is_remote = TRUE;
    g_strlcpy(g_selected_folder.remote_host, "tsunami", sizeof(g_selected_folder.remote_host));
    ASSERT_TRUE("Ctrl+d on remote folder is consumed",
                handle_projects_tab_keys(&ctrl_d, &app) == TRUE);
    ASSERT_TRUE("Ctrl+d on remote folder opens confirm overlay",
                g_show_overlay_calls == 3 && g_last_overlay_type == OVERLAY_PROJECT_KILL);
    ASSERT_TRUE("Ctrl+d on remote folder keeps remote host for confirm",
                app.project_kill.folder_is_remote == TRUE &&
                strcmp(app.project_kill.remote_host, "tsunami") == 0);
    teardown_app(&app);
}

static void test_ctrl_s_opens_remote_host_overlay(void) {
    AppData app;
    const CofiTabProvider *p = registered_projects_provider();
    setup_app(&app);
    reset_capture();
    app.current_tab = (TabMode)p->tab_mode;

    GdkEventKey ctrl_s;
    memset(&ctrl_s, 0, sizeof(ctrl_s));
    ctrl_s.keyval = GDK_KEY_s;
    ctrl_s.state = GDK_CONTROL_MASK;

    ASSERT_TRUE("Ctrl+s handled by Projects",
                handle_projects_tab_keys(&ctrl_s, &app) == TRUE);
    ASSERT_TRUE("Ctrl+s opens remote host overlay",
                g_show_project_remote_host_calls == 1);
    ASSERT_TRUE("Ctrl+s clears remote status line",
                g_remote_status_clear_calls == 1);
    teardown_app(&app);
}

static void test_ctrl_t_opens_terminal_only_for_folder_rows(void) {
    AppData app;
    const CofiTabProvider *p = registered_projects_provider();
    setup_app(&app);
    reset_capture();
    app.current_tab = (TabMode)p->tab_mode;

    GdkEventKey ctrl_t;
    memset(&ctrl_t, 0, sizeof(ctrl_t));
    ctrl_t.keyval = GDK_KEY_t;
    ctrl_t.state = GDK_CONTROL_MASK;

    g_has_selected_session = TRUE;
    g_strlcpy(g_selected_session.name, "work", sizeof(g_selected_session.name));
    ASSERT_TRUE("Ctrl+t on session is ignored",
                handle_projects_tab_keys(&ctrl_t, &app) == FALSE);
    ASSERT_TRUE("Ctrl+t on session does not launch terminal",
                g_open_folder_terminal_calls == 0);

    g_has_selected_session = FALSE;
    g_has_selected_folder = TRUE;
    g_selected_folder.path = "/home/dl/work dir";
    g_selected_folder.is_remote = FALSE;
    g_open_folder_terminal_result = COFI_HANDLED_HIDE;
    ASSERT_TRUE("Ctrl+t on local folder is handled",
                handle_projects_tab_keys(&ctrl_t, &app) == TRUE);
    ASSERT_TRUE("Ctrl+t launches terminal for selected folder",
                g_open_folder_terminal_calls == 1 &&
                strcmp(g_last_terminal_folder_path, "/home/dl/work dir") == 0 &&
                g_last_terminal_folder_remote == FALSE);
    ASSERT_TRUE("Ctrl+t hide result hides window",
                g_hide_window_calls == 1);
    teardown_app(&app);
}

static void test_escape_in_remote_scope_returns_to_local(void) {
    AppData app;
    const CofiTabProvider *p = registered_projects_provider();
    setup_app(&app);
    reset_capture();
    app.current_tab = (TabMode)p->tab_mode;
    g_remote_scope_active = TRUE;

    GdkEventKey esc;
    memset(&esc, 0, sizeof(esc));
    esc.keyval = GDK_KEY_Escape;

    ASSERT_TRUE("Esc handled while remote scope active",
                handle_projects_tab_keys(&esc, &app) == TRUE);
    ASSERT_TRUE("Esc clears remote scope", g_remote_scope_clear_calls == 1);
    ASSERT_TRUE("Esc clears remote status line", g_remote_status_clear_calls == 1);
    ASSERT_TRUE("Esc refreshes and redraws projects tab",
                g_projects_refresh_calls == 1 &&
                g_update_scroll_calls == 1 &&
                g_update_display_calls == 1);
    teardown_app(&app);
}

static void test_projects_on_leave_clears_remote_status_line(void) {
    AppData app;
    const CofiTabProvider *p = registered_projects_provider();
    setup_app(&app);
    reset_capture();

    ASSERT_TRUE("projects provider exposes on_leave hook", p->on_leave != NULL);
    p->on_leave(&app);
    ASSERT_TRUE("projects on_leave clears remote status line",
                g_remote_status_clear_calls == 1);
    teardown_app(&app);
}

static void test_command_args_contract(void) {
    AppData app;
    const CofiTabProvider *p = registered_projects_provider();
    setup_app(&app);
    reset_capture();

    ASSERT_TRUE("projects empty command args no-op",
                p->on_command_args(&app, "") == COFI_NO_OP);

    g_has_named_result = TRUE;
    ASSERT_TRUE("projects named command args hide",
                p->on_command_args(&app, "work") == COFI_HANDLED_HIDE);

    g_has_named_result = FALSE;
    slot_assign(&app.harpoon.store, 'b', "projects", "session:zellij:work");
    ASSERT_TRUE("projects slot command args hide",
                p->on_command_args(&app, "@b") == COFI_HANDLED_HIDE);

    ASSERT_TRUE("projects missing command args error",
                p->on_command_args(&app, "missing") == COFI_ACTION_ERROR);
    teardown_app(&app);
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) {
        printf("SKIP: GTK unavailable\n");
        return 0;
    }

    printf("projects_provider behavioral tests\n");
    printf("===================================\n\n");

    test_registered_command_metadata();
    test_registered_config_entries();
    test_path_config_display_resolves_path_state();
    test_path_config_validation();
    test_command_handler_without_args_surfaces_tab();
    test_projects_command_metadata();
    test_command_handler_named_session_does_not_hide_directly();
    test_command_handler_recalls_slot_without_direct_hide();
    test_command_handler_invalid_arg_shows_error();
    test_ctrl_n_opens_new_session_but_ctrl_shift_n_falls_through();
    test_ctrl_r_opens_rename_but_ctrl_shift_r_falls_through();
    test_delete_shortcuts_unified_forget_vs_kill();
    test_ctrl_s_opens_remote_host_overlay();
    test_ctrl_t_opens_terminal_only_for_folder_rows();
    test_escape_in_remote_scope_returns_to_local();
    test_projects_on_leave_clears_remote_status_line();
    test_command_args_contract();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
