#include <seccomp.h>
#include <unistd.h> // for execvp
#include <stdio.h>  // for fprintf, perror
#include <stdlib.h>
#include <errno.h> // for EPERM

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
    if (ctx == NULL) {
        perror("seccomp_init");
        return EXIT_FAILURE;
    }

    // Block create new processes
    seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(execve), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(execveat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(fork), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(vfork), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(clone), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(clone3), 0);

    if (seccomp_load(ctx) < 0) {
        perror("seccomp_load");
        return EXIT_FAILURE;
    }

    int i = execvp(argv[1], &argv[1]);
    if (i < 0) {
        perror("execvp");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
