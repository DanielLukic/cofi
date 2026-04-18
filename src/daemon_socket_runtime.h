#ifndef DAEMON_SOCKET_RUNTIME_H
#define DAEMON_SOCKET_RUNTIME_H

#include <stdint.h>

#include "app_data.h"

int daemon_socket_start_monitor(AppData *app);
void daemon_socket_stop_monitor(AppData *app);
void daemon_socket_dispatch_opcode(AppData *app, uint8_t opcode);

#endif // DAEMON_SOCKET_RUNTIME_H
