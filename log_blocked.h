#include <sys/types.h>

void log_blocked(const char *, const char *);

pid_t fork(void);
pid_t vfork(void);

int execve(const char *, char *const*, char *const*);
int execv(const char *, char *const*);
int execvp(const char *, char *const*);
int execvpe(const char *, char *const*, char *const*);
