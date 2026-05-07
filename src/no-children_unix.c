#if defined(__linux__)
#define _GNU_SOURCE
#include <dlfcn.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <spawn.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>


#ifdef __linux__
typedef int (*real_posix_spawn_fn)(pid_t *, const char *, const posix_spawn_file_actions_t *,
                                   const posix_spawnattr_t *, char *const[], char *const[]);
typedef int (*real_posix_spawnp_fn)(pid_t *, const char *, const posix_spawn_file_actions_t *,
                                    const posix_spawnattr_t *, char *const[], char *const[]);
typedef pid_t (*real_fork_fn)(void);
typedef int (*real_execve_fn)(const char *, char *const[], char *const[]);
typedef int (*real_execv_fn)(const char *, char *const[]);
typedef int (*real_execvp_fn)(const char *, char *const[]);
typedef int (*real_execvpe_fn)(const char *, char *const[], char *const[]);

static real_posix_spawn_fn cached_posix_spawn = NULL;
static real_posix_spawnp_fn cached_posix_spawnp = NULL;
static real_fork_fn cached_fork = NULL;
static real_fork_fn cached_vfork = NULL;
static real_execve_fn cached_execve = NULL;
static real_execv_fn cached_execv = NULL;
static real_execvp_fn cached_execvp = NULL;
static real_execvpe_fn cached_execvpe = NULL;

/* A dup of the original stderr saved at load time, so log messages reach the
 * terminal even when the child process has redirected fd 2 (e.g. CMake sets
 * up an ERROR_QUIET pipe before fork()). FD_CLOEXEC keeps it from leaking
 * into successfully exec'd children. */
static int nochild_log_fd = STDERR_FILENO;

__attribute__((constructor))
static void nochild_init(void) {
    cached_posix_spawn  = (real_posix_spawn_fn) dlsym(RTLD_NEXT, "posix_spawn");
    cached_posix_spawnp = (real_posix_spawnp_fn)dlsym(RTLD_NEXT, "posix_spawnp");
    cached_fork         = (real_fork_fn)         dlsym(RTLD_NEXT, "fork");
    cached_vfork        = (real_fork_fn)         dlsym(RTLD_NEXT, "vfork");
    cached_execve       = (real_execve_fn)       dlsym(RTLD_NEXT, "execve");
    cached_execv        = (real_execv_fn)        dlsym(RTLD_NEXT, "execv");
    cached_execvp       = (real_execvp_fn)       dlsym(RTLD_NEXT, "execvp");
    cached_execvpe      = (real_execvpe_fn)      dlsym(RTLD_NEXT, "execvpe");

    int fd = dup(STDERR_FILENO);
    if (fd >= 0) {
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        nochild_log_fd = fd;
    }
}

static real_posix_spawn_fn  get_real_posix_spawn(void)  { return cached_posix_spawn; }
static real_posix_spawnp_fn get_real_posix_spawnp(void) { return cached_posix_spawnp; }
static real_fork_fn         get_real_fork(void)         { return cached_fork; }
static real_fork_fn         get_real_vfork(void)        { return cached_vfork; }
static real_execve_fn       get_real_execve(void)       { return cached_execve; }
static real_execv_fn        get_real_execv(void)        { return cached_execv; }
static real_execvp_fn       get_real_execvp(void)       { return cached_execvp; }
static real_execvpe_fn      get_real_execvpe(void)      { return cached_execvpe; }
#endif /* __linux__ */

static const char *allowed_execs[] = {
    "ninja", "ninja-build", "make", "gmake", "cmake",
    "uname",
    "xcrun", "xcode-select", "sw_vers", "sysctl", "arch",
    "cc", "gcc", "clang", "c++", "g++", "clang++",
    "cc1", "cc1plus", "collect2",
    "ld", "ar", "ranlib", "as",
    // "sh", "bash", "dash",
    NULL
};

static void to_lower_ascii(char *s) {
    if (!s) return;
    for (; *s; ++s) {
        *s = (char)tolower((unsigned char)*s);
    }
}

