#include "../src/app_data.h"

const char *tmux_selected_session_name(AppData *app) {
    (void)app;
    return NULL;
}

void show_tmux_kill_overlay(AppData *app, const char *session_name) {
    (void)app;
    (void)session_name;
}

void show_tmux_rename_overlay(AppData *app, const char *session_name) {
    (void)app;
    (void)session_name;
}

void show_tmux_new_overlay(AppData *app) {
    (void)app;
}
