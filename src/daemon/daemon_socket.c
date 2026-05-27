#include "daemon/daemon_socket.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "core/log/log.h"

#define COFI_SHOW_TAB_NAME_MAX 63

static int s_show_tab_client_fd = -1;

int daemon_socket_is_valid_opcode(uint8_t opcode) {
    return opcode >= COFI_OPCODE_WINDOWS && opcode <= COFI_OPCODE_SHOW_TAB;
}

const char *daemon_socket_opcode_name(uint8_t opcode) {
    switch (opcode) {
        case COFI_OPCODE_WINDOWS:
            return "windows";
        case COFI_OPCODE_WORKSPACES:
            return "workspaces";
        case COFI_OPCODE_HARPOON:
            return "harpoon";
        case COFI_OPCODE_MATCHING:
            return "matching";
        case COFI_OPCODE_COMMAND:
            return "command";
        case COFI_OPCODE_RUN:
            return "run";
        case COFI_OPCODE_APPLICATIONS:
            return "applications";
        case COFI_OPCODE_SHOW_TAB:
            return "show_tab";
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

int daemon_socket_send_tab_name(int socket_fd, const char *name) {
    if (socket_fd < 0 || !name) {
        errno = EINVAL;
        return -1;
    }

    size_t name_len = strnlen(name, COFI_SHOW_TAB_NAME_MAX + 1);
    if (name_len == 0 || name_len > COFI_SHOW_TAB_NAME_MAX) {
        errno = EINVAL;
        return -1;
    }

    uint8_t header[2] = {COFI_OPCODE_SHOW_TAB, (uint8_t)name_len};
    ssize_t sent = send(socket_fd, header, sizeof(header), 0);
    if (sent != (ssize_t)sizeof(header)) {
        if (sent >= 0) errno = EIO;
        return -1;
    }

    sent = send(socket_fd, name, name_len, 0);
    if (sent != (ssize_t)name_len) {
        if (sent >= 0) errno = EIO;
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

    if (bytes != (ssize_t)sizeof(opcode)) {
        close(client_fd);
        errno = (bytes < 0) ? saved_errno : EPROTO;
        return -1;
    }

    if (!daemon_socket_is_valid_opcode(opcode)) {
        close(client_fd);
        errno = EPROTO;
        return -1;
    }

    if (opcode == COFI_OPCODE_SHOW_TAB) {
        if (s_show_tab_client_fd >= 0) {
            close(s_show_tab_client_fd);
        }
        s_show_tab_client_fd = client_fd;
    } else {
        close(client_fd);
    }

    *opcode_out = opcode;
    return 0;
}

int daemon_socket_accept_tab_name(int listener_fd, char *out, size_t max) {
    (void)listener_fd;
    if (!out || max == 0 || s_show_tab_client_fd < 0) {
        errno = EINVAL;
        return -1;
    }

    uint8_t len = 0;
    ssize_t bytes = recv(s_show_tab_client_fd, &len, sizeof(len), 0);
    if (bytes != (ssize_t)sizeof(len) || len == 0 || len > COFI_SHOW_TAB_NAME_MAX ||
        (size_t)len >= max) {
        close(s_show_tab_client_fd);
        s_show_tab_client_fd = -1;
        errno = EPROTO;
        return -1;
    }

    bytes = recv(s_show_tab_client_fd, out, len, 0);
    close(s_show_tab_client_fd);
    s_show_tab_client_fd = -1;
    if (bytes != (ssize_t)len) {
        errno = EPROTO;
        return -1;
    }

    out[len] = '\0';
    return 0;
}
