// test/test_detach_survival_bin.c
// Directly tests fork+setsid+double-fork behavior.
// Usage: test_detach_survival_bin <sentinel_file>
// Writes the grandchild PID to sentinel_file, then execs "sleep 30".
// The sentinel process is in a separate process group (setsid was called).

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <sentinel_file>\n", argv[0]);
        return 1;
    }
    const char *sentinel = argv[1];

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid > 0) {
        // parent: wait for intermediate child then exit
        int status;
        waitpid(pid, &status, 0);
        return 0;
    }

    // intermediate child: create new session
    setsid();

    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, 0);
        dup2(devnull, 1);
        dup2(devnull, 2);
        if (devnull > 2) close(devnull);
    }

    // double-fork so grandchild is not a session leader
    pid_t gpid = fork();
    if (gpid < 0) _exit(1);
    if (gpid > 0) _exit(0);  // intermediate exits; grandchild continues

    // grandchild: write our PID to sentinel, then exec sleep
    FILE *f = fopen(sentinel, "w");
    if (f) { fprintf(f, "%d\n", getpid()); fclose(f); }
    execlp("sleep", "sleep", "30", NULL);
    _exit(127);
}