static const char *base_name(const char *path) {
    const char *base = path;
    if (!path) return NULL;
    for (const char *p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

static int is_whitelisted_exec(const char *path_or_file) {
    if (!path_or_file || !*path_or_file) return 0;

    char name[256];
    const char *base = base_name(path_or_file);
    size_t n = strlen(base);
    if (n == 0 || n >= sizeof(name)) return 0;

    memcpy(name, base, n + 1);
    to_lower_ascii(name);

    for (int i = 0; allowed_execs[i] != NULL; ++i) {
        if (strcmp(name, allowed_execs[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int current_process_whitelisted(void) {
    static int cached = -1;
    if (cached >= 0) return cached;

#ifdef __APPLE__
    char path[PATH_MAX];
    uint32_t size = (uint32_t)sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) {
        cached = 0;
        return cached;
    }
    cached = is_whitelisted_exec(path);
#else
#ifdef __GLIBC__
    if (program_invocation_short_name && *program_invocation_short_name) {
        cached = is_whitelisted_exec(program_invocation_short_name);
        if (cached) {
            return cached;
        }
    }
#endif

    char path[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n <= 0) {
        cached = 0;
        return cached;
    }
    path[n] = '\0';
    cached = is_whitelisted_exec(path);
#endif
    return cached;
}


void log_blocked(const char *func, const char *cmd) {
    static int count = 0;
    static int initialized = 0;
    static int log_max = 200;
    static int log_every = 1000;

    if (!initialized) {
        const char *max_env = getenv("NOCHILD_LOG_MAX");
        const char *every_env = getenv("NOCHILD_LOG_EVERY");
        if (max_env && *max_env) {
            log_max = atoi(max_env);
        }
        if (every_env && *every_env) {
            log_every = atoi(every_env);
        }
        initialized = 1;
    }

    ++count;
    if (log_max >= 0 && count > log_max) {
        if (log_every <= 0 || (count % log_every) != 0) {
            return;
        }
    }

    dprintf(nochild_log_fd, "[NOCHILD %3d] Blocked %-12s \u2192 %s\n",
            count, func, cmd ? cmd : "(null)");
}

static int nochild_posix_spawn(pid_t *pid,
                               const char *path,
                               const posix_spawn_file_actions_t *file_actions,
                               const posix_spawnattr_t *attrp,
                               char *const argv[],
                               char *const envp[]) {
    if (is_whitelisted_exec(path)) {
#ifdef __APPLE__
        return posix_spawn(pid, path, file_actions, attrp, argv, envp);
#else
        real_posix_spawn_fn real_fn = get_real_posix_spawn();
        if (real_fn) return real_fn(pid, path, file_actions, attrp, argv, envp);
        errno = ENOSYS; return ENOSYS;
#endif
    }

    log_blocked("posix_spawn", path);
    errno = EACCES;
    return EACCES;
}

static int nochild_posix_spawnp(pid_t *pid,
                                const char *file,
                                const posix_spawn_file_actions_t *file_actions,
                                const posix_spawnattr_t *attrp,
                                char *const argv[],
                                char *const envp[]) {
    if (is_whitelisted_exec(file)) {
#ifdef __APPLE__
        return posix_spawnp(pid, file, file_actions, attrp, argv, envp);
#else
        real_posix_spawnp_fn real_fn = get_real_posix_spawnp();
        if (real_fn) return real_fn(pid, file, file_actions, attrp, argv, envp);
        errno = ENOSYS; return ENOSYS;
#endif
    }

    log_blocked("posix_spawnp", file);
    errno = EACCES;
    return EACCES;
}

static pid_t nochild_fork(void) {
    if (current_process_whitelisted()) {
#ifdef __APPLE__
        return fork();
#else
        real_fork_fn real_fn = get_real_fork();
        if (real_fn) return real_fn();
        errno = ENOSYS; return -1;
#endif
    }

    log_blocked("fork", NULL);
    errno = EACCES;
    return -1;
}

static pid_t nochild_vfork(void) {
    if (current_process_whitelisted()) {
#ifdef __APPLE__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        return vfork();
#pragma GCC diagnostic pop
#else
        real_fork_fn real_fn = get_real_vfork();
        if (real_fn) return real_fn();
        errno = ENOSYS; return -1;
#endif
    }

    log_blocked("vfork", NULL);
    errno = EACCES;
    return -1;
}

static int nochild_execve(const char *path, char *const argv[], char *const envp[]) {
    if (is_whitelisted_exec(path)) {
#ifdef __APPLE__
        return execve(path, argv, envp);
#else
        real_execve_fn real_fn = get_real_execve();
        if (real_fn) return real_fn(path, argv, envp);
        errno = ENOSYS; return -1;
#endif
    }

    log_blocked("execve", path);
    errno = EACCES;
    return -1;
}

static int nochild_execv(const char *path, char *const argv[]) {
    if (is_whitelisted_exec(path)) {
#ifdef __APPLE__
        return execv(path, argv);
#else
        real_execv_fn real_fn = get_real_execv();
        if (real_fn) return real_fn(path, argv);
        errno = ENOSYS; return -1;
#endif
    }

    log_blocked("execv", path);
    errno = EACCES;
    return -1;
}

static int nochild_execvp(const char *file, char *const argv[]) {
    if (is_whitelisted_exec(file)) {
#ifdef __APPLE__
        return execvp(file, argv);
#else
        real_execvp_fn real_fn = get_real_execvp();
        if (real_fn) return real_fn(file, argv);
        errno = ENOSYS; return -1;
#endif
    }

    log_blocked("execvp", file);
    errno = EACCES;
    return -1;
}

#ifdef __linux__
static int nochild_execvpe(const char *file, char *const argv[], char *const envp[]) {
    if (is_whitelisted_exec(file)) {
        real_execvpe_fn real_fn = get_real_execvpe();
        if (real_fn) {
            return real_fn(file, argv, envp);
        }
        errno = ENOSYS;
        return -1;
    }

    log_blocked("execvpe", file);
    errno = EACCES;
    return -1;
}
#endif

#ifdef __APPLE__
#define DYLD_INTERPOSE(_replacement, _replacee) \
    __attribute__((used)) static struct { \
        const void *replacement; \
        const void *replacee; \
    } _interpose_##_replacee \
    __attribute__((section("__DATA,__interpose"))) = { \
        (const void *)(unsigned long)&_replacement, \
        (const void *)(unsigned long)&_replacee \
    };

DYLD_INTERPOSE(nochild_posix_spawn, posix_spawn)
DYLD_INTERPOSE(nochild_posix_spawnp, posix_spawnp)
DYLD_INTERPOSE(nochild_fork, fork)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
DYLD_INTERPOSE(nochild_vfork, vfork)
#pragma GCC diagnostic pop
DYLD_INTERPOSE(nochild_execve, execve)
DYLD_INTERPOSE(nochild_execv, execv)
DYLD_INTERPOSE(nochild_execvp, execvp)
#ifdef __linux__
DYLD_INTERPOSE(nochild_execvpe, execvpe)
#endif
#else
int posix_spawn(pid_t *pid,
                const char *path,
                const posix_spawn_file_actions_t *file_actions,
                const posix_spawnattr_t *attrp,
                char *const argv[],
                char *const envp[]) {
    return nochild_posix_spawn(pid, path, file_actions, attrp, argv, envp);
}

int posix_spawnp(pid_t *pid,
                 const char *file,
                 const posix_spawn_file_actions_t *file_actions,
                 const posix_spawnattr_t *attrp,
                 char *const argv[],
                 char *const envp[]) {
    return nochild_posix_spawnp(pid, file, file_actions, attrp, argv, envp);
}

pid_t fork(void) {
    return nochild_fork();
}

pid_t vfork(void) {
    return nochild_vfork();
}

int execve(const char *path, char *const argv[], char *const envp[]) {
    return nochild_execve(path, argv, envp);
}

int execv(const char *path, char *const argv[]) {
    return nochild_execv(path, argv);
}

int execvp(const char *file, char *const argv[]) {
    return nochild_execvp(file, argv);
}

#ifdef __linux__
int execvpe(const char *file, char *const argv[], char *const envp[]) {
    return nochild_execvpe(file, argv, envp);
}
#endif
#endif
