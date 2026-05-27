#include "core/app/app_data.h"

ProjectSessionEntry *projects_selected_session(AppData *app) {
    (void)app;
    return NULL;
}

ProjectFolder *projects_selected_folder(AppData *app) {
    (void)app;
    return NULL;
}

void show_project_kill_overlay(AppData *app, const char *session_name, ProjectBackend backend) {
    (void)app;
    (void)session_name;
    (void)backend;
}

void show_project_rename_overlay(AppData *app, const char *session_name) {
    (void)app;
    (void)session_name;
}

void show_project_new_overlay(AppData *app,
                              ProjectBackend backend,
                              const char *start_dir,
                              const char *initial_name) {
    (void)app;
    (void)backend;
    (void)start_dir;
    (void)initial_name;
}
