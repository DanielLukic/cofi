#ifndef APP_INIT_H
#define APP_INIT_H

#include "app_data.h"

// Application initialization functions
void init_app_data(AppData *app);
void init_x11_connection(AppData *app);
void init_workspaces(AppData *app);
void init_window_list(AppData *app);
void init_history_from_windows(AppData *app);

#endif // APP_INIT_H