#ifndef DAEMON_SOCKET_RUNTIME_H
#define DAEMON_SOCKET_RUNTIME_H

#include <stdint.h>

#include "core/app/app_data.h"

int daemon_socket_start_monitor(AppData *app);
void daemon_socket_stop_monitor(AppData *app);
void daemon_socket_dispatch_opcode(AppData *app, uint8_t opcode);
void daemon_socket_dispatch_show_tab(AppData *app, const char *name);

#endif // DAEMON_SOCKET_RUNTIME_H
