#ifdef __linux__
#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <spawn.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

typedef int (*real_posix_spawn_fn)(pid_t *, const char *, const posix_spawn_file_actions_t *,
                                   const posix_spawnattr_t *, char *const[], char *const[]);
typedef int (*real_posix_spawnp_fn)(pid_t *, const char *, const posix_spawn_file_actions_t *,
                                    const posix_spawnattr_t *, char *const[], char *const[]);
typedef pid_t (*real_fork_fn)(void);
typedef int (*real_execve_fn)(const char *, char *const[], char *const[]);
typedef int (*real_execv_fn)(const char *, char *const[]);
typedef int (*real_execvp_fn)(const char *, char *const[]);
#ifdef __linux__
typedef int (*real_execvpe_fn)(const char *, char *const[], char *const[]);
#endif

static const char *allowed_execs[] = {
    "ninja", "ninja-build", "make", "gmake", "cmake",
    "cc", "gcc", "clang", "c++", "g++", "clang++",
    "ld", "ar", "ranlib", "as",
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
#ifdef __APPLE__
    char path[PATH_MAX];
    uint32_t size = (uint32_t)sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) return 0;
    return is_whitelisted_exec(path);
#else
    char path[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n <= 0) return 0;
    path[n] = '\0';
    return is_whitelisted_exec(path);
#endif
}


void log_blocked(const char *func, const char *cmd) {
    static int count = 0;
    if (++count > 200) return;
    fprintf(stderr, "[NOCHILD %3d] Blocked %-12s → %s\n",
            count, func, cmd ? cmd : "(null)");
}

static int nochild_posix_spawn(pid_t *pid,
                               const char *path,
                               const posix_spawn_file_actions_t *file_actions,
                               const posix_spawnattr_t *attrp,
                               char *const argv[],
                               char *const envp[]) {
    if (is_whitelisted_exec(path)) {
        real_posix_spawn_fn real_fn = (real_posix_spawn_fn)dlsym(RTLD_NEXT, "posix_spawn");
        return real_fn(pid, path, file_actions, attrp, argv, envp);
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
        real_posix_spawnp_fn real_fn = (real_posix_spawnp_fn)dlsym(RTLD_NEXT, "posix_spawnp");
        return real_fn(pid, file, file_actions, attrp, argv, envp);
    }

    log_blocked("posix_spawnp", file);
    errno = EACCES;
    return EACCES;
}

static pid_t nochild_fork(void) {
    if (current_process_whitelisted()) {
        real_fork_fn real_fn = (real_fork_fn)dlsym(RTLD_NEXT, "fork");
        return real_fn();
    }

    log_blocked("fork", NULL);
    errno = EACCES;
    return -1;
}

static pid_t nochild_vfork(void) {
    if (current_process_whitelisted()) {
        real_fork_fn real_fn = (real_fork_fn)dlsym(RTLD_NEXT, "vfork");
        return real_fn();
    }

    log_blocked("vfork", NULL);
    errno = EACCES;
    return -1;
}

static int nochild_execve(const char *path, char *const argv[], char *const envp[]) {
    if (is_whitelisted_exec(path)) {
        real_execve_fn real_fn = (real_execve_fn)dlsym(RTLD_NEXT, "execve");
        return real_fn(path, argv, envp);
    }

    log_blocked("execve", path);
    errno = EACCES;
    return -1;
}

static int nochild_execv(const char *path, char *const argv[]) {
    if (is_whitelisted_exec(path)) {
        real_execv_fn real_fn = (real_execv_fn)dlsym(RTLD_NEXT, "execv");
        return real_fn(path, argv);
    }

    log_blocked("execv", path);
    errno = EACCES;
    return -1;
}

static int nochild_execvp(const char *file, char *const argv[]) {
    if (is_whitelisted_exec(file)) {
        real_execvp_fn real_fn = (real_execvp_fn)dlsym(RTLD_NEXT, "execvp");
        return real_fn(file, argv);
    }

    log_blocked("execvp", file);
    errno = EACCES;
    return -1;
}

#ifdef __linux__
static int nochild_execvpe(const char *file, char *const argv[], char *const envp[]) {
    if (is_whitelisted_exec(file)) {
        real_execvpe_fn real_fn = (real_execvpe_fn)dlsym(RTLD_NEXT, "execvpe");
        return real_fn(file, argv, envp);
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
DYLD_INTERPOSE(nochild_vfork, vfork)
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
