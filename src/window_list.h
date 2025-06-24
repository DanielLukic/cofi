#ifndef WINDOW_LIST_H
#define WINDOW_LIST_H

#include "app_data.h"

// Get list of all windows using _NET_CLIENT_LIST
void get_window_list(AppData *app);

#endif // WINDOW_LIST_H