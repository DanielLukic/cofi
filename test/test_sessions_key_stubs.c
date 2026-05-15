#include "../src/app_data.h"

SessionEntry *sessions_selected_session(AppData *app) {
    (void)app;
    return NULL;
}

void show_session_kill_overlay(AppData *app, const char *session_name, SessionBackend backend) {
    (void)app;
    (void)session_name;
    (void)backend;
}

void show_session_rename_overlay(AppData *app, const char *session_name) {
    (void)app;
    (void)session_name;
}

void show_session_new_overlay(AppData *app) {
    (void)app;
}
