#include "daemon_socket.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "log.h"

int daemon_socket_is_valid_opcode(uint8_t opcode) {
    return opcode >= COFI_OPCODE_WINDOWS && opcode <= COFI_OPCODE_APPLICATIONS;
}

const char *daemon_socket_opcode_name(uint8_t opcode) {
    switch (opcode) {
        case COFI_OPCODE_WINDOWS:
            return "windows";
        case COFI_OPCODE_WORKSPACES:
            return "workspaces";
        case COFI_OPCODE_HARPOON:
            return "harpoon";
        case COFI_OPCODE_NAMES:
            return "names";
        case COFI_OPCODE_COMMAND:
            return "command";
        case COFI_OPCODE_RUN:
            return "run";
        case COFI_OPCODE_APPLICATIONS:
            return "applications";
        default:
            return "invalid";
    }
}

int daemon_socket_get_path(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        errno = EINVAL;
        return -1;
    }

    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir || runtime_dir[0] == '\0') {
        runtime_dir = "/tmp";
    }

    int written = snprintf(buffer, buffer_size, "%s/cofi.sock", runtime_dir);
    if (written < 0 || (size_t)written >= buffer_size) {
        errno = ENAMETOOLONG;
        return -1;
    }

    return 0;
}

int daemon_socket_connect(const char *socket_path) {
    if (!socket_path) {
        errno = EINVAL;
        return -1;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    return fd;
}

int daemon_socket_send_opcode(int socket_fd, uint8_t opcode) {
    if (socket_fd < 0 || !daemon_socket_is_valid_opcode(opcode)) {
        errno = EINVAL;
        return -1;
    }

    ssize_t sent = send(socket_fd, &opcode, sizeof(opcode), 0);
    if (sent != (ssize_t)sizeof(opcode)) {
        if (sent >= 0) {
            errno = EIO;
        }
        return -1;
    }

    return 0;
}

int daemon_socket_send_opcode_to_path(const char *socket_path, uint8_t opcode) {
    int fd = daemon_socket_connect(socket_path);
    if (fd < 0) {
        return -1;
    }

    int rc = daemon_socket_send_opcode(fd, opcode);
    int saved_errno = errno;
    close(fd);
    errno = saved_errno;
    return rc;
}

static int bind_listener_once(int listener_fd, const char *socket_path) {
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (bind(listener_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        return -1;
    }

    if (listen(listener_fd, 16) < 0) {
        return -1;
    }

    return 0;
}

int daemon_socket_bind_listener(const char *socket_path) {
    if (!socket_path) {
        errno = EINVAL;
        return -1;
    }

    int listener_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener_fd < 0) {
        return -1;
    }

    if (bind_listener_once(listener_fd, socket_path) == 0) {
        return listener_fd;
    }

    if (errno != EADDRINUSE) {
        int saved_errno = errno;
        close(listener_fd);
        errno = saved_errno;
        return -1;
    }

    int existing_fd = daemon_socket_connect(socket_path);
    if (existing_fd >= 0) {
        close(existing_fd);
        close(listener_fd);
        errno = EADDRINUSE;
        return -1;
    }

    if (unlink(socket_path) != 0 && errno != ENOENT) {
        int saved_errno = errno;
        close(listener_fd);
        errno = saved_errno;
        return -1;
    }

    if (bind_listener_once(listener_fd, socket_path) == 0) {
        log_warn("Removed stale socket and rebound %s", socket_path);
        return listener_fd;
    }

    int saved_errno = errno;
    close(listener_fd);
    errno = saved_errno;
    return -1;
}

int daemon_socket_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }

    return 0;
}

int daemon_socket_accept_opcode(int listener_fd, uint8_t *opcode_out) {
    if (listener_fd < 0 || !opcode_out) {
        errno = EINVAL;
        return -1;
    }

    int client_fd = accept(listener_fd, NULL, NULL);
    if (client_fd < 0) {
        return -1;
    }

    uint8_t opcode = 0;
    ssize_t bytes = recv(client_fd, &opcode, sizeof(opcode), 0);
    int saved_errno = errno;
    close(client_fd);

    if (bytes != (ssize_t)sizeof(opcode)) {
        errno = (bytes < 0) ? saved_errno : EPROTO;
        return -1;
    }

    if (!daemon_socket_is_valid_opcode(opcode)) {
        errno = EPROTO;
        return -1;
    }

    *opcode_out = opcode;
    return 0;
}
