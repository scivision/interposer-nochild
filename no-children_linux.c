#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <sys/types.h>
#include <spawn.h>

#include "log_blocked.h"



/* --- posix_spawn --- */
int posix_spawn(pid_t *pid, const char *path,
                const posix_spawn_file_actions_t *fa,
                const posix_spawnattr_t *attrp,
                char *const argv[], char *const envp[]) {
    log_blocked("posix_spawn", path);
    errno = EACCES;
    return -1;
}
