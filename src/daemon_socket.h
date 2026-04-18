#ifndef DAEMON_SOCKET_H
#define DAEMON_SOCKET_H

#include <stddef.h>
#include <stdint.h>

#define COFI_SOCKET_PATH_MAX 108

#define COFI_OPCODE_RESERVED 0
#define COFI_OPCODE_WINDOWS 1
#define COFI_OPCODE_WORKSPACES 2
#define COFI_OPCODE_HARPOON 3
#define COFI_OPCODE_NAMES 4
#define COFI_OPCODE_COMMAND 5
#define COFI_OPCODE_RUN 6
#define COFI_OPCODE_APPLICATIONS 7

int daemon_socket_is_valid_opcode(uint8_t opcode);
const char *daemon_socket_opcode_name(uint8_t opcode);

int daemon_socket_get_path(char *buffer, size_t buffer_size);
int daemon_socket_connect(const char *socket_path);
int daemon_socket_send_opcode(int socket_fd, uint8_t opcode);
int daemon_socket_send_opcode_to_path(const char *socket_path, uint8_t opcode);

int daemon_socket_bind_listener(const char *socket_path);
int daemon_socket_set_nonblocking(int fd);
int daemon_socket_accept_opcode(int listener_fd, uint8_t *opcode_out);

#endif // DAEMON_SOCKET_H
