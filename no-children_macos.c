#include <spawn.h>
#include <sys/types.h>
#include <errno.h>

#include "log_blocked.h"


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
