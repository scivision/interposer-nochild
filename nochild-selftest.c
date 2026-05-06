#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern char **environ;

static int expect_blocked_fork(void) {
    errno = 0;
    pid_t p = fork();
    if (p == -1 && errno == EACCES) {
        fprintf(stderr, "[selftest] fork blocked as expected (EACCES)\n");
        return 0;
    }

    if (p == 0) {
        _exit(0);
    }
    if (p > 0) {
        int st = 0;
        (void)waitpid(p, &st, 0);
    }

    fprintf(stderr,
            "[selftest] FAIL: fork was not blocked by interposer (pid=%ld errno=%d: %s)\n",
            (long)p, errno, strerror(errno));
    return 1;
}

static int expect_blocked_spawnp(void) {
    pid_t p = 0;
    char *argv[] = {(char *)"echo", (char *)"ok", NULL};

    errno = 0;
    int rc = posix_spawnp(&p, "echo", NULL, NULL, argv, environ);
    if (rc == EACCES) {
        fprintf(stderr, "[selftest] posix_spawnp blocked as expected (EACCES)\n");
        return 0;
    }

    if (rc == 0) {
        int st = 0;
        (void)waitpid(p, &st, 0);
    }

    fprintf(stderr,
            "[selftest] FAIL: posix_spawnp was not blocked by interposer (rc=%d pid=%ld errno=%d: %s)\n",
            rc, (long)p, errno, strerror(errno));
    return 1;
}

int main(void) {
    int failures = 0;

    failures += expect_blocked_fork();
    failures += expect_blocked_spawnp();

    if (failures == 0) {
        fprintf(stderr,
                "[selftest] PASS: interposer is active and child launch is blocked.\n");
        return EXIT_SUCCESS;
    }

    fprintf(stderr,
            "[selftest] FAIL: interposer not active for this process. On macOS this can indicate SIP or invocation context prevented DYLD interposition.\n");
    return EXIT_FAILURE;
}
