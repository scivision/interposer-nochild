#include "log_blocked.h"

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>


void log_blocked(const char *func, const char *cmd) {
    static int count = 0;
    if (++count > 200) return;
    fprintf(stderr, "[NOCHILD %3d] Blocked %-12s → %s\n",
            count, func, cmd ? cmd : "(null)");
}

pid_t fork(void) {
    log_blocked("fork", NULL);
    errno = EACCES;
    return -1;
}

pid_t vfork(void) {
    log_blocked("vfork", NULL);
    errno = EACCES;
    return -1;
}

int execve(const char *path, char *const argv[], char *const envp[]) {
    log_blocked("execve", path);
    errno = EACCES;
    return -1;
}

int execv(const char *path, char *const argv[]) {
    log_blocked("execv", path);
    errno = EACCES;
    return -1;
}

int execvp(const char *file, char *const argv[]) {
    log_blocked("execvp", file);
    errno = EACCES;
    return -1;
}

int execvpe(const char *file, char *const argv[], char *const envp[]) {
    log_blocked("execvpe", file);
    errno = EACCES;
    return -1;
}
