#include <spawn.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

static void log_blocked(const char *func, const char *cmd) {
    static int count = 0;
    if (++count > 200) return;   // safety against spam
    fprintf(stderr, "[NOCHILD %3d] Blocked %-12s → %s\n",
            count, func, cmd ? cmd : "(null)");
}

// === Simple blocking versions ===
int fork(void)                  { log_blocked("fork", NULL); errno = EACCES; return -1; }
int vfork(void)                 { log_blocked("vfork", NULL); errno = EACCES; return -1; }

int posix_spawn(pid_t * __restrict pid,
                const char * __restrict path,
                const posix_spawn_file_actions_t *file_actions,
                const posix_spawnattr_t * __restrict attrp,
                char *const argv[ __restrict],
                char *const envp[ __restrict]) {
    log_blocked("posix_spawn", path);
    errno = EACCES;
    return -1;
}

// exec family
int execve(const char *path, char *const argv[], char *const envp[]) {
    log_blocked("execve", path);
    errno = EACCES; return -1;
}

int execv(const char *path, char *const argv[]) {
    log_blocked("execv", path);
    errno = EACCES; return -1;
}

int execvp(const char *file, char *const argv[]) {
    log_blocked("execvp", file);
    errno = EACCES; return -1;
}

int execvpe(const char *file, char *const argv[], char *const envp[]) {
    log_blocked("execvpe", file);
    errno = EACCES; return -1;
}
