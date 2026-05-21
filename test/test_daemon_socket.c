#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../src/daemon_socket.h"

static int pass = 0;
static int fail = 0;

#define ASSERT_TRUE(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while (0)

static void build_socket_path(char *out, size_t out_size, const char *name) {
    snprintf(out, out_size, "/tmp/cofi-test-%d-%s.sock", getpid(), name);
}

static void cleanup_socket_path(const char *path) {
    unlink(path);
}

static int send_raw_byte(const char *socket_path, unsigned char value) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    ssize_t sent = send(fd, &value, 1, 0);
    close(fd);
    return sent == 1 ? 0 : -1;
}

static void test_opcode_validation(void) {
    ASSERT_TRUE("opcode windows valid", daemon_socket_is_valid_opcode(COFI_OPCODE_WINDOWS));
    ASSERT_TRUE("opcode applications valid", daemon_socket_is_valid_opcode(COFI_OPCODE_APPLICATIONS));
    ASSERT_TRUE("opcode show-tab valid", daemon_socket_is_valid_opcode(COFI_OPCODE_SHOW_TAB));
    ASSERT_TRUE("opcode 0 reserved invalid", !daemon_socket_is_valid_opcode(COFI_OPCODE_RESERVED));
    ASSERT_TRUE("opcode 9 invalid", !daemon_socket_is_valid_opcode(9));
}

static void test_socket_path_derivation(void) {
    char path[COFI_SOCKET_PATH_MAX] = {0};

    setenv("XDG_RUNTIME_DIR", "/tmp/cofi-runtime-test", 1);
    ASSERT_TRUE("path from XDG_RUNTIME_DIR", daemon_socket_get_path(path, sizeof(path)) == 0 &&
                strcmp(path, "/tmp/cofi-runtime-test/cofi.sock") == 0);

    unsetenv("XDG_RUNTIME_DIR");
    ASSERT_TRUE("path falls back to /tmp", daemon_socket_get_path(path, sizeof(path)) == 0 &&
                strcmp(path, "/tmp/cofi.sock") == 0);
}

static void test_stale_socket_cleanup(void) {
    char path[COFI_SOCKET_PATH_MAX] = {0};
    build_socket_path(path, sizeof(path), "stale");

    int stale_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_TRUE("create stale socket fd", stale_fd >= 0);

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    ASSERT_TRUE("bind stale socket", bind(stale_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    ASSERT_TRUE("listen stale socket", listen(stale_fd, 1) == 0);

    close(stale_fd);

    int listener_fd = daemon_socket_bind_listener(path);
    ASSERT_TRUE("bind listener succeeds after stale cleanup", listener_fd >= 0);

    if (listener_fd >= 0) {
        close(listener_fd);
    }
    cleanup_socket_path(path);
}

static void test_accept_rejects_reserved_opcode(void) {
    char path[COFI_SOCKET_PATH_MAX] = {0};
    build_socket_path(path, sizeof(path), "reserved");

    int listener_fd = daemon_socket_bind_listener(path);
    ASSERT_TRUE("listener bind for reserved opcode test", listener_fd >= 0);
    if (listener_fd < 0) {
        cleanup_socket_path(path);
        return;
    }

    ASSERT_TRUE("send raw reserved opcode", send_raw_byte(path, 0) == 0);

    uint8_t opcode = 0;
    int rc = daemon_socket_accept_opcode(listener_fd, &opcode);
    ASSERT_TRUE("accept rejects reserved opcode", rc != 0 && errno == EPROTO);

    close(listener_fd);
    cleanup_socket_path(path);
}

static void test_socket_level_delivery_harness(void) {
    char path[COFI_SOCKET_PATH_MAX] = {0};
    build_socket_path(path, sizeof(path), "harness");

    int listener_fd = daemon_socket_bind_listener(path);
    ASSERT_TRUE("listener bind for harness", listener_fd >= 0);
    if (listener_fd < 0) {
        cleanup_socket_path(path);
        return;
    }

    pid_t pid = fork();
    ASSERT_TRUE("fork harness process", pid >= 0);
    if (pid == 0) {
        int opcodes[] = {
            COFI_OPCODE_WINDOWS,
            COFI_OPCODE_WORKSPACES,
            COFI_OPCODE_HARPOON,
            COFI_OPCODE_MATCHING,
            COFI_OPCODE_COMMAND,
            COFI_OPCODE_RUN,
            COFI_OPCODE_APPLICATIONS
        };

        for (size_t i = 0; i < sizeof(opcodes) / sizeof(opcodes[0]); i++) {
            if (daemon_socket_send_opcode_to_path(path, (uint8_t)opcodes[i]) != 0) {
                _exit(1);
            }
        }
        _exit(0);
    }

    int expected[] = {
        COFI_OPCODE_WINDOWS,
        COFI_OPCODE_WORKSPACES,
        COFI_OPCODE_HARPOON,
        COFI_OPCODE_MATCHING,
        COFI_OPCODE_COMMAND,
        COFI_OPCODE_RUN,
        COFI_OPCODE_APPLICATIONS
    };

    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        uint8_t received = 0;
        int rc = daemon_socket_accept_opcode(listener_fd, &received);
        char name[96];
        snprintf(name, sizeof(name), "harness received opcode[%zu]", i);
        ASSERT_TRUE(name, rc == 0 && received == (uint8_t)expected[i]);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    ASSERT_TRUE("harness child exits successfully", WIFEXITED(status) && WEXITSTATUS(status) == 0);

    close(listener_fd);
    cleanup_socket_path(path);
}

static void test_show_tab_name_roundtrip(void) {
    char path[COFI_SOCKET_PATH_MAX] = {0};
    build_socket_path(path, sizeof(path), "show-tab");

    int listener_fd = daemon_socket_bind_listener(path);
    ASSERT_TRUE("listener bind for show-tab roundtrip", listener_fd >= 0);
    if (listener_fd < 0) {
        cleanup_socket_path(path);
        return;
    }

    int sender_fd = daemon_socket_connect(path);
    ASSERT_TRUE("connect sender for show-tab", sender_fd >= 0);
    if (sender_fd < 0) {
        close(listener_fd);
        cleanup_socket_path(path);
        return;
    }

    ASSERT_TRUE("send show-tab payload succeeds",
                daemon_socket_send_tab_name(sender_fd, "emoji") == 0);
    close(sender_fd);

    uint8_t opcode = 0;
    ASSERT_TRUE("accept show-tab opcode", daemon_socket_accept_opcode(listener_fd, &opcode) == 0);
    ASSERT_TRUE("accepted opcode is show-tab", opcode == COFI_OPCODE_SHOW_TAB);

    char tab_name[64] = {0};
    ASSERT_TRUE("accept show-tab name succeeds",
                daemon_socket_accept_tab_name(listener_fd, tab_name, sizeof(tab_name)) == 0);
    ASSERT_TRUE("accepted show-tab name matches", strcmp(tab_name, "emoji") == 0);

    close(listener_fd);
    cleanup_socket_path(path);
}

int main(void) {
    test_opcode_validation();
    test_socket_path_derivation();
    test_stale_socket_cleanup();
    test_accept_rejects_reserved_opcode();
    test_socket_level_delivery_harness();
    test_show_tab_name_roundtrip();

    printf("\nResults: %d/%d tests passed\n", pass, pass + fail);
    return fail == 0 ? 0 : 1;
}
