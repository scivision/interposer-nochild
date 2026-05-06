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

    // Block process creation syscalls. CMake's execute_process() relies on
    // fork/clone to spawn children — blocking these prevents child processes.
    // We do NOT block execve/execveat: our execvp below uses execve to replace
    // the current process with CMake (no fork), and CMake inherits this filter.
    seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(fork), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(vfork), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(clone), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(clone3), 0);

    if (seccomp_load(ctx) < 0) {
        perror("seccomp_load");
        seccomp_release(ctx);
        return EXIT_FAILURE;
    }

    // since seccomp_load has pushed the filter into the kernel, we can release the user-space context
    seccomp_release(ctx);

    // Now launch CMake — this execve is still allowed
    execvp(argv[1], &argv[1]);

    perror("execvp");
    return EXIT_FAILURE;
}
